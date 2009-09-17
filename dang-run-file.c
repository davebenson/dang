#include <stdio.h>
#include <string.h>
#include "dang.h"

static DangExpr *replace_name (DangExpr *in, const char *new_name)
{
  DangExpr *rv = dang_expr_new_function (new_name, in->function.n_args, in->function.args);
  dang_code_position_copy (&rv->any.code_position, &in->any.code_position);
  dang_expr_unref (in);
  return rv;
}

DangExpr *dang_do_toplevel_transforms (DangExpr   *in,
                                       DangError **error)
{
  /* $var_decl(A) ==> not allowed 
     $assign($var_decl(A), X) ==> $define_global_infer_type(A, X)
     $var_decl(TYPE, A) ==> $define_global(TYPE, A)
   */
  if (in->type != DANG_EXPR_TYPE_FUNCTION)
    return in;
  if (dang_expr_is_function (in, "$var_decl"))
    {
      if (in->function.n_args == 1)
        {
          dang_set_error (error, "untyped globals are not allowed ("DANG_CP_FORMAT")",
                          DANG_CP_ARGS(in->any.code_position));
          return NULL;
        }
      dang_assert (in->function.n_args == 2);
      return replace_name (in, "$define_global");
    }
  else if (dang_expr_is_function (in, "$assign")
        && in->function.n_args == 2
        && in->function.args[0]->type == DANG_EXPR_TYPE_FUNCTION
        && strcmp (in->function.args[0]->function.name, "$var_decl") == 0)
    {
      DangExpr *vd_expr = in->function.args[0];
      if (vd_expr->function.n_args == 1)
        {
          /* Convert to $define_global_infer_type */
          DangExpr *args[2] = { vd_expr->function.args[0],
                                in->function.args[1] };
          DangExpr *rv = dang_expr_new_function ("$define_global_infer_type",
                                                 2, args);
          const char *name;
          dang_assert (vd_expr->function.args[0]->type == DANG_EXPR_TYPE_BAREWORD);
          name = vd_expr->function.args[0]->bareword.name;
          dang_code_position_copy (&rv->any.code_position,
                                   &in->any.code_position);
          dang_expr_unref (in);
          return rv;
        }
      else
        {
          /* convert to $assign(define_global(TYPE,X), VALUE) */
          DangExpr *rv;
          DangExpr *args[2];
          vd_expr = replace_name (dang_expr_ref (vd_expr), "$define_global");
          args[0] = vd_expr;
          args[1] = in->function.args[1];
          rv = dang_expr_new_function ("$assign", 2, args);
          dang_code_position_copy (&rv->any.code_position,
                                   &in->any.code_position);
          dang_expr_unref (vd_expr);
          dang_expr_unref (in);
          return rv;
        }
    }

  return in;
}

static char **
scan_bareword_array (DangExpr *expr,
                     unsigned *n_names_out)
{
  char **rv;
  unsigned i;
  dang_assert (expr->type == DANG_EXPR_TYPE_FUNCTION);
  rv = dang_new (char *, expr->function.n_args);
  for (i = 0; i < expr->function.n_args; i++)
    {
      dang_assert (expr->function.args[i]->type == DANG_EXPR_TYPE_BAREWORD);
      rv[i] = expr->function.args[i]->bareword.name;
    }
  *n_names_out = expr->function.n_args;
  return rv;
}

