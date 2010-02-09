#include <string.h>
#include "dang.h"
#include "magic.h"

static void
init_assign__struct (DangValueType *type,
                     void          *dst,
                     const void    *src)
{
  DangValueTypeStruct *stype = (DangValueTypeStruct *) type;
  unsigned i;
  for (i = 0; i < stype->n_members; i++)
    {
      DangValueType *mtype = stype->members[i].type;
      void *dval = (char*)dst + stype->members[i].offset;
      if (DANG_UNLIKELY (stype->members[i].name == NULL))
        {
          if (stype->members[i].has_default_value)
            {
              if (mtype->init_assign != NULL)
                mtype->init_assign (mtype, dval, stype->members[i].default_value);
              else
                memcpy (dval, stype->members[i].default_value, mtype->sizeof_instance);
            }
          else
            memset ((char*)dst + stype->members[i].offset,
                    0, stype->members[i].type->sizeof_instance);
        }
      else if (mtype->init_assign != NULL)
        mtype->init_assign (mtype, dval, (char*)src + stype->members[i].offset);
      else
        memcpy (dval, (char*)src + stype->members[i].offset,
                mtype->sizeof_instance);
    }
}
static void
assign__struct (DangValueType *type,
                void          *dst,
                const void    *src)
{
  DangValueTypeStruct *stype = (DangValueTypeStruct *) type;
  unsigned i;
  for (i = 0; i < stype->n_members; i++)
    {
      DangValueType *mtype = stype->members[i].type;
      void *dval = (char*)dst + stype->members[i].offset;
      if (DANG_UNLIKELY (stype->members[i].name == NULL))
        {
          if (stype->members[i].has_default_value)
            {
              if (mtype->assign != NULL)
                mtype->assign (mtype, dval, stype->members[i].default_value);
              else
                memcpy (dval, stype->members[i].default_value, mtype->sizeof_instance);
            }
          else
            memset ((char*)dst + stype->members[i].offset,
                    0, stype->members[i].type->sizeof_instance);
        }
      else if (mtype->assign != NULL)
        mtype->assign (mtype, dval, (char*)src + stype->members[i].offset);
      else
        memcpy (dval, (char*)src + stype->members[i].offset,
                mtype->sizeof_instance);
    }
}

static void
destruct__struct (DangValueType *type,
                void          *dst)
{
  DangValueTypeStruct *stype = (DangValueTypeStruct *) type;
  unsigned i;
  for (i = 0; i < stype->n_members; i++)
    if (stype->members[i].type->destruct != NULL)
      stype->members[i].type->destruct (stype->members[i].type,
                                      (char*)dst + stype->members[i].offset);
}

static char *
to_string__struct (DangValueType *type,
                   const void    *value)
{
  DangStringBuffer buf = DANG_STRING_BUFFER_INIT;
  unsigned i;
  DangValueTypeStruct *stype = (DangValueTypeStruct *) type;
  dang_string_buffer_printf (&buf, "%s(", type->full_name);
  for (i = 0; i < stype->n_members; i++)
    if (stype->members[i].name != NULL)
      {
        char *str;
        dang_string_buffer_printf (&buf, "%s -> ", stype->members[i].name);
        str = dang_value_to_string (stype->members[i].type,
                                    (const char*)value + stype->members[i].offset);
        dang_string_buffer_append (&buf, str);
        if (i + 1 < stype->n_members)
          dang_string_buffer_append (&buf, ", ");
        dang_free (str);
      }
  dang_string_buffer_append (&buf, ")");
  return buf.str;
}

static DangValueTypeStruct *global_struct_type_list = NULL;


