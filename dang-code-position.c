#include "dang.h"

void dang_code_position_init (DangCodePosition *code_position)
{
  code_position->line = 0;
  code_position->filename = NULL;
}

void dang_code_position_copy (DangCodePosition *target,
                              DangCodePosition *source)
{
  *target = *source;
  if (source->filename)
    target->filename = dang_string_ref_copy (source->filename);
}

void dang_code_position_clear(DangCodePosition *code_position)
{
  if (code_position->filename)
    dang_string_unref (code_position->filename);
}
