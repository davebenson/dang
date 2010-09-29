#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../dsk.h"

typedef enum
{
  RENDER_MODE_H_FILE,
  RENDER_MODE_C_FILE
} RenderMode;

static const char description[] =
      "Produce C bindings for the xml parsers\n"
      "generated by DskXmlBinding.\n\n"
      "Renders a .h file which contains the prototypes,\n"
      "and a .c file which contains the data DskXmlBindingType structures.\n";

static DskXmlBinding *binding;

static DSK_CMDLINE_CALLBACK_DECLARE (handle_searchpath)
{
  DSK_UNUSED (arg_name);
  DSK_UNUSED (error);
  dsk_xml_binding_add_searchpath (binding,
                                  arg_value,
                                  (const char *) callback_data);
  return DSK_TRUE;
}

#if 0
static int pstrcmp (const void *a, const void *b)
{
  const char *A = * (char **) a;
  const char *B = * (char **) b;
  return strcmp (A, B);
}
#endif

static void set_string_ns (DskPrint *ctx,
                           const char *var,
                           const char *name,
                           dsk_boolean uc_first,
                           dsk_boolean uc_nonfirst)
{
  char *str = dsk_malloc (strlen (name) * 2 + 1);
  char *at = str;
  const char *in = name;
  dsk_boolean next_is_first = DSK_TRUE;
  while (*in)
    {
      if (*in == '.')
        {
          *at++ = '_';
          *at++ = '_';
          in++;
          next_is_first = DSK_TRUE;
        }
      else if (*in == '_' || *in == '-')
        {
          next_is_first = DSK_TRUE;
          in++;
        }
      else
        {
          if (( next_is_first && uc_first)
           || (!next_is_first && uc_nonfirst))
            *at++ = dsk_ascii_toupper (*in);
          else
            *at++ = dsk_ascii_tolower (*in);
          in++;
          next_is_first = DSK_FALSE;
        }
    }
  *at = 0;
  dsk_print_set_string (ctx, var, str);
  dsk_free (str);
}

static void
set_struct_uppercase (DskPrint *ctx, const char *var, const char *value)
{
  const char *in = value;
  char *str = dsk_malloc (strlen (value) * 2 + 1);
  char *out = str;
  while (*in)
    {
      if (in != value && dsk_ascii_isupper (*in))
        *out++ = '_';
      *out++ = dsk_ascii_toupper (*in);
      in++;
    }
  *out = 0;
  dsk_print_set_string (ctx, var, str);
  dsk_free (str);
}
static void
set_struct_lowercase (DskPrint *ctx, const char *var, const char *value)
{
  const char *in = value;
  char *str = dsk_malloc (strlen (value) * 2 + 1);
  char *out = str;
  while (*in)
    {
      if (in != value && dsk_ascii_isupper (*in))
        *out++ = '_';
      *out++ = dsk_ascii_tolower (*in);
      in++;
    }
  *out = 0;
  dsk_print_set_string (ctx, var, str);
  dsk_free (str);
}

static void
set_ctypename (DskPrint *ctx,
               const char *name,
               DskXmlBindingType *type)
{
  if (type->ctypename)
    {
      dsk_print_set_string (ctx, name, type->ctypename);
    }
  else
    {
      unsigned max_size = 3 * strlen (type->ns->name)  /* max possible expansion == a -> A__ */
                        + 2                     /* __ */
                        + strlen (type->name)   /* (no transformation on type-name itself) */
                        + 1;                    /* nul-terminate */
      char *str = dsk_malloc (max_size);
      char *at = str;
      const char *in = type->ns->name;
      dsk_boolean next_is_upper = DSK_TRUE;
      while (*in)
        {
          if (*in == '.')
            {
              *at++ = '_';
              *at++ = '_';
              in++;
              next_is_upper = DSK_TRUE;
            }
          else if (*in == '_' || *in == '-')
            {
              next_is_upper = DSK_TRUE;
              in++;
            }
          else
            {
              if (next_is_upper)
                *at++ = dsk_ascii_toupper (*in);
              else
                *at++ = dsk_ascii_tolower (*in);
              in++;
              next_is_upper = DSK_FALSE;
            }
        }
      *at++ = '_';
      *at++ = '_';
      strcpy (at, type->name);
      dsk_print_set_string (ctx, name, str);
      dsk_free (str);
    }
}