DangValueType *dang_value_type_new_struct (char *name,
                                           unsigned    n_members,
				           DangStructMember *members)
{
  unsigned max_align = 1;
  unsigned offset = 0;
  DangValueTypeStruct *rv;
  dang_boolean use_memcpy = TRUE;
  unsigned i;
  for (i = 0; i < n_members; i++)
    {
      offset = DANG_ALIGN (offset, members[i].type->alignof_instance);
      max_align = DANG_MAX (max_align, members[i].type->alignof_instance);
      members[i].offset = offset;
      offset += members[i].type->sizeof_instance;
      if (members[i].type->init_assign != NULL)
        use_memcpy = FALSE;
    }
  offset = DANG_ALIGN (offset, max_align);

  rv = dang_new0 (DangValueTypeStruct, 1);
  rv->base_type.magic = DANG_VALUE_TYPE_MAGIC;
  rv->base_type.ref_count = 1;
  rv->base_type.full_name = name;         /* takes ownership */
  rv->base_type.sizeof_instance = offset;
  rv->base_type.alignof_instance = max_align;
  
  if (!use_memcpy)
    {
      rv->base_type.init_assign = init_assign__struct;
      rv->base_type.assign = assign__struct;
      rv->base_type.destruct = destruct__struct;
    }
  rv->base_type.to_string = to_string__struct;

  for (i = 0; i < n_members; i++)
    if (members[i].name != NULL)
      {
        dang_value_type_add_simple_member (&rv->base_type,
                                           members[i].name,
                                           DANG_MEMBER_COMPLETELY_PUBLIC,
                                           members[i].type,
                                           FALSE,
                                           members[i].offset);
      }
  rv->n_members = n_members;
  rv->members = members;                /* takes ownership */

  rv->prev_struct = global_struct_type_list;
  global_struct_type_list = rv;

  return &rv->base_type;
}

dang_boolean dang_value_type_is_struct (DangValueType* type)
{
  return type->to_string == to_string__struct;
}

typedef struct _EqualMemberData EqualMemberData;
struct _EqualMemberData
{
  DangFunction *func;
  unsigned offset : 31;
  unsigned must_unref_func : 1;
};

typedef struct _EqualData EqualData;
struct _EqualData
{
  dang_boolean invert;          /* if TRUE, implement not_equal() */
  unsigned n_members;
  EqualMemberData members[1];   /* more follow */
};

static void free_equal_data (void *data)
{
  EqualData *eqd = data;
  unsigned i;
  for (i = 0; i < eqd->n_members; i++)
    if (eqd->members[i].must_unref_func)
      dang_function_unref (eqd->members[i].func);
  dang_free (eqd);
}

static DANG_SIMPLE_C_FUNC_DECLARE (implement_equals)
{
  EqualData *eqd = func_data;
  char *a[2] = { args[0], args[1] };
  void *arg_values[2];
  unsigned i;
  for (i = 0; i < eqd->n_members; i++)
    {
      char subrv;
      arg_values[0] = a[0] + eqd->members[i].offset;
      arg_values[1] = a[1] + eqd->members[i].offset;
      if (!dang_function_call_nonyielding_v (eqd->members[i].func, &subrv, arg_values, error))
        return FALSE;
      if (!subrv)
        {
          /* structures are not equal */
          * (char*) rv_out = eqd->invert;
          return TRUE;
        }
    }
  /* structures are equal */
  * (char*) rv_out = 1 - eqd->invert;
  return TRUE;
}

