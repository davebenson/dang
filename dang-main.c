#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "config.h"
#ifdef HAVE_READLINE
# ifdef READLINE_HEADER_REQUIRE_PREFIX
#  include <readline/readline.h>
#  include <readline/history.h>
# else
#  include <readline.h>
#  include <history.h>
# endif
#endif
#include "dang.h"

static dang_boolean interactive = FALSE;
static dang_boolean errors_fatal = FALSE;
static dang_boolean quiet_exceptions = FALSE;

static void
usage (void)
{

  fprintf (stderr,
   "usage: dang [OPTIONS] FILENAME\n"
   "or     dang [OPTIONS] -\n"
   "\n"
   "Options:\n"
   "  --errors-fatal      Die if an uncaught exception is thrown.\n"
   "  --errors-not-fatal  Do not die if an uncaught exception is thrown.\n"
   "  -i, --interactive   Expect interactive mode.\n"
   "  --not-interactive   Not interactive mode.\n"
   "  --quiet-exceptions  Do not print exception information.\n"
   "  -I dir              Add directory to include path.\n"
   "\n"
   "See --help-debug for debugging options.\n"
  );
  exit (1);
}
static void
debug_options_usage (void)
{
#ifdef DANG_DEBUG
  fprintf (stderr,
           "Debug options:\n"
           "  --debug-disassemble        Print opcodes\n"
           "  --debug-dump-exprs         Print parsed expressions.\n"
           //"  --debug-run                Print steps as they are run.\n"
           //"  --debug-run-data           Print locals (before the step is executed).\n"
           "  --debug-all                Enable all debugging.\n"
          );
#else
  fprintf (stderr, "Debugging disabled in this build.\n");
#endif
  exit (1);
}

char *
read_string_from_file (FILE *fp)
{
  DangUtilArray array;
  int nread;
  char buf[4096];
  DANG_UTIL_ARRAY_INIT (&array, char);
  while ((nread=fread (buf, 1, sizeof (buf), fp)) > 0)
    dang_util_array_append (&array, nread, buf);
  dang_util_array_append (&array, 1, "");
  return array.data;
}

#ifndef HAVE_READLINE
static char *readline (const char *prompt)
{
  static char buf[1024];
  fputs (prompt, stdout);
  fflush (stdout);
  return fgets (buf, sizeof (buf), stdin);
}
static void add_history (const char *str)
{
}
#endif

static void
function_run (DangFunction *function)
{
  DangThread *thread = dang_thread_new (function, 0, NULL);
  dang_thread_run (thread);
  switch (thread->status)
    {
    case DANG_THREAD_STATUS_YIELDED:
      dang_warning ("unexpected yield");
      dang_thread_cancel (thread);
      break;
    case DANG_THREAD_STATUS_THREW:
      if (!quiet_exceptions)
        {
          if (thread->info.threw.type == NULL)
            dang_warning ("unhandled dataless exception");
          else if (thread->info.threw.type == dang_value_type_error ())
            {
              DangError *e = * (DangError **) thread->info.threw.value;
              if (e == NULL)
                dang_warning ("NULL error thrown");
              else if (e->backtrace)
                dang_warning ("ERROR: %s\n%s", e->message, e->backtrace);
              else
                dang_warning ("ERROR: %s", e->message);
            }
          else
            dang_warning ("unhandled exception of type %s",
                          thread->info.threw.type->full_name);
        }
      if (errors_fatal)
        {
          if (quiet_exceptions)
            exit (1);
          else
            dang_fatal_user_error ("uncaught exception:  aborting");
        }
      break;
    case DANG_THREAD_STATUS_CANCELLED:
      dang_warning ("Cancelled");
      break;
    case DANG_THREAD_STATUS_NOT_STARTED:
    case DANG_THREAD_STATUS_RUNNING:
      dang_warning ("should not be reached (thread status=%s)",
                    dang_thread_status_name (thread->status));
      break;
    case DANG_THREAD_STATUS_DONE:
      break;
    }
  if (interactive && function->base.sig->return_type != NULL)
    {
      char *str = dang_value_to_string (function->base.sig->return_type,
                                        thread->rv_frame + 1);
      printf ("[%s] = %s\n", function->base.sig->return_type->full_name, str);
      fflush (stdout);
      dang_free (str);
    }
  dang_thread_unref (thread);
}

