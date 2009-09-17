#include <string.h>
#include "dang.h"

DangCompileFlags dang_compile_flags_void = DANG_COMPILE_FLAGS_VOID;
DangCompileFlags dang_compile_flags_rvalue_restrictive = DANG_COMPILE_FLAGS_RVALUE_RESTRICTIVE;
DangCompileFlags dang_compile_flags_rvalue_permissive = DANG_COMPILE_FLAGS_RVALUE_PERMISSIVE;
DangCompileFlags dang_compile_flags_lvalue_restrictive = DANG_COMPILE_FLAGS_LVALUE_RESTRICTIVE;
DangCompileFlags dang_compile_flags_lvalue_permissive = DANG_COMPILE_FLAGS_LVALUE_PERMISSIVE;

static inline void
dang_compile_result_init      (DangCompileResult *result,
                               DangCompileResultType type,
                               DangValueType     *value_type,
                               dang_boolean       is_lvalue,
                               dang_boolean       is_rvalue)
{
  result->any.type = type;
  result->any.return_type = value_type;
  result->any.is_lvalue = is_lvalue;
  result->any.is_rvalue = is_rvalue;
  result->any.lock_list = NULL;
}

#define DANG_COMPILE_RESULT_INIT(res, shorttype, value_type, l, r) \
  dang_compile_result_init(res,DANG_COMPILE_RESULT_##shorttype, value_type, l, r)

void dang_compile_result_init_void (DangCompileResult *result)
{
  DANG_COMPILE_RESULT_INIT(result, VOID, NULL, FALSE, FALSE);
}

void dang_compile_result_init_stack (DangCompileResult *to_init,
                                     DangValueType     *type,
                                     DangVarId          var_id,
                                     dang_boolean       was_initialized,
                                     dang_boolean       is_lvalue,
                                     dang_boolean       is_rvalue)
{
  dang_assert (type != NULL);
  DANG_COMPILE_RESULT_INIT (to_init, STACK, type, is_lvalue, is_rvalue);
  to_init->stack.var_id = var_id;
  to_init->stack.was_initialized = was_initialized;
  to_init->stack.lvalue_callback = NULL;
  to_init->stack.callback_data = NULL;
  to_init->stack.callback_data_destroy = NULL;
}

void dang_compile_result_init_pointer (DangCompileResult *to_init,
                                       DangValueType     *type,
                                       DangVarId          var_id,
                                       unsigned           offset,
                                       dang_boolean       is_lvalue,
                                       dang_boolean       is_rvalue)
{
  DANG_COMPILE_RESULT_INIT (to_init, POINTER, type, is_lvalue, is_rvalue);
  to_init->pointer.var_id = var_id;
  to_init->pointer.offset = offset;
}

void dang_compile_result_init_global (DangCompileResult *result,
                                      DangValueType     *type,
                                      DangNamespace     *ns,
                                      unsigned           offset,
                                      dang_boolean       is_lvalue,
                                      dang_boolean       is_rvalue)
{
  DANG_COMPILE_RESULT_INIT (result, GLOBAL, type, is_lvalue, is_rvalue);
  result->global.ns = ns;
  result->global.ns_offset = offset;
}

void
dang_compile_result_set_error_position_v (DangCompileResult *result,
                                    DangCodePosition   *position,
                                    const char        *format,
                                    va_list            args)
{
  char *str;
  DANG_COMPILE_RESULT_INIT (result, ERROR, NULL, FALSE, FALSE);
  str = dang_strdup_vprintf (format, args);
  if (position != NULL && position->filename != NULL)
    {
      result->error.error = dang_error_new ("%s:%u: %s",
                                            position->filename->str,
                                            position->line,
                                            str);
    }
  else
    {
      result->error.error = dang_error_new ("unknown location: %s",
                                            str);
    }
  dang_free (str);
}
void dang_compile_result_set_error (DangCompileResult *result,
                                    DangCodePosition  *pos,
                                    const char        *format,
                                    ...)
{
  va_list args;
  va_start (args, format);
  dang_compile_result_set_error_position_v (result, pos, format, args);
  va_end (args);
}

void
dang_compile_result_set_error_builder
                                   (DangCompileResult *result,
                                    DangBuilder *builder,
                                    const char        *format,
                                    ...)
{
  va_list args;
  va_start (args, format);
  dang_compile_result_set_error_position_v (result,
                                            &builder->pos,
                                            format, args);
  va_end (args);
}

void dang_compile_result_clear (DangCompileResult *result,
                                DangBuilder *builder)
{
  if (result->type == DANG_COMPILE_RESULT_STACK)
    {
      if (result->stack.callback_data_destroy != NULL)
        result->stack.callback_data_destroy (result->stack.callback_data, builder);
    }
  else if (result->type == DANG_COMPILE_RESULT_ERROR)
    {
      dang_error_unref (result->error.error);
    }
  else if (result->type == DANG_COMPILE_RESULT_LITERAL)
    {
      if (result->any.return_type->destruct != NULL
       && result->literal.value != NULL)
        result->any.return_type->destruct (result->any.return_type,
                                           result->literal.value);
      dang_free (result->literal.value);
    }
  
  while (result->any.lock_list != NULL)
    {
      DangCompileLock *kill = result->any.lock_list;
      DangCompileLock **p_at;
      result->any.lock_list = kill->next_in_result;

      for (p_at = &builder->locks; ; p_at = &(*p_at)->next_in_builder)
        {
          if (*p_at == kill)
            {
              dang_assert (*p_at != NULL);
              *p_at = (*p_at)->next_in_builder;
              break;
            }
        }
      dang_code_position_clear (&kill->cp);
      dang_free (kill);
    }
}

void
dang_compile_result_steal_locks  (DangCompileResult *dst,
                                  DangCompileResult *src)
{
  DangCompileLock *dst_last = dst->any.lock_list;
  if (dst_last == NULL)
    {
      dst->any.lock_list = src->any.lock_list;
    }
  else
    {
      while (dst_last->next_in_result)
        dst_last = dst_last->next_in_result;
      dst_last->next_in_result = src->any.lock_list;
    }
  src->any.lock_list = NULL;
}

void dang_compile_result_init_literal (DangCompileResult *to_init,
                                       DangValueType     *type,
                                       const void        *value)
{
  DANG_COMPILE_RESULT_INIT (to_init, LITERAL, type, FALSE, TRUE);
  to_init->literal.value = dang_value_copy (type, value);
}

void dang_compile_result_init_literal_take (DangCompileResult *to_init,
                                       DangValueType     *type,
                                       void              *value)
{
  DANG_COMPILE_RESULT_INIT (to_init, LITERAL, type, FALSE, TRUE);
  to_init->literal.value = value;
}

static dang_boolean
do_members_collide (DangValueType *type,
                    unsigned       a_n,
                    DangValueElement **a,
                    unsigned       b_n,
                    DangValueElement **b)
{
  unsigned n = DANG_MIN (a_n, b_n);
  unsigned i;
  DangValueType *t = type;
  for (i = 0; i < n; i++)
    {
      if (dang_value_type_is_union (t))
        {
          if (a[i] != b[i])
            return TRUE;
        }
      else
        {
          if (a[i] != b[i])
            return FALSE;
        }
      t = a[i]->info.member.member_type;
    }
  return TRUE;
}

dang_boolean
dang_compile_result_lock        (DangCompileResult *result,
                                 DangBuilder *builder,
                                 DangCodePosition    *cp,
                                 dang_boolean       is_write_lock,
                                 DangVarId          var_id,
                                 unsigned           n_member_accesses,
                                 DangValueElement  **member_accesses)

{
  DangCompileLock *lock;
  DangValueType *type = dang_builder_get_var_type (builder, var_id);
  for (lock = builder->locks; lock; lock = lock->next_in_builder)
    if (var_id == lock->var
     && (is_write_lock || lock->is_write_lock)
     && do_members_collide (type,
                            lock->n_member_accesses, lock->member_accesses,
                            n_member_accesses, member_accesses))
      {
        /* FAIL */
        DangStringBuffer s = DANG_STRING_BUFFER_INIT;
        const char *name = dang_builder_get_var_name (builder, var_id);
        unsigned i;
        if (name == NULL)
          name = "*unnamed_tmp*";
        dang_string_buffer_printf (&s, "aliasing error %s", name);
        for (i = 0; i < lock->n_member_accesses; i++)
          dang_string_buffer_printf (&s, ".%s", lock->member_accesses[i]->name);
        dang_string_buffer_printf (&s, " for %s at "DANG_CP_FORMAT" conflicts with access of %s",
                                   lock->is_write_lock ? "writing" : "reading",
                                   DANG_CP_ARGS (lock->cp),
                                   name);
        for (i = 0; i < lock->n_member_accesses; i++)
          dang_string_buffer_printf (&s, ".%s", member_accesses[i]->name);
        dang_string_buffer_printf (&s, " for %s",
                                   is_write_lock ? "writing" : "reading");
        dang_compile_result_clear (result, builder);
        dang_compile_result_set_error (result, cp, "%s", s.str);
        dang_free (s.str);
        return FALSE;
      }


  lock = dang_malloc (sizeof (DangCompileLock) + sizeof(DangValueElement*) * n_member_accesses);
  lock->var = var_id;
  lock->is_write_lock = is_write_lock;
  lock->n_member_accesses = n_member_accesses;
  lock->next_in_result = result->any.lock_list;
  result->any.lock_list = lock;
  lock->next_in_builder = builder->locks;
  dang_code_position_copy (&lock->cp, cp);
  builder->locks = lock;
  memcpy (lock->member_accesses, member_accesses,
          sizeof(DangValueElement*) * n_member_accesses);
  return TRUE;
}
