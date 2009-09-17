#include "dang.h"

static void
handle_deref_by_moving_onto_stack (DangBuilder *builder,
                                  DangCompileResult *cur_res,
                                  DangValueMember   *member,
                                  DangCompileFlags  *flags,
                                  DangCompileResult *new_res)
{
  DangVarId new_var_id;
  DangCompileResult res;
  dang_assert (member->type == DANG_VALUE_MEMBER_TYPE_SIMPLE
           &&  member->info.simple.dereference);

  /* Allocate a new variable and initialize it. */
  new_var_id = dang_builder_add_tmp (builder, cur_res->any.return_type);
  dang_compile_result_init_stack (&res, cur_res->any.return_type, new_var_id,
                                  FALSE, TRUE, FALSE);
  dang_builder_add_assign (builder, &res, cur_res);

  dang_compile_result_init_pointer (new_res, member->member_type,
                                    new_var_id, member->info.simple.offset,
                                    flags->must_be_lvalue,
                                    flags->must_be_rvalue);
  dang_compile_result_clear (&res, builder);
  /* TODO */
  //dang_compile_obey_flags (flags, new_res);
}

/* NOTE: takes responsibility for cleanup cur_res */
void
dang_compile_member_access (DangBuilder *builder, 
                            DangCompileResult *cur_res,
                            DangValueType     *member_type,
                            const char        *member_name,
                            DangValueMember   *member,
                            DangCompileFlags  *flags,
                            DangCompileResult *new_res)
{
  DangError *error = NULL;
  dang_assert (member != NULL);
  dang_assert (cur_res->any.return_type != NULL);
  if (!dang_builder_check_member_access (builder, member_type,
                                                  member->flags,
                                                  flags->must_be_rvalue,
                                                  flags->must_be_lvalue,
                                                  &error))
    {
      dang_compile_result_set_error (new_res, &builder->pos,
                                     "error accessing %s.%s: %s",
                                     member_type->full_name, member_name,
                                     error->message);
      dang_error_unref (error);
      return;
    }
  switch (member->type)
    {
    case DANG_VALUE_MEMBER_TYPE_SIMPLE:
      switch (cur_res->type)
        {
        case DANG_COMPILE_RESULT_STACK:
          if (member->info.simple.dereference)
            {
              dang_compile_result_init_pointer (new_res, member->member_type,
                                                cur_res->stack.var_id,
                                                member->info.simple.offset,
                                                flags->must_be_lvalue,
                                                flags->must_be_rvalue);
            }
          else
            {
              DangVarId id = dang_builder_add_local_alias (builder,
                                                                    cur_res->stack.var_id,
                                                                    member->info.simple.offset,
                                                                    member->member_type);
              dang_compile_result_force_initialize (builder, cur_res);
              dang_builder_note_var_create (builder, id);
              dang_compile_result_init_stack (new_res, member->member_type, id,
                                              TRUE,
                                              cur_res->any.is_lvalue,
                                              cur_res->any.is_rvalue);
            }
          break;
        case DANG_COMPILE_RESULT_POINTER:
          if (member->info.simple.dereference)
            {
              handle_deref_by_moving_onto_stack (builder, cur_res, member, flags, new_res);
            }
          else
            {
              dang_compile_result_init_pointer (new_res, member->member_type,
                                                cur_res->pointer.var_id,
                                                cur_res->pointer.offset + member->info.simple.offset,
                                                cur_res->any.is_lvalue,
                                                cur_res->any.is_rvalue);
            }
          break;
        case DANG_COMPILE_RESULT_GLOBAL:
          if (member->info.simple.dereference)
            {
              handle_deref_by_moving_onto_stack (builder, cur_res, member, flags, new_res);
            }
          else
            {
              dang_compile_result_init_global (new_res, member->member_type,
                                               cur_res->global.ns,
                                               cur_res->global.ns_offset + member->info.simple.offset,
                                               cur_res->any.is_lvalue,
                                               cur_res->any.is_rvalue);
            }
          break;
        case DANG_COMPILE_RESULT_LITERAL:
          {
            char *value;
            if (member->info.simple.dereference)
              {
                void *ptr = * (void**) cur_res->literal.value;
                if (ptr == NULL)
                  {
                    dang_compile_result_set_error (new_res, &builder->pos,
                                                   "attempted to dereference a NULL literal");
                    return;
                  }
                value = (char*)ptr + member->info.simple.offset;
              }
            else
              {
                value = (char*) cur_res->literal.value + member->info.simple.offset;
              }
            dang_compile_result_init_literal (new_res, member->member_type, value);
            break;
          }
        default:
          dang_assert_not_reached ();
        }
      dang_compile_obey_flags (builder, flags, new_res);
      dang_compile_result_steal_locks (new_res, cur_res);
      dang_compile_result_clear (cur_res, builder);
      break;
    case DANG_VALUE_MEMBER_TYPE_VIRTUAL:
      if ((member->flags & DANG_MEMBER_PUBLIC_READABLE) == 0)
        {
          dang_compile_result_set_error (new_res, &builder->pos,
                                         "attempted to read a write-only member (%s) (of type %s)",
                                         member_name,
                                         cur_res->any.return_type->full_name);
          return;
        }
      member->info.virt.compile (builder, cur_res, member->info.virt.member_data, flags, new_res);
      return;
    default:
      dang_assert_not_reached ();
    }
}

#ifdef DANG_DEBUG
void _dang_compile_member_access__register_debug ()
{
}
#endif