static dang_boolean
interactive_handle_parser_expressions (DangParser  *parser)
{
  DangExpr *expr;
  DangImports *imports = dang_parser_peek_imports (parser);
  dang_boolean got_expr = FALSE;
  while ((expr=dang_parser_pop_expr (parser)) != NULL)
    {
      DangCompileContext *cc;
      DangError *error = NULL;
#if DANG_DEBUG
      if (dang_debug_parse)
        {
          fprintf (stderr, "got expression:\n");
          dang_debug_dump_expr (expr);
        }
#endif
      got_expr = TRUE;
      DangFunction *function;
      cc = dang_compile_context_new ();

      if ((expr = dang_do_toplevel_transforms (expr, &error)) == NULL)
        {
          dang_warning ("error doing toplevel transformations: %s", error->message);
          dang_error_unref (error);
          continue;
        }

      DangAnnotations *annotations;
      DangValueType *rv_type = NULL;
      DangVarTable *table;
      DangSignature *sig;
      DangExprTag *tag;
      if (!dang_syntax_check (expr, &error))
        {
          dang_warning ("error syntax checking: %s", error->message);
          dang_error_unref (error);
          continue;
        }
      table = dang_var_table_new (TRUE);
      dang_var_table_add_params (table, NULL, 0, NULL);
      annotations = dang_annotations_new ();
      if (!dang_expr_annotate_types (annotations, expr,
                                     imports, table, &error))
        {
          dang_warning ("error getting type of expr: %s", error->message);
          dang_error_unref (error);
          continue;
        }
      tag = dang_expr_get_annotation (annotations, expr,
                                      DANG_EXPR_ANNOTATION_TAG);
      if (tag == NULL)
        rv_type = NULL;
      else if (tag->tag_type == DANG_EXPR_TAG_STATEMENT)
        rv_type = dang_var_table_get_return_type (table);
      else if (tag->tag_type == DANG_EXPR_TAG_VALUE)
        rv_type = tag->info.value.type;
      else
        {
          dang_warning ("bad tag type '%s' parsing interactive mode",
                        dang_expr_tag_type_name (tag->tag_type));
          continue;
        }
      sig = dang_signature_new (rv_type, 0, NULL);
      function = dang_function_new_stub (imports, sig, expr,
                                         NULL, 0, NULL);
      dang_signature_unref (sig);
      dang_annotations_free (annotations);
      dang_var_table_free (table);

      /* compile */
      cc = dang_compile_context_new ();
      dang_compile_context_register (cc, function);
      if (!dang_compile_context_finish (cc, &error))
        {
          dang_warning ("error compiling: %s", error->message);
          dang_error_unref (error);
          error = NULL;
          if (errors_fatal)
            exit (1);
        }
      dang_compile_context_free (cc);

      /* run */
      function_run (function);
      dang_function_unref (function);
    }
  return got_expr;
}