static void
set_boolean (DskPrint *ctx,
             const char *name,
             dsk_boolean value)
{
  dsk_print_set_string (ctx, name, value ? "DSK_TRUE" : "DSK_FALSE");
}

static void
render_member (DskPrint *ctx, DskXmlBindingStructMember *member)
{
  set_ctypename (ctx, "member_type", member->type);
  dsk_print_set_string (ctx, "member_name", member->name);
  if (member->quantity == DSK_XML_BINDING_REQUIRED
   || member->quantity == DSK_XML_BINDING_OPTIONAL)
    dsk_print_set_string (ctx, "maybe_star", "");
  else
    dsk_print_set_string (ctx, "maybe_star", "*");
  if (member->quantity == DSK_XML_BINDING_REQUIRED_REPEATED
   || member->quantity == DSK_XML_BINDING_REPEATED)
    dsk_print (ctx, "${indent}unsigned n_$member_name;");
  if (member->quantity == DSK_XML_BINDING_OPTIONAL)
    dsk_print (ctx, "${indent}dsk_boolean has_$member_name;");
  dsk_print (ctx, "${indent}$member_type $maybe_star$member_name;");
}

static void
render_member_descriptor (DskPrint *ctx, DskXmlBindingStructMember *member)
{
  dsk_print_set_string (ctx, "member_name", member->name);
  switch (member->quantity)
    {
    case DSK_XML_BINDING_REQUIRED:
      dsk_print_set_string (ctx, "quantity", "DSK_XML_BINDING_REQUIRED");
      dsk_print_set_string (ctx, "qoffset", "0");
      break;
    case DSK_XML_BINDING_OPTIONAL:
      dsk_print_set_string (ctx, "quantity", "DSK_XML_BINDING_OPTIONAL");
      dsk_print_set_template_string (ctx, "qoffset", "DSK_OFFSETOF ($fulltypename, has_$member_name)");
      break;
    case DSK_XML_BINDING_REPEATED:
    case DSK_XML_BINDING_REQUIRED_REPEATED:
      if (member->quantity == DSK_XML_BINDING_REPEATED)
        dsk_print_set_string (ctx, "quantity", "DSK_XML_BINDING_REPEATED");
      else
        dsk_print_set_string (ctx, "quantity", "DSK_XML_BINDING_REQUIRED_REPEATED");
      dsk_print_set_template_string (ctx, "qoffset", "DSK_OFFSETOF ($fulltypename, n_$member_name)");
      break;
    }
  if (member->type->ns->name)
    set_string_ns (ctx, "member_namespace_func_prefix", member->type->ns->name, 0, 0);
  else
    dsk_print_set_string (ctx, "member_namespace_func_prefix", "dsk_xml_binding");
  set_struct_lowercase (ctx, "member_func_prefix", member->type->name);
  dsk_print_set_template_string (ctx, "member_c_type",
                                 "${member_namespace_func_prefix}__${member_func_prefix}__type");
  dsk_print (ctx,
             "$indent{\n"
             "$indent  $quantity,\n"
             "$indent  \"$member_name\",\n"
             "$indent  $member_c_type,\n"
             "$indent  $qoffset,\n"
             "$indent  DSK_OFFSETOF ($fulltypename, $member_name)\n"
             "$indent},");
}