static DangFunction *
struct_equal_try_sig  (DangMatchQuery *query,
                       void *data,
                       DangError **error)
{
  /* Gather operator_equal function and offset for each member */
  EqualData *eqd;
  DangMatchQueryElement subelts[2];
  DangMatchQuery subquery = *query;
  DangValueTypeStruct *stype;
  unsigned i;
  DangFunctionParam fparams[2];
  DangSignature *sig;
  DangFunction *rv;
  if (query->n_elements != 2
   || query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT
   || !dang_value_type_is_struct (query->elements[1].info.simple_input)
   || query->elements[1].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT
   || query->elements[0].info.simple_input != query->elements[1].info.simple_input)
    return FALSE;

  stype = (DangValueTypeStruct *) query->elements[1].info.simple_input;
  eqd = dang_malloc (sizeof (EqualData)
                     + (stype->n_members ? (stype->n_members-1) : 0) * sizeof (EqualMemberData));
  eqd->invert = (dang_boolean) data;
  eqd->n_members = stype->n_members;
  subquery.elements = subelts;
  subelts[0].type = subelts[1].type = DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT;
  for (i = 0; i < stype->n_members; i++)
    {
      char *name = "operator_equal";
      DangFunction *func;
      DangValueType *mtype = stype->members[i].type;
      subelts[0].info.simple_input = mtype;
      subelts[1].info.simple_input = mtype;
      func = dang_imports_lookup_function (query->imports, 1, &name, &subquery, error);
      if (func == NULL)
        {
          dang_error_add_suffix (*error, "in equality test for %s's %s",
                                 stype->base_type.full_name, stype->members[i].name);
          return NULL;
        }
      if (func->base.sig->return_type != dang_value_type_boolean ())
        {
          dang_error_add_suffix (*error, "equality test for %s's %s didn't return boolean",
                                 stype->base_type.full_name, stype->members[i].name);
          return NULL;
        }
      eqd->members[i].offset = stype->members[i].offset;
      eqd->members[i].func = func;
      if (func->base.is_owned)
        {
          eqd->members[i].must_unref_func = 0;
          dang_function_unref (func);
        }
      else
        {
          eqd->members[i].must_unref_func = 1;
        }
    }
  fparams[0].name = NULL;
  fparams[0].type = (DangValueType *) stype;
  fparams[0].dir = DANG_FUNCTION_PARAM_IN;
  fparams[1] = fparams[0];
  sig = dang_signature_new (dang_value_type_boolean (), 2, fparams);
  rv = dang_function_new_simple_c (sig, implement_equals, eqd, free_equal_data);
  dang_signature_unref (sig);
  return rv;
}

void _dang_struct_init (DangNamespace *ns)
{
  DangFunctionFamily *equal, *not_equal;
  DangError *error = NULL;
  equal = dang_function_family_new_variadic_c ("operator_equal(struct)",
                                               struct_equal_try_sig, (void*)0,
                                               NULL);
  not_equal = dang_function_family_new_variadic_c ("operator_not_equal(struct)",
                                               struct_equal_try_sig, (void*)1,
                                               NULL);
  if (!dang_namespace_add_function_family (ns, "operator_not_equal", not_equal, &error))
    dang_die ("adding operator_not_equal: %s", error->message);
  if (!dang_namespace_add_function_family (ns, "operator_equal", equal, &error))
    dang_die ("adding operator_equal: %s", error->message);
  dang_function_family_unref (equal);
  dang_function_family_unref (not_equal);
}

void _dang_struct_cleanup1 (void)
{
  DangValueTypeStruct *s;
  unsigned i;
  for (s = global_struct_type_list; s; s = s->prev_struct)
    for (i = 0; i < s->n_members; i++)
      if (s->members[i].has_default_value)
        {
          if (s->members[i].type->destruct != NULL)
            s->members[i].type->destruct (s->members[i].type,
                                          s->members[i].default_value);
          dang_free (s->members[i].default_value);
          s->members[i].has_default_value = FALSE;      /* unneeded */
        }
}

void _dang_struct_cleanup2 (void)
{
  while (global_struct_type_list)
    {
      DangValueTypeStruct *kill = global_struct_type_list;
      unsigned i;
      global_struct_type_list = kill->prev_struct;

      /* free 'kill' */
      dang_value_type_cleanup (&kill->base_type);
      for (i = 0; i < kill->n_members; i++)
        dang_free (kill->members[i].name);
      dang_free (kill->members);
      dang_free (kill->base_type.full_name);
      dang_free (kill);
    }
}