int main(int argc, char **argv)
{
  unsigned i;
  const char *input_name = NULL;
  DangString *filename;
  DangError *error = NULL;
  DangParseOptions parse_options = DANG_PARSE_OPTIONS_INIT;
  DangParser *parser;
  DangTokenizer *tokenizer;
  dang_boolean got_interactive = FALSE;
  dang_boolean got_errors_fatal = FALSE;
  DangNamespace *ns = dang_namespace_default ();
  DangImportedNamespace ins;
  DangImports *imports;
  ins.reverse = TRUE;
  ins.n_names = 0;
  ins.names = NULL;
  ins.qualifier = NULL;
  ins.ns = ns;
  imports = dang_imports_new (1, &ins, ns);

  for (i = 1; i < (unsigned) argc; i++)
    {
      if (strcmp (argv[i], "-") == 0)
        {
          input_name = "-";
        }
      else if (argv[i][0] == '-')
        {
          if (strcmp (argv[i], "--help-debug") == 0)
            debug_options_usage ();
#ifdef DANG_DEBUG
          else if (strcmp (argv[i], "--debug-disassemble") == 0)
            dang_debug_disassemble = TRUE;
          else if (strcmp (argv[i], "--debug-dump-exprs") == 0)
            dang_debug_parse = TRUE;
          //else if (strcmp (argv[i], "--debug-run") == 0)
            //dang_debug_run = TRUE;
          //else if (strcmp (argv[i], "--debug-run-data") == 0)
            //dang_debug_run_data = TRUE;
          else if (strcmp (argv[i], "--debug-all") == 0)
            {
              dang_debug_disassemble = TRUE;
              dang_debug_parse = TRUE;
              //dang_debug_run = TRUE;
              //dang_debug_run_data = TRUE;
            }
#endif
          else if (strcmp (argv[i], "--errors-fatal") == 0)
            {
              got_errors_fatal = TRUE;
              errors_fatal = TRUE;
            }
          else if (strcmp (argv[i], "--errors-not-fatal") == 0)
            {
              got_errors_fatal = TRUE;
              errors_fatal = FALSE;
            }
          else if (strcmp (argv[i], "--interactive") == 0
               ||  strcmp (argv[i], "-i") == 0)
            {
              got_interactive = TRUE;
              interactive = TRUE;
              input_name = "-";
            }
          else if (strcmp (argv[i], "--not-interactive") == 0)
            {
              got_interactive = TRUE;
              interactive = FALSE;
            }
          else if (strcmp (argv[i], "--quiet-exceptions") == 0)
            {
              quiet_exceptions = TRUE;
            }
          else if (strcmp (argv[i], "-I") == 0)
            {
              if (i + 1 == (unsigned)argc)
                {
                  fprintf (stderr, "-I requires a parameter\n");
                  return 1;
                }
              dang_module_add_path (argv[i+1]);
              i++;
            }
          else if (strncmp (argv[i], "-I", 2) == 0)
            {
              dang_module_add_path (argv[i]+2);
            }
          else
            {
              dang_warning ("unknown option: `%s'\n", argv[i]);
              usage ();
            }
        }
      else
        {
          input_name = argv[i];
        }
    }

  if (input_name == NULL)
    usage ();

  if (!got_errors_fatal)
    errors_fatal = !interactive;

  filename = dang_string_new (input_name);

  if (!interactive)
    {
      DangRunFileOptions options = DANG_RUN_FILE_OPTIONS_DEFAULTS;
      if (!dang_run_file (input_name, &options, &error))
        {
          if (!quiet_exceptions)
            fprintf (stderr, "ERROR: %s\n\n", error->message);
          return 1;
        }
      goto cleanup;
    }

  parse_options.mode = DANG_PARSE_MODE_TOPLEVEL;
  parse_options.imports = imports;
  parser = dang_parser_create (&parse_options, &error);
  if (parser == NULL)
    dang_die ("error creating default parser?: %s",
              error->message);

  /* parse into expressions */
  tokenizer = dang_tokenizer_new (filename);
  {
    dang_boolean pending = FALSE;
    char *str;
//DangDefaultParser_Trace(stderr, "<TRACE>");
    while ((str=readline (pending ? "...> " : ">>>> ")) != NULL)
      {
        DangToken *token;
        const char *at = str;
        while (*at && isspace (*at))
          at++;
        if (*at == 0)
          continue;
        add_history (str);
        if (!dang_tokenizer_feed (tokenizer, strlen (str), str, &error))
          {
            dang_warning ("error tokenizing: %s", error->message);
            if (errors_fatal)
              exit (1);
            dang_tokenizer_free (tokenizer);
            tokenizer = dang_tokenizer_new (filename);
          }
        while ((token=dang_tokenizer_pop_token (tokenizer)) != NULL)
          {
            if (!dang_parser_parse (parser, token, &error))
              {
                dang_warning ("parse failed: %s", error->message);
                if (errors_fatal)
                  exit (1);
                dang_error_unref (error);
                error = NULL;
                parser = dang_parser_create (&parse_options, &error);
                if (parser == NULL)
                  dang_die ("error recreating default parser?: %s",
                            error->message);
                pending = FALSE;
                continue;
              }
            interactive_handle_parser_expressions (parser);
          }
        if (!dang_parser_end_parse (parser, &error))
          {
            dang_warning ("error: %s", error->message);
          }
        interactive_handle_parser_expressions (parser);
        dang_parser_destroy (parser);
        parser = dang_parser_create (&parse_options, &error);
      }
  }
  if (!dang_parser_end_parse (parser, &error))
    {
      dang_die ("error at end-of-file: %s", error->message);
    }
  interactive_handle_parser_expressions (parser);
  dang_tokenizer_free (tokenizer);
  dang_parser_destroy (parser);

cleanup:
  dang_imports_unref (imports);
  dang_string_unref (filename);
  dang_cleanup ();
  return 0;
}