static void
render_file (DskXmlBindingNamespace *ns,
             const char *output_basename,
             RenderMode mode)
{
  DskPrint *ctx;
  char *name = dsk_malloc (strlen (output_basename) + 10);
  FILE *fp;
  unsigned i, j, k;
  strcpy (name, output_basename);
  switch (mode)
    {
    case RENDER_MODE_H_FILE: strcat (name, ".h"); break;
    case RENDER_MODE_C_FILE: strcat (name, ".c"); break;
    }
  fp = fopen (name, "w");
  if (fp == NULL)
    dsk_die ("error creating %s: %s", name, strerror (errno));

  ctx = dsk_print_new_fp_fclose (fp);
  dsk_print_set_string (ctx, "output_basename", output_basename);
  dsk_print_set_string (ctx, "output_filename", name);

  dsk_print (ctx,
             "/* $output_filename -- generated by dsk-make-xml-binding */\n"
             "/* DO NOT HAND EDIT - changes will be lost */\n\n");
  switch (mode)
    {
    case RENDER_MODE_H_FILE:
      dsk_print (ctx, "#include <dsk/dsk.h>");
      break;
    case RENDER_MODE_C_FILE:
      dsk_print (ctx, "#include \"$output_basename.h\"");
      break;
    }

  /* convert ns->name into camel-case, lowercase and uppercase strings */
  dsk_print_set_string (ctx, "namespace_name", ns->name);
  set_string_ns (ctx, "namespace_func_prefix", ns->name, 0, 0);
  set_string_ns (ctx, "namespace_type_prefix", ns->name, 1, 0);
  set_string_ns (ctx, "namespace_enum_prefix", ns->name, 1, 1);

#if 0
  /* gather all enums / unions in this namespace; emit #define's */
  for (i = 0; i < ns->n_types; i++)
    if (ns->types[i]->is_union)
      n_labels += ((DskXmlBindingTypeUnion*)ns->types[i])->n_cases;
  labels = dsk_malloc (sizeof (char *) * n_labels);
  k = 0;
  for (i = 0; i < ns->n_types; i++)
    if (ns->types[i]->is_union)
      {
        DskXmlBindingTypeUnion *t = ((DskXmlBindingTypeUnion*)ns->types[i]);
        for (j = 0; j < t->n_cases; j++)
          labels[k] = t->cases[j].name;
      }
  dsk_assert (k == n_labels);

  /* uniquify all labels */
  qsort (labels, k, sizeof (char*), pstrcmp);
  if (n_labels > 0)
    {
      unsigned o = 0;
      for (i = 1; i < n_labels; i++)
        if (strcmp (labels[o], labels[i]) != 0)
          labels[++o] = labels[i];
      n_labels = o + 1;
    }

  for (i = 0; i < n_labels; i++)
    {
      dsk_print_push (ctx);
      set_struct_uppercase (ctx, "label", labels[i]);
      dsk_print_set_int (ctx, "value", i);
      dsk_print (ctx, "#define ${namespace_enum_prefix}__ENUM_VALUE__$label   $value");
      dsk_print_pop (ctx);
    }
#endif

  /* render typedefs for structures and unions */
  if (mode == RENDER_MODE_H_FILE)
    {
      for (i = 0; i < ns->n_types; i++)
        if (ns->types[i]->is_struct
         || ns->types[i]->is_union)
          {
            dsk_print_push (ctx);
            dsk_print_set_string (ctx, "typename", ns->types[i]->name);
            //set_struct_lowercase (ctx, "typename_func_prefix", ns->entries[i].name);
            //set_struct_uppercase (ctx, "typename_func_prefix", ns->entries[i].name);
            dsk_print_set_template_string (ctx,
                                           "fulltypename",
                                           "${namespace_type_prefix}__$typename");
            dsk_print (ctx, "typedef struct _$fulltypename $fulltypename;");
            dsk_print_pop (ctx);
          }
    }

  /* render c-enums for unions/enums */
  if (mode == RENDER_MODE_H_FILE)
    for (i = 0; i < ns->n_types; i++)
      if (ns->types[i]->is_union)
        {
          DskXmlBindingTypeUnion *t = ((DskXmlBindingTypeUnion*)ns->types[i]);
          dsk_print (ctx, "typedef enum\n{");
          dsk_print_push (ctx);
          set_struct_uppercase (ctx, "uc_typename", t->base_type.name);
          //set_struct_lowercase (ctx, "lc_typename", t->base_type.name);
          dsk_print_set_string (ctx, "typename", t->base_type.name);
          for (j = 0; j < t->n_cases; j++)
            {
              dsk_print_push (ctx);
              set_struct_uppercase (ctx, "label", t->cases[j].name);
              dsk_print_set_template_string (ctx, "elabel",
                           "${namespace_enum_prefix}__${uc_typename}__$label");
              dsk_print (ctx, "  $elabel,");
              dsk_print_pop (ctx);
            }
          dsk_print (ctx, "} ${namespace_type_prefix}__${typename}_Type;");
          dsk_print_pop (ctx);
        }

  /* render struct's and union's */
  for (i = 0; i < ns->n_types; i++)
    if (ns->types[i]->is_struct)
      {
        DskXmlBindingType *type = ns->types[i];
        DskXmlBindingTypeStruct *s = (DskXmlBindingTypeStruct *) type;
        dsk_print_push (ctx);
        set_ctypename (ctx, "fulltypename", type);
        set_struct_lowercase (ctx, "typename_lowercase", ns->types[i]->name);
        dsk_print_set_template_string (ctx, "fullfuncprefix",
                                       "${namespace_func_prefix}__${typename_lowercase}");
        switch (mode)
          {
          case RENDER_MODE_H_FILE:
            dsk_print (ctx, "struct _$fulltypename\n{");
            break;
          case RENDER_MODE_C_FILE:
            dsk_print (ctx, "static const DskXmlBindingStructMember ${fullfuncprefix}__members[] =\n{");
            break;
          }
        for (j = 0; j < s->n_members; j++)
          {
            dsk_print_push (ctx);
            dsk_print_set_string (ctx, "indent", "  ");
            switch (mode)
              {
              case RENDER_MODE_H_FILE:
                render_member (ctx, s->members + j);
                break;
              case RENDER_MODE_C_FILE:
                render_member_descriptor (ctx, s->members + j);
                break;
              }
            dsk_print_pop (ctx);
          }
        dsk_print_pop (ctx);
        dsk_print (ctx, "};");
      }
    else if (ns->types[i]->is_union)
      {
        DskXmlBindingType *type = ns->types[i];
        DskXmlBindingTypeUnion *u = (DskXmlBindingTypeUnion *) type;
        dsk_print_push (ctx);
        set_ctypename (ctx, "fulltypename", type);
        set_struct_lowercase (ctx, "typename_lowercase", ns->types[i]->name);
        dsk_print_set_template_string (ctx, "fullfuncprefix",
                                       "${namespace_func_prefix}__${typename_lowercase}");
        switch (mode)
          {
          case RENDER_MODE_H_FILE:
            dsk_print (ctx, "struct _$fulltypename\n"
                            "{\n"
                            "  ${fulltypename}_Type type;\n"
                            "  union {");
            break;
          case RENDER_MODE_C_FILE:
            dsk_print (ctx, "static const DskXmlBindingUnionCase ${fullfuncprefix}__cases[] =\n{");
            break;
          }
        for (j = 0; j < u->n_cases; j++)
          {
            DskXmlBindingType *ct = u->cases[j].type;
            dsk_print_push (ctx);
            dsk_print_set_string (ctx, "case_name", u->cases[j].name);
            switch (mode)
              {
              case RENDER_MODE_H_FILE:
                if (u->cases[j].elide_struct_outer_tag)
                  {
                    DskXmlBindingTypeStruct *cs = (DskXmlBindingTypeStruct*) ct;
                    dsk_assert (ct->is_struct);
                    dsk_print (ctx, "    struct {");
                    for (k = 0; k < cs->n_members; k++)
                      {
                        dsk_print_push (ctx);
                        dsk_print_set_string (ctx, "indent", "      ");
                        render_member (ctx, cs->members + k);
                        dsk_print_pop (ctx);
                      }
                    dsk_print (ctx, "    } $case_name;");
                  }
                else if (ct != NULL)
                  {
                    set_ctypename (ctx, "case_type", ct);
                    dsk_print (ctx, "    $case_type $case_name;");
                  }
                break;
              case RENDER_MODE_C_FILE:
                dsk_print_push (ctx);
                dsk_print_set_string (ctx, "name", u->cases[j].name);
                set_boolean (ctx, "elide_struct_outer_tag",
                             u->cases[j].elide_struct_outer_tag);

                if (u->cases[j].type && u->cases[j].type->ns == NULL)
                  {
                    dsk_print_set_template_string (ctx, "case_type",
             "${namespace_func_prefix}__${typename_lowercase}__${name}__type");
                  }
                else if (u->cases[j].type)
                  {
                    if (u->cases[j].type->ns->name)
                      set_string_ns (ctx, "namespace_func_prefix", u->cases[j].type->ns->name, 0, 0);
                    else
                      dsk_print_set_string (ctx, "namespace_func_prefix", "dsk_xml_binding");
                    set_struct_lowercase (ctx, "type_func_prefix", u->cases[j].type->name);
                    dsk_print_set_template_string (ctx, "case_type", "${namespace_func_prefix}__${type_func_prefix}__type");
                  }
                else
                  {
                    dsk_print_set_string (ctx, "case_type", "NULL");
                  }
                dsk_print (ctx,
                           "  {\n"
                           "    \"$name\",\n"
                           "    $elide_struct_outer_tag,\n"
                           "    $case_type\n"
                           "  },");
                dsk_print_pop (ctx);
                break;
              }
          }
        switch (mode)
          {
          case RENDER_MODE_H_FILE:
            dsk_print (ctx, "  } info;\n"
                            "};");
            break;
          case RENDER_MODE_C_FILE:
            dsk_print (ctx, "};");
            break;
          }
        dsk_print_pop (ctx);
      }

  /* Render global structures */
  if (mode == RENDER_MODE_C_FILE)
    {
      for (i = 0; i < ns->n_types; i++)
        if (ns->types[i]->is_struct)
          {
            dsk_print (ctx,
                       "DskXmlBindingTypeStruct ${full_func_prefix}__descriptor =\n"
                       "{\n"
                       "  {\n"
                       "    DSK_FALSE,    /* !is_fundamental */\n"
                       "    DSK_TRUE,     /* is_static */\n"
                       "    DSK_TRUE,     /* is_struct */\n"
                       "    DSK_FALSE,    /* is_union */\n"
                       "    0,            /* ref_count */\n"
                       "    sizeof (${full_type}),\n"
                       "    alignof (${full_type}),\n"
                       "    \"$full_type\",\n"
                       "    NULL,    /* ctypename==typename */\n"
                       "    dsk_xml_binding_struct_parse,\n"
                       "    dsk_xml_binding_struct_to_xml,\n"
                       "    $dsk_xml_binding_struct_clear,\n"
                       "    NULL           /* finalize_type */\n"
                       "  },\n"
                       "  $n_members,\n"
                       "  ${full_func_prefix}__members,\n"
                       "  ${full_func_prefix}__members_sorted_by_name\n"
                       "};\n");
          }
        else if (ns->types[i]->is_union)
          {
            dsk_print (ctx,
                       "DskXmlBindingTypeUnion ${full_func_prefix}__descriptor =\n"
                       "{\n"
                       "  {\n"
                       "    DSK_FALSE,    /* !is_fundamental */\n"
                       "    DSK_TRUE,     /* is_static */\n"
                       "    DSK_FALSE,    /* is_struct */\n"
                       "    DSK_TRUE,     /* is_union */\n"
                       "    0,            /* ref_count */\n"
                       "    sizeof (${full_type}),\n"
                       "    alignof (${full_type}),\n"
                       "    \"$full_type\",\n"
                       "    NULL,    /* ctypename==typename */\n"
                       "    dsk_xml_binding_union_parse,\n"
                       "    dsk_xml_binding_union_to_xml,\n"
                       "    $dsk_xml_binding_union_clear,\n"
                       "    NULL           /* finalize_type */\n"
                       "  },\n"
                       "  $n_cases,\n"
                       "  DSK_OFFSETOF ($full_type, variant),\n"
                       "  ${full_func_prefix}__cases,\n"
                       "  ${full_func_prefix}__cases_sorted_by_name\n"
                       "};\n");
          }
    }

  /* render descriptor declarations */
  if (mode == RENDER_MODE_H_FILE)
    {
      for (i = 0; i < ns->n_types; i++)
        if (ns->types[i]->is_struct
         || ns->types[i]->is_union)
          {
            dsk_print_push (ctx);
            set_struct_lowercase (ctx, "label", ns->types[i]->name);
            if (ns->types[i]->is_struct)
              dsk_print_set_string (ctx, "desc_type", "DskXmlBindingTypeStruct");
            else
              dsk_print_set_string (ctx, "desc_type", "DskXmlBindingTypeUnion");
            dsk_print_set_template_string (ctx, "func_prefix",
                                           "${namespace_func_prefix}__${label}");

            dsk_print (ctx, "extern const $desc_type ${func_prefix}__descriptor;");
            dsk_print (ctx, "#define ${func_prefix}__type ((DskXmlBindingType*)(&${func_prefix}__descriptor))");
            dsk_print_pop (ctx);
          }
    }

  /* render namespace object */
  switch (mode)
    {
    case RENDER_MODE_H_FILE:
      dsk_print (ctx, "extern const DskXmlBindingNamespace ${namespace_func_prefix}__descriptor;");
      break;
    case RENDER_MODE_C_FILE:
      dsk_print_set_string (ctx, "ns_name", ns->name);
      dsk_print_set_uint (ctx, "n_types", ns->n_types);
      if (ns->types_sorted_by_name)
        {
          for (i = 0; i < ns->n_types; i++)
            if (ns->types_sorted_by_name[i] != i)
              break;
        }
      else
        i = ns->n_types;
      if (i < ns->n_types)
        {
          /* needs sorting array */
          dsk_print_set_template_string (ctx, "types_sorted_by_name",
                                         "${namespace_func_prefix}__type_sorted_by_name");
          dsk_print (ctx, "static unsigned $types_sorted_by_name[] =\n{");
          for (i = 0; i < ns->n_types; i++)
            {
              dsk_print_push (ctx);
              dsk_print_set_uint (ctx, "index", ns->types_sorted_by_name[i]);
              dsk_print_set_string (ctx, "name", ns->types[ns->types_sorted_by_name[i]]->name);
              dsk_print (ctx, "  $index,   /* types[$index] = $name */");
              dsk_print_pop (ctx);
            }
          dsk_print (ctx, "};");
        }
      else
        dsk_print_set_string (ctx, "types_sorted_by_name", "NULL");

      dsk_print (ctx, "static DskXmlBindingType *${namespace_func_prefix}__type_array[] =\n{");
      for (i = 0; i < ns->n_types; i++)
        {
          dsk_print_push (ctx);
          set_struct_lowercase (ctx, "type_lowercase", ns->types[i]->name);
          dsk_print (ctx, "  ${namespace_func_prefix}__${type_lowercase}__type,");
          dsk_print_pop (ctx);
        }
      dsk_print (ctx, "};\n");

      dsk_print (ctx, "const DskXmlBindingNamespace ${namespace_func_prefix}__descriptor =\n"
                      "{\n"
                      "  DSK_TRUE,              /* is_static */\n"
                      "  \"$ns_name\",\n"            
                      "  $n_types,               /* n_types */\n"
                      "  ${namespace_func_prefix}__type_array,\n"
                      "  0,                     /* ref_count */\n"
                      "  $types_sorted_by_name\n"
                      "};");
      break;
    }

  dsk_free (name);
  dsk_print_free (ctx);
}