static dang_boolean
handle_parser_expressions (DangParser *parser,
                           DangRunFileOptions *options,
                           DangError **error)
{
  DangExpr *expr;
  while ((expr=dang_parser_pop_expr (parser)) != NULL)
    {
      DangCompileContext *cc;
      DangFunction *function;
#if DANG_DEBUG
      if (dang_debug_parse)
        {
          fprintf (stderr, "got expression:\n");
          dang_debug_dump_expr (expr);
        }
#endif

      if (options->is_module)
        {
          /* MUST be matching $module() directive */
          if (!dang_expr_is_function (expr, "$module"))
            {
              dang_set_error (error, "module must begin with 'module' directive ("DANG_CP_FORMAT")",
                              DANG_CP_EXPR_ARGS(expr));
              return FALSE;
            }
          options->is_module = FALSE;
          /* Fall-through to normal $module() case */
        }

      /* Handle module/imports manipulating "metafunctions";
         they are not really true metafunctions, because
         normal metafunctions cannot change the "imports" settings. */
      if (dang_expr_is_function (expr, "$use"))
        {
          unsigned n_names;
          char **names = scan_bareword_array (expr, &n_names);
          if (!dang_module_load (expr->function.n_args, names, error))
            {
              dang_error_add_suffix (*error, "at "DANG_CP_FORMAT, DANG_CP_EXPR_ARGS (expr));
              return FALSE;
            }
          dang_expr_unref (expr);
          continue;
        }
      else if (dang_expr_is_function (expr, "$module"))
        {
          unsigned n_names;
          char **names = scan_bareword_array (expr, &n_names);
          DangImports *old_imports, *new_imports;
          DangNamespace *ns = dang_namespace_force (n_names, names, error);
          if (ns == NULL)
            {
              dang_expr_unref (expr);
              dang_free (names);
              return FALSE;
            }
          old_imports = dang_parser_peek_imports (parser);
          new_imports = dang_imports_new (old_imports->n_imported_namespaces,
                                          old_imports->imported_namespaces,
                                          ns);
          dang_parser_set_imports (parser, new_imports);
          dang_imports_unref (new_imports);
          continue;
        }
      else if (dang_expr_is_function (expr, "$import"))
        {
          /* $import(${include,exclude}(names...), $module_name(...), qualifier?) */
          DangImportedNamespace ins;
          char **mod_names;
          unsigned n_mod_names;
          dang_assert (2 <= expr->function.n_args
                      && expr->function.n_args <= 3);
          if (dang_expr_is_function (expr->function.args[0], "$include"))
            ins.reverse = FALSE;
          else if (dang_expr_is_function (expr->function.args[0], "$exclude"))
            ins.reverse = TRUE;
          else
            dang_assert_not_reached ();
          ins.names = scan_bareword_array (expr->function.args[0], &ins.n_names);
          if (expr->function.n_args == 2)
            ins.qualifier = NULL;
          else
            {
              dang_assert (expr->function.args[2]->type == DANG_EXPR_TYPE_BAREWORD);
              ins.qualifier = expr->function.args[2]->bareword.name;
            }
          mod_names = scan_bareword_array (expr->function.args[1], &n_mod_names);
          ins.ns = dang_namespace_try (n_mod_names, mod_names, error);
          if (ins.ns == NULL)
            {
              dang_free (ins.names);
              dang_free (mod_names);
              return FALSE;
            }
          continue;
        }

      if ((expr = dang_do_toplevel_transforms (expr, error)) == NULL)
        {
          dang_expr_unref (expr);
          return FALSE;
        }
      else
        function = dang_function_new_stub (dang_parser_peek_imports (parser),
                                           dang_signature_void_func (), expr,
                                           NULL, 0, NULL);
      cc = dang_compile_context_new ();
      dang_compile_context_register (cc, function);
      /* TODO: support for some yielding? */
      if (!dang_compile_context_finish (cc, error)
       || !dang_function_call_nonyielding_v (function, NULL, NULL, error))
        {
          dang_function_unref (function);
          dang_expr_unref (expr);
          dang_compile_context_free (cc);
          return FALSE;
        }
      dang_function_unref (function);
      dang_expr_unref (expr);
      dang_compile_context_free (cc);
    }
  return TRUE;
}

dang_boolean
dang_run_file (const char           *filename,
               DangRunFileOptions   *options,
               DangError           **error)
{
  FILE *input_fp;
  DangRunFileOptions opt = *options;
  DangImportedNamespace imported_ns;
  DangImports *imports;
  DangParseOptions parse_opts = DANG_PARSE_OPTIONS_INIT;
  DangParser *parser;
  DangTokenizer *tokenizer;
  DangString *fstring;
  size_t nread;
  char tmp_buf[4096];
  dang_boolean must_close_input_fp;
  if (strcmp (filename, "-") == 0)
    {
      fstring = dang_string_new ("*standard-input*");
      input_fp = stdin;
      must_close_input_fp = FALSE;
    }
  else
    {
      fstring = dang_string_new (filename);
      input_fp = fopen (filename, "r");
      if (input_fp == NULL)
        {
          dang_set_error (error, "error opening %s: %s\n",
                          filename, strerror (errno));
          return FALSE;
        }
      must_close_input_fp = TRUE;
    }

  imported_ns.n_names = 0;
  imported_ns.names = 0;
  imported_ns.reverse = TRUE;
  imported_ns.qualifier = NULL;
  imported_ns.ns = dang_namespace_default ();
  if (options->is_module)
    imports = dang_imports_new (1, &imported_ns, NULL);
  else
    imports = dang_imports_new (1, &imported_ns, dang_namespace_default ());
  parse_opts.imports = imports;
  parser = dang_parser_create (&parse_opts, error);
  dang_imports_unref (imports);
  if (parser == NULL)
    {
      if (must_close_input_fp)
        fclose (input_fp);
      return FALSE;
    }
  tokenizer = dang_tokenizer_new (fstring);
  dang_string_unref (fstring);

  while ((nread=fread (tmp_buf, 1, sizeof (tmp_buf), input_fp)) > 0)
    {
      DangToken *token;
      if (!dang_tokenizer_feed (tokenizer, nread, tmp_buf, error))
        {
          dang_parser_destroy (parser);
          dang_tokenizer_free (tokenizer);
          if (must_close_input_fp)
            fclose (input_fp);
          return FALSE;
        }
      while ((token=dang_tokenizer_pop_token (tokenizer)) != NULL)
        {
          if (!dang_parser_parse (parser, token, error))
            {
              dang_parser_destroy (parser);
              dang_tokenizer_free (tokenizer);
              if (must_close_input_fp)
                fclose (input_fp);
              return FALSE;
            }
          if (!handle_parser_expressions (parser, &opt, error))
            {
              dang_parser_destroy (parser);
              dang_tokenizer_free (tokenizer);
              if (must_close_input_fp)
                fclose (input_fp);
              return FALSE;
            }
        }
    }
  if (must_close_input_fp)
    fclose (input_fp);
  dang_tokenizer_free (tokenizer);
  if (!dang_parser_end_parse (parser, error))
    return FALSE;
  if (!handle_parser_expressions (parser, &opt, error))
    {
      dang_parser_destroy (parser);
      return FALSE;
    }
  dang_parser_destroy (parser);
  return TRUE;
}