int main(int argc, char **argv)
{
  char *output_basename;
  char *namespace_name;
  DskXmlBindingNamespace *ns;
  DskError *error = NULL;

  dsk_cmdline_init ("Make XML C-Bindings", description, NULL, 0);
  dsk_cmdline_add_string ("output-basename",
                          "Directory and basename for output files",
                          "FILE_PREFIX",
                          DSK_CMDLINE_MANDATORY,
                          &output_basename);
  dsk_cmdline_add_string ("namespace",
                          "Namespace to generate prototypes/descriptors for",
                          "NS",
                          DSK_CMDLINE_MANDATORY,
                          &namespace_name);
  dsk_cmdline_add_func ("search-tree",
                        "Filesystem tree to search for format files",
                        "PATH",
                        DSK_CMDLINE_REPEATABLE,
                        handle_searchpath, "/");
  dsk_cmdline_add_func ("search-dotted",
                        "Filesystem area to search for format files; namespaces are separated by '.'",
                        "PATH",
                        DSK_CMDLINE_REPEATABLE,
                        handle_searchpath, ".");

  binding = dsk_xml_binding_new ();
  dsk_cmdline_process_args (&argc, &argv);

  ns = dsk_xml_binding_get_ns (binding, namespace_name, &error);
  if (ns == NULL)
    dsk_die ("getting namespace '%s': %s", namespace_name, error->message);
  render_file (ns, output_basename, RENDER_MODE_H_FILE);
  render_file (ns, output_basename, RENDER_MODE_C_FILE);

  dsk_xml_binding_free (binding);
  dsk_cleanup ();

  return 0;
}
