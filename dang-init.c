#include <math.h>
#include <ctype.h>
#include "dang.h"
#include "config.h"
#include <stdio.h>
#include <string.h>

/* TODO: define these guys */
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)

#define add_simple dang_namespace_add_simple_c_from_params

/* --- simple, non-lazy well-typed functions --- */
static dang_boolean
do_assert         (void      **args,
                   void       *rv_out,
                   void       *func_data,
                   DangError **error)
{
  DANG_UNUSED (rv_out);
  DANG_UNUSED (func_data);
  if (*(char*)(args[0]) == 0)
    {
      dang_set_error (error, "assertion failed");
      return FALSE;
    }
  return TRUE;
}

static dang_boolean
do_system_println (void      **args,
                   void       *rv_out,
                   void       *func_data,
                   DangError **error)
{
  DangString *str = * (DangString **) (args[0]);
  if (str)
    fputs (str->str, stdout);
  fputc ('\n', stdout);

  DANG_UNUSED (rv_out);
  DANG_UNUSED (error);
  DANG_UNUSED (func_data);

  return TRUE;
}
static dang_boolean
do_system_abort   (void      **args,
                   void       *rv_out,
                   void       *func_data,
                   DangError **error)
{
  DangString *str = * (DangString **) (args[0]);

  DANG_UNUSED (rv_out);
  DANG_UNUSED (func_data);

  *error = dang_error_new ("%s", str ? str->str : "(null)");

  return FALSE;
}

//static DANG_SIMPLE_C_FUNC_DECLARE(do_string_length)
//{
//  DangString *str = * (DangString **) args[0];
//  DANG_UNUSED (func_data);
//  DANG_UNUSED (error);
//  if (str == NULL)
//    {
//#ifdef DANG_NPE_ON_NULL_STRINGS
//      dang_set_error (error, "null-pointer exception in length(string)");
//      return FALSE;
//#else
//      * (uint32_t *) rv_out = 0;
//      return TRUE;
//#endif
//    }
//  * (uint32_t *) rv_out = str->len;
//  return TRUE;
//}

static DANG_SIMPLE_C_FUNC_DECLARE (do_operator_not)
{
  DANG_UNUSED (error); DANG_UNUSED (func_data);
  * (char*)rv_out = ! (*(char*)(args[0]));
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE (do_operator_boolean_equal)
{
  DANG_UNUSED (error); DANG_UNUSED (func_data);
  * (char*)rv_out = (*(char*)(args[0])) == (*(char*)(args[1]));
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE (do_operator_boolean_notequal)
{
  DANG_UNUSED (error); DANG_UNUSED (func_data);
  /* XXX: would XOR be faster? */
  * (char*)rv_out = (*(char*)(args[0])) != (*(char*)(args[1]));
  return TRUE;
}

typedef int8_t     dang_ctype_int8;
typedef int16_t    dang_ctype_int16;
typedef int32_t    dang_ctype_int32;
typedef int64_t    dang_ctype_int64;
typedef uint8_t    dang_ctype_uint8;
typedef uint16_t   dang_ctype_uint16;
typedef uint32_t   dang_ctype_uint32;
typedef uint64_t   dang_ctype_uint64;
typedef float      dang_ctype_float;
typedef double     dang_ctype_double;
typedef uint32_t   dang_ctype_char;

#define DEFINE_CAST_FUNCTION(to_type, from_type)    \
static dang_boolean                                                       \
operator_cast__to_##to_type##__from_##from_type (void **args,             \
                                              void *rv,                   \
                                              void *func_data,            \
                                              DangError **error)          \
{                                                                         \
  DANG_UNUSED (func_data); DANG_UNUSED (error);                           \
  * (dang_ctype_##to_type*) rv = * (dang_ctype_##from_type*) args[0];     \
  return TRUE;                                                            \
}
#if 0
types="int8 int16 int32 int64 uint8 uint16 uint32 uint64 float double"
for a in $types; do for b in $types; do test "$a" = "$b" || echo "DEFINE_CAST_FUNCTION($a, $b)" ; done ; done
#endif
DEFINE_CAST_FUNCTION(int8, int8)
DEFINE_CAST_FUNCTION(int8, int16)
DEFINE_CAST_FUNCTION(int8, int32)
DEFINE_CAST_FUNCTION(int8, int64)
DEFINE_CAST_FUNCTION(int8, uint8)
DEFINE_CAST_FUNCTION(int8, uint16)
DEFINE_CAST_FUNCTION(int8, uint32)
DEFINE_CAST_FUNCTION(int8, uint64)
DEFINE_CAST_FUNCTION(int8, float)
DEFINE_CAST_FUNCTION(int8, double)
DEFINE_CAST_FUNCTION(int8, char)
DEFINE_CAST_FUNCTION(int16, int8)
DEFINE_CAST_FUNCTION(int16, int16)
DEFINE_CAST_FUNCTION(int16, int32)
DEFINE_CAST_FUNCTION(int16, int64)
DEFINE_CAST_FUNCTION(int16, uint8)
DEFINE_CAST_FUNCTION(int16, uint16)
DEFINE_CAST_FUNCTION(int16, uint32)
DEFINE_CAST_FUNCTION(int16, uint64)
DEFINE_CAST_FUNCTION(int16, float)
DEFINE_CAST_FUNCTION(int16, double)
DEFINE_CAST_FUNCTION(int16, char)
DEFINE_CAST_FUNCTION(int32, int8)
DEFINE_CAST_FUNCTION(int32, int16)
DEFINE_CAST_FUNCTION(int32, int32)
DEFINE_CAST_FUNCTION(int32, int64)
DEFINE_CAST_FUNCTION(int32, uint8)
DEFINE_CAST_FUNCTION(int32, uint16)
DEFINE_CAST_FUNCTION(int32, uint32)
DEFINE_CAST_FUNCTION(int32, uint64)
DEFINE_CAST_FUNCTION(int32, float)
DEFINE_CAST_FUNCTION(int32, double)
DEFINE_CAST_FUNCTION(int32, char)
DEFINE_CAST_FUNCTION(int64, int8)
DEFINE_CAST_FUNCTION(int64, int16)
DEFINE_CAST_FUNCTION(int64, int32)
DEFINE_CAST_FUNCTION(int64, int64)
DEFINE_CAST_FUNCTION(int64, uint8)
DEFINE_CAST_FUNCTION(int64, uint16)
DEFINE_CAST_FUNCTION(int64, uint32)
DEFINE_CAST_FUNCTION(int64, uint64)
DEFINE_CAST_FUNCTION(int64, float)
DEFINE_CAST_FUNCTION(int64, double)
DEFINE_CAST_FUNCTION(int64, char)
DEFINE_CAST_FUNCTION(uint8, int8)
DEFINE_CAST_FUNCTION(uint8, int16)
DEFINE_CAST_FUNCTION(uint8, int32)
DEFINE_CAST_FUNCTION(uint8, int64)
DEFINE_CAST_FUNCTION(uint8, uint8)
DEFINE_CAST_FUNCTION(uint8, uint16)
DEFINE_CAST_FUNCTION(uint8, uint32)
DEFINE_CAST_FUNCTION(uint8, uint64)
DEFINE_CAST_FUNCTION(uint8, float)
DEFINE_CAST_FUNCTION(uint8, double)
DEFINE_CAST_FUNCTION(uint8, char)
DEFINE_CAST_FUNCTION(uint16, int8)
DEFINE_CAST_FUNCTION(uint16, int16)
DEFINE_CAST_FUNCTION(uint16, int32)
DEFINE_CAST_FUNCTION(uint16, int64)
DEFINE_CAST_FUNCTION(uint16, uint8)
DEFINE_CAST_FUNCTION(uint16, uint16)
DEFINE_CAST_FUNCTION(uint16, uint32)
DEFINE_CAST_FUNCTION(uint16, uint64)
DEFINE_CAST_FUNCTION(uint16, float)
DEFINE_CAST_FUNCTION(uint16, double)
DEFINE_CAST_FUNCTION(uint16, char)
DEFINE_CAST_FUNCTION(uint32, int8)
DEFINE_CAST_FUNCTION(uint32, int16)
DEFINE_CAST_FUNCTION(uint32, int32)
DEFINE_CAST_FUNCTION(uint32, int64)
DEFINE_CAST_FUNCTION(uint32, uint8)
DEFINE_CAST_FUNCTION(uint32, uint16)
DEFINE_CAST_FUNCTION(uint32, uint32)
DEFINE_CAST_FUNCTION(uint32, uint64)
DEFINE_CAST_FUNCTION(uint32, float)
DEFINE_CAST_FUNCTION(uint32, double)
DEFINE_CAST_FUNCTION(uint32, char)
DEFINE_CAST_FUNCTION(uint64, int8)
DEFINE_CAST_FUNCTION(uint64, int16)
DEFINE_CAST_FUNCTION(uint64, int32)
DEFINE_CAST_FUNCTION(uint64, int64)
DEFINE_CAST_FUNCTION(uint64, uint8)
DEFINE_CAST_FUNCTION(uint64, uint16)
DEFINE_CAST_FUNCTION(uint64, uint32)
DEFINE_CAST_FUNCTION(uint64, uint64)
DEFINE_CAST_FUNCTION(uint64, float)
DEFINE_CAST_FUNCTION(uint64, double)
DEFINE_CAST_FUNCTION(uint64, char)
DEFINE_CAST_FUNCTION(float, int8)
DEFINE_CAST_FUNCTION(float, int16)
DEFINE_CAST_FUNCTION(float, int32)
DEFINE_CAST_FUNCTION(float, int64)
DEFINE_CAST_FUNCTION(float, uint8)
DEFINE_CAST_FUNCTION(float, uint16)
DEFINE_CAST_FUNCTION(float, uint32)
DEFINE_CAST_FUNCTION(float, uint64)
DEFINE_CAST_FUNCTION(float, float)
DEFINE_CAST_FUNCTION(float, double)
DEFINE_CAST_FUNCTION(float, char)
DEFINE_CAST_FUNCTION(double, int8)
DEFINE_CAST_FUNCTION(double, int16)
DEFINE_CAST_FUNCTION(double, int32)
DEFINE_CAST_FUNCTION(double, int64)
DEFINE_CAST_FUNCTION(double, uint8)
DEFINE_CAST_FUNCTION(double, uint16)
DEFINE_CAST_FUNCTION(double, uint32)
DEFINE_CAST_FUNCTION(double, uint64)
DEFINE_CAST_FUNCTION(double, float)
DEFINE_CAST_FUNCTION(double, double)
DEFINE_CAST_FUNCTION(double, char)
DEFINE_CAST_FUNCTION(char, int8)
DEFINE_CAST_FUNCTION(char, int16)
DEFINE_CAST_FUNCTION(char, int32)
DEFINE_CAST_FUNCTION(char, int64)
DEFINE_CAST_FUNCTION(char, uint8)
DEFINE_CAST_FUNCTION(char, uint16)
DEFINE_CAST_FUNCTION(char, uint32)
DEFINE_CAST_FUNCTION(char, uint64)
DEFINE_CAST_FUNCTION(char, float)
DEFINE_CAST_FUNCTION(char, double)
DEFINE_CAST_FUNCTION(char, char)

#define DEFINE_CMP_FUNCTION(func_name, argtype)                           \
static DANG_SIMPLE_C_FUNC_DECLARE(func_name)                              \
{                                                                         \
  argtype a = * (argtype *) args[0];                                      \
  argtype b = * (argtype *) args[1];                                      \
  * (int32_t *) rv_out = (a < b) ? -1 : (a > b) ? 1 : 0;                  \
  DANG_UNUSED (func_data); DANG_UNUSED (error);                           \
  return TRUE;                                                            \
}
#define DEFINE_BINOP_FUNCTION(func_name, rettype, arg1type, op, arg2type) \
static DANG_SIMPLE_C_FUNC_DECLARE(func_name)                              \
{                                                                         \
  DANG_UNUSED (func_data); DANG_UNUSED (error);                           \
  * (rettype*) rv_out = (* (arg1type*) args[0]) op (* (arg2type*) args[1]);\
  return TRUE;                                                            \
}
#define DEFINE_NEGATE(func_name, argtype)                                 \
static DANG_SIMPLE_C_FUNC_DECLARE(func_name)                              \
{                                                                         \
  DANG_UNUSED (func_data); DANG_UNUSED (error);                           \
  * (argtype *) rv_out = - (* (argtype*) args[0]);                        \
  return TRUE;                                                            \
}
#define DEFINE_NOT(func_name, argtype)                                    \
static DANG_SIMPLE_C_FUNC_DECLARE(func_name)                              \
{                                                                         \
  DANG_UNUSED (func_data); DANG_UNUSED (error);                           \
  * (char*) rv_out = (* (argtype*) args[0]) == 0;                         \
  return TRUE;                                                            \
}
#define DEFINE_BITWISE_COMPLEMENT(func_name, argtype)                     \
static DANG_SIMPLE_C_FUNC_DECLARE(func_name)                              \
{                                                                         \
  DANG_UNUSED (func_data); DANG_UNUSED (error);                           \
  * (argtype*) rv_out = ~ (* (argtype*) args[0]);                         \
  return TRUE;                                                            \
}
#define DEFINE_ASSIGN_BINOP_FUNCTION(func_name, arg1type, op, arg2type)   \
static DANG_SIMPLE_C_FUNC_DECLARE(func_name)                              \
{                                                                         \
  DANG_UNUSED (func_data); DANG_UNUSED (error);                           \
  DANG_UNUSED (rv_out);                                                       \
  (* (arg1type*) args[0]) op (* (arg2type*) args[1]);                     \
  return TRUE;                                                            \
}
#define DEFINE_DIVLIKE_FUNCTION(func_name, type, op)                      \
static DANG_SIMPLE_C_FUNC_DECLARE(func_name)                              \
{                                                                         \
  DANG_UNUSED (func_data);                                                \
  if (*(type*) args[1] == 0)                                              \
    {                                                                     \
       dang_set_error (error, "'%s' by zero", #op);                       \
       return FALSE;                                                      \
    }                                                                     \
  * (type*) rv_out = (* (type*) args[0]) op (* (type*) args[1]);              \
  return TRUE;                                                            \
}
#define DEFINE_ASSIGN_DIVLIKE_FUNCTION(func_name, type, op)               \
static DANG_SIMPLE_C_FUNC_DECLARE(func_name)                              \
{                                                                         \
  DANG_UNUSED (func_data);                                                \
  DANG_UNUSED (rv_out);                                                       \
  if (*(type*) args[1] == 0)                                              \
    {                                                                     \
       dang_set_error (error, "'%s' by zero", #op);                       \
       return FALSE;                                                      \
    }                                                                     \
  (* (type*) args[0]) op (* (type*) args[1]);                             \
  return TRUE;                                                            \
}

#define DEFINE_FMOD_FUNCTION(func_name, type)                             \
static DANG_SIMPLE_C_FUNC_DECLARE(func_name)                              \
{                                                                         \
  DANG_UNUSED (func_data);                                                \
  if (*(type*) args[1] == 0)                                              \
    {                                                                     \
       dang_set_error (error, "'%%' by zero");                            \
       return FALSE;                                                      \
    }                                                                     \
  * (type*) rv_out = fmod (* (type*) args[0], * (type*) args[1]);         \
  return TRUE;                                                            \
}
#define DEFINE_ASSIGN_FMOD_FUNCTION(func_name, type)                      \
static DANG_SIMPLE_C_FUNC_DECLARE(func_name)                              \
{                                                                         \
  DANG_UNUSED (func_data);                                                \
  DANG_UNUSED (rv_out);                                                   \
  if (*(type*) args[1] == 0)                                              \
    {                                                                     \
       dang_set_error (error, "'%%' by zero");                            \
       return FALSE;                                                      \
    }                                                                     \
  * (type*) args[0] = fmod (* (type*) args[0], * (type*) args[1]);        \
  return TRUE;                                                            \
}

#define ORDER_MACRO_PRE(a,b) a;b;
#define ORDER_MACRO_POST(a,b) b;a;

#define DEFINE_INCR_OP_FUNCTION(func_name, type, order_macro, OP)         \
static DANG_SIMPLE_C_FUNC_DECLARE(func_name)                              \
{                                                                         \
  type *v = args[0];                                                      \
  type *r = rv_out;                                                       \
  DANG_UNUSED (func_data); DANG_UNUSED (error);                           \
  order_macro(*v OP 1, *r = *v)                                           \
  return TRUE;                                                            \
}

#define STD_NUMERIC_OPS(type, ctype)                                                   \
DEFINE_BINOP_FUNCTION(do_operator_lessthan_##type, char, ctype, <, ctype)              \
DEFINE_BINOP_FUNCTION(do_operator_lesseq_##type, char, ctype, <=, ctype)               \
DEFINE_BINOP_FUNCTION(do_operator_greaterthan_##type, char, ctype, >, ctype)           \
DEFINE_BINOP_FUNCTION(do_operator_greatereq_##type, char, ctype, >=, ctype)            \
DEFINE_BINOP_FUNCTION(do_operator_equal_##type, char, ctype, ==, ctype)                \
DEFINE_BINOP_FUNCTION(do_operator_notequal_##type, char, ctype, !=, ctype)             \
DEFINE_BINOP_FUNCTION(do_operator_add_##type, ctype, ctype, +, ctype)                  \
DEFINE_BINOP_FUNCTION(do_operator_multiply_##type, ctype, ctype, *, ctype)             \
DEFINE_BINOP_FUNCTION(do_operator_subtract_##type, ctype, ctype, -, ctype)             \
DEFINE_ASSIGN_BINOP_FUNCTION(do_operator_assign_add_##type, ctype, +=, ctype)          \
DEFINE_ASSIGN_BINOP_FUNCTION(do_operator_assign_multiply_##type, ctype, *=, ctype)     \
DEFINE_ASSIGN_BINOP_FUNCTION(do_operator_assign_subtract_##type, ctype, -=, ctype)     \
DEFINE_CMP_FUNCTION(do_operator_cmp_##type, ctype)                                     \
DEFINE_INCR_OP_FUNCTION(do_operator_preincrement_##type, ctype, ORDER_MACRO_PRE, +=)   \
DEFINE_INCR_OP_FUNCTION(do_operator_postincrement_##type, ctype, ORDER_MACRO_POST, +=) \
DEFINE_INCR_OP_FUNCTION(do_operator_predecrement_##type, ctype, ORDER_MACRO_PRE, -=)   \
DEFINE_INCR_OP_FUNCTION(do_operator_postdecrement_##type, ctype, ORDER_MACRO_POST, -=) \
DEFINE_NEGATE(do_operator_negate_##type, ctype)                                        \
DEFINE_NOT(do_operator_not_##type, ctype)                                              \
DEFINE_DIVLIKE_FUNCTION(do_operator_divide_##type, ctype, /)                           \
DEFINE_ASSIGN_DIVLIKE_FUNCTION(do_operator_assign_divide_##type, ctype, /=)

#define STD_BITWISE_OPS(type, ctype)                                                   \
DEFINE_BINOP_FUNCTION(do_operator_bitwise_and_##type, ctype, ctype, &, ctype)          \
DEFINE_BINOP_FUNCTION(do_operator_bitwise_or_##type, ctype, ctype, |, ctype)           \
DEFINE_BINOP_FUNCTION(do_operator_bitwise_xor_##type, ctype, ctype, ^, ctype)          \
DEFINE_BINOP_FUNCTION(do_operator_left_shift_##type, ctype, ctype, <<, uint32_t)       \
DEFINE_BINOP_FUNCTION(do_operator_right_shift_##type, ctype, ctype, >>, uint32_t)      \
DEFINE_ASSIGN_BINOP_FUNCTION(do_operator_assign_bitwise_and_##type, ctype, &=, ctype)  \
DEFINE_ASSIGN_BINOP_FUNCTION(do_operator_assign_bitwise_or_##type, ctype, |=, ctype)   \
DEFINE_ASSIGN_BINOP_FUNCTION(do_operator_assign_bitwise_xor_##type, ctype, ^=, ctype)  \
DEFINE_ASSIGN_BINOP_FUNCTION(do_operator_assign_left_shift_##type, ctype, <<=, ctype)  \
DEFINE_ASSIGN_BINOP_FUNCTION(do_operator_assign_right_shift_##type, ctype, >>=, ctype) \
DEFINE_BITWISE_COMPLEMENT(do_operator_bitwise_complement_##type, ctype)

#define RIGHT_SHIFT_OPS(type, ctype)                                                   \
DEFINE_BINOP_FUNCTION(do_operator_right_shift_##type, ctype, ctype, >>, uint32_t)      \
DEFINE_ASSIGN_BINOP_FUNCTION(do_operator_assign_right_shift_##type, ctype, >>=, ctype)

STD_NUMERIC_OPS(int8, int8_t)
STD_NUMERIC_OPS(uint8, uint8_t)
STD_NUMERIC_OPS(int16, int16_t)
STD_NUMERIC_OPS(uint16, uint16_t)
STD_NUMERIC_OPS(int32, int32_t)
STD_NUMERIC_OPS(uint32, uint32_t)
STD_NUMERIC_OPS(int64, int64_t)
STD_NUMERIC_OPS(uint64, uint64_t)
STD_NUMERIC_OPS(float, float)
STD_NUMERIC_OPS(double, double)

DEFINE_DIVLIKE_FUNCTION(do_operator_mod_int8,  int8_t, %)
DEFINE_DIVLIKE_FUNCTION(do_operator_mod_int16, int16_t, %)
DEFINE_DIVLIKE_FUNCTION(do_operator_mod_int32, int32_t, %)
DEFINE_DIVLIKE_FUNCTION(do_operator_mod_int64, int64_t, %)
DEFINE_DIVLIKE_FUNCTION(do_operator_mod_uint8,  uint8_t, %)
DEFINE_DIVLIKE_FUNCTION(do_operator_mod_uint16, uint16_t, %)
DEFINE_DIVLIKE_FUNCTION(do_operator_mod_uint32, uint32_t, %)
DEFINE_DIVLIKE_FUNCTION(do_operator_mod_uint64, uint64_t, %)
DEFINE_ASSIGN_DIVLIKE_FUNCTION(do_operator_assign_mod_int8,  int8_t, %=)
DEFINE_ASSIGN_DIVLIKE_FUNCTION(do_operator_assign_mod_int16, int16_t, %=)
DEFINE_ASSIGN_DIVLIKE_FUNCTION(do_operator_assign_mod_int32, int32_t, %=)
DEFINE_ASSIGN_DIVLIKE_FUNCTION(do_operator_assign_mod_int64, int64_t, %=)
DEFINE_ASSIGN_DIVLIKE_FUNCTION(do_operator_assign_mod_uint8,  uint8_t, %=)
DEFINE_ASSIGN_DIVLIKE_FUNCTION(do_operator_assign_mod_uint16, uint16_t, %=)
DEFINE_ASSIGN_DIVLIKE_FUNCTION(do_operator_assign_mod_uint32, uint32_t, %=)
DEFINE_ASSIGN_DIVLIKE_FUNCTION(do_operator_assign_mod_uint64, uint64_t, %=)
DEFINE_FMOD_FUNCTION(do_operator_mod_float, float);
DEFINE_FMOD_FUNCTION(do_operator_mod_double, double);
DEFINE_ASSIGN_FMOD_FUNCTION(do_operator_assign_mod_float, float);
DEFINE_ASSIGN_FMOD_FUNCTION(do_operator_assign_mod_double, double);
STD_BITWISE_OPS(uint8, uint8_t)
STD_BITWISE_OPS(uint16, uint16_t)
STD_BITWISE_OPS(uint32, uint32_t)
STD_BITWISE_OPS(uint64, uint64_t)

/* Bitwise operators on signed integers are basically the same
   as those on unsigned, except for ">>" and ">>=", which must
   do sign-extension. */
#define do_operator_bitwise_complement_int8 do_operator_bitwise_complement_uint8
#define do_operator_bitwise_and_int8        do_operator_bitwise_and_uint8
#define do_operator_bitwise_or_int8         do_operator_bitwise_or_uint8
#define do_operator_bitwise_xor_int8        do_operator_bitwise_xor_uint8
#define do_operator_left_shift_int8         do_operator_left_shift_uint8
#define do_operator_assign_bitwise_and_int8 do_operator_assign_bitwise_and_uint8
#define do_operator_assign_bitwise_or_int8  do_operator_assign_bitwise_or_uint8
#define do_operator_assign_bitwise_xor_int8 do_operator_assign_bitwise_xor_uint8
#define do_operator_assign_left_shift_int8  do_operator_assign_left_shift_uint8
RIGHT_SHIFT_OPS(int8, int8_t)

#define do_operator_bitwise_complement_int16 do_operator_bitwise_complement_uint16
#define do_operator_bitwise_and_int16        do_operator_bitwise_and_uint16
#define do_operator_bitwise_or_int16         do_operator_bitwise_or_uint16
#define do_operator_bitwise_xor_int16        do_operator_bitwise_xor_uint16
#define do_operator_left_shift_int16         do_operator_left_shift_uint16
#define do_operator_assign_bitwise_and_int16 do_operator_assign_bitwise_and_uint16
#define do_operator_assign_bitwise_or_int16  do_operator_assign_bitwise_or_uint16
#define do_operator_assign_bitwise_xor_int16 do_operator_assign_bitwise_xor_uint16
#define do_operator_assign_left_shift_int16  do_operator_assign_left_shift_uint16
RIGHT_SHIFT_OPS(int16, int16_t)

#define do_operator_bitwise_complement_int32 do_operator_bitwise_complement_uint32
#define do_operator_bitwise_and_int32        do_operator_bitwise_and_uint32
#define do_operator_bitwise_or_int32         do_operator_bitwise_or_uint32
#define do_operator_bitwise_xor_int32        do_operator_bitwise_xor_uint32
#define do_operator_left_shift_int32         do_operator_left_shift_uint32
#define do_operator_assign_bitwise_and_int32 do_operator_assign_bitwise_and_uint32
#define do_operator_assign_bitwise_or_int32  do_operator_assign_bitwise_or_uint32
#define do_operator_assign_bitwise_xor_int32 do_operator_assign_bitwise_xor_uint32
#define do_operator_assign_left_shift_int32  do_operator_assign_left_shift_uint32
RIGHT_SHIFT_OPS(int32, int32_t)

#define do_operator_bitwise_complement_int64 do_operator_bitwise_complement_uint64
#define do_operator_bitwise_and_int64        do_operator_bitwise_and_uint64
#define do_operator_bitwise_or_int64         do_operator_bitwise_or_uint64
#define do_operator_bitwise_xor_int64        do_operator_bitwise_xor_uint64
#define do_operator_left_shift_int64         do_operator_left_shift_uint64
#define do_operator_assign_bitwise_and_int64 do_operator_assign_bitwise_and_uint64
#define do_operator_assign_bitwise_or_int64  do_operator_assign_bitwise_or_uint64
#define do_operator_assign_bitwise_xor_int64 do_operator_assign_bitwise_xor_uint64
#define do_operator_assign_left_shift_int64  do_operator_assign_left_shift_uint64
RIGHT_SHIFT_OPS(int64, int64_t)
   

#define do_operator_lessthan_char      do_operator_lessthan_uint32
#define do_operator_lesseq_char        do_operator_lesseq_uint32
#define do_operator_greaterthan_char   do_operator_greaterthan_uint32
#define do_operator_greatereq_char     do_operator_greatereq_uint32
#define do_operator_equal_char         do_operator_equal_uint32
#define do_operator_notequal_char      do_operator_notequal_uint32
#define do_operator_cmp_char           do_operator_cmp_uint32

#define DEFINE_NUMBER_TO_STRING_FUNCTION(type, ctype, format_str) \
static dang_boolean                                               \
do_to_string_##type (void **args,                                 \
                     void *rv,                                    \
                     void *func_data,                             \
                     DangError **error)                           \
{                                                                 \
  char buf[128];                                                  \
  DANG_UNUSED (error); DANG_UNUSED (func_data);                   \
  snprintf(buf, sizeof(buf), format_str, *(ctype*) (args[0]));    \
  buf[sizeof(buf)-1] = 0;                                         \
  *(DangString**)rv = dang_string_new (buf);                      \
  return TRUE;                                                    \
}
DEFINE_NUMBER_TO_STRING_FUNCTION(int8, int8_t, "%"PRIi8);
DEFINE_NUMBER_TO_STRING_FUNCTION(uint8, uint8_t, "%"PRIu8);
DEFINE_NUMBER_TO_STRING_FUNCTION(int16, int16_t, "%"PRIi16);
DEFINE_NUMBER_TO_STRING_FUNCTION(uint16, uint16_t, "%"PRIu16);
DEFINE_NUMBER_TO_STRING_FUNCTION(int32, int32_t, "%"PRIi32);
DEFINE_NUMBER_TO_STRING_FUNCTION(uint32, uint32_t, "%"PRIu32);
DEFINE_NUMBER_TO_STRING_FUNCTION(int64, int64_t, "%"PRIi64);
DEFINE_NUMBER_TO_STRING_FUNCTION(uint64, uint64_t, "%"PRIu64);
DEFINE_NUMBER_TO_STRING_FUNCTION(float, float, "%.6f");
DEFINE_NUMBER_TO_STRING_FUNCTION(double, double, "%.9f");
static dang_boolean                                               \
do_to_string_boolean(void **args,
                     void *rv,
                     void *func_data,
                     DangError **error)
{
  DANG_UNUSED (error); DANG_UNUSED (func_data);
  *(DangString**)rv = dang_string_ref_copy (dang_string_peek_boolean (*(char*)(args[0])));
  return TRUE;
}

static dang_boolean
do_to_string_string (void **args,
                     void *rv,
                     void *func_data,
                     DangError **error)
{
  DangString *in = * (DangString**) args[0];
  DANG_UNUSED (error); DANG_UNUSED (func_data);
  *(DangString**)rv = in ? dang_string_ref_copy (in) : dang_string_new ("(null)");
  return TRUE;
}

static dang_boolean
do_to_string_char (void **args,
                     void *rv,
                     void *func_data,
                     DangError **error)
{
  uint32_t t = * (const uint32_t *) args[0];
  char buf[6];
  unsigned len = dang_utf8_encode (t, buf);
  DANG_UNUSED (error); DANG_UNUSED (func_data);
  *(DangString**)rv = dang_string_new_len (buf, len);
  return TRUE;
}


/* --- string operators --- */
static dang_boolean
do_operator_lessthan_string  (void **args,
                              void *rv,
                              void *func_data,
                              DangError **error)
{
  DangString *a = * (DangString **) (args[0]);
  DangString *b = * (DangString **) (args[1]);
  DANG_UNUSED (func_data); DANG_UNUSED (error);
  if (a == NULL)
    {
      if (b == NULL)
        * (char*) rv = 0;
      else
        * (char*) rv = 1;
    }
  else
    {
      if (b == NULL)
        * (char*) rv = 0;
      else
        * (char*) rv = (strcmp(a->str, b->str) < 0);
    }
  return TRUE;
}
static dang_boolean
do_operator_lesseq_string  (void **args,
                              void *rv,
                              void *func_data,
                              DangError **error)
{
  DangString *a = * (DangString **) (args[0]);
  DangString *b = * (DangString **) (args[1]);
  DANG_UNUSED (func_data); DANG_UNUSED (error);
  if (a == NULL)
    {
      if (b == NULL)
        * (char*) rv = 1;
      else
        * (char*) rv = 1;
    }
  else
    {
      if (b == NULL)
        * (char*) rv = 0;
      else
        * (char*) rv = (strcmp(a->str, b->str) <= 0);
    }
  return TRUE;
}
static dang_boolean
do_operator_greaterthan_string  (void **args,
                              void *rv,
                              void *func_data,
                              DangError **error)
{
  DangString *a = * (DangString **) (args[0]);
  DangString *b = * (DangString **) (args[1]);
  DANG_UNUSED (func_data); DANG_UNUSED (error);
  if (a == NULL)
    {
      if (b == NULL)
        * (char*) rv = 0;
      else
        * (char*) rv = 0;
    }
  else
    {
      if (b == NULL)
        * (char*) rv = 1;
      else
        * (char*) rv = (strcmp(a->str, b->str) > 0);
    }
  return TRUE;
}
static dang_boolean
do_operator_greatereq_string  (void **args,
                              void *rv,
                              void *func_data,
                              DangError **error)
{
  DangString *a = * (DangString **) (args[0]);
  DangString *b = * (DangString **) (args[1]);
  DANG_UNUSED (func_data); DANG_UNUSED (error);
  if (a == NULL)
    {
      if (b == NULL)
        * (char*) rv = 1;
      else
        * (char*) rv = 0;
    }
  else
    {
      if (b == NULL)
        * (char*) rv = 1;
      else
        * (char*) rv = (strcmp(a->str, b->str) >= 0);
    }
  return TRUE;
}
static dang_boolean
do_operator_equal_string  (void **args,
                              void *rv,
                              void *func_data,
                              DangError **error)
{
  DangString *a = * (DangString **) (args[0]);
  DangString *b = * (DangString **) (args[1]);
  DANG_UNUSED (func_data); DANG_UNUSED (error);
  if (a == NULL)
    {
      if (b == NULL)
        * (char*) rv = 1;
      else
        * (char*) rv = 0;
    }
  else
    {
      if (b == NULL)
        * (char*) rv = 0;
      else
        * (char*) rv = (strcmp(a->str, b->str) == 0);
    }
  return TRUE;
}
static dang_boolean
do_operator_notequal_string  (void **args,
                              void *rv,
                              void *func_data,
                              DangError **error)
{
  DangString *a = * (DangString **) (args[0]);
  DangString *b = * (DangString **) (args[1]);
  DANG_UNUSED (func_data); DANG_UNUSED (error);
  if (a == NULL)
    {
      if (b == NULL)
        * (char*) rv = 0;
      else
        * (char*) rv = 1;
    }
  else
    {
      if (b == NULL)
        * (char*) rv = 1;
      else
        * (char*) rv = (strcmp(a->str, b->str) != 0);
    }
  return TRUE;
}

static dang_boolean
do_operator_cmp_string  (void **args,
                              void *rv,
                              void *func_data,
                              DangError **error)
{
  DangString *a = * (DangString **) (args[0]);
  DangString *b = * (DangString **) (args[1]);
  DANG_UNUSED (func_data); DANG_UNUSED (error);
  if (a == NULL)
    {
      if (b == NULL)
        * (int32_t*) rv = 0;
      else
        * (int32_t*) rv = -1;
    }
  else
    {
      if (b == NULL)
        * (int32_t*) rv = 1;
      else
        * (int32_t*) rv = (strcmp(a->str, b->str));
    }
  return TRUE;
}

static dang_boolean
do_operator_add_string  (void **args,
                              void *rv,
                              void *func_data,
                              DangError **error)
{
  DangString *a[2];
  DANG_UNUSED (func_data);
  a[0] = *(DangString**) (args[0]);
  a[1] = *(DangString**) (args[1]);
  if (a[0] == NULL || a[1] == NULL)
    {
      dang_set_error (error, "null-pointer exception in operator_add(string,string)");
      return FALSE;
    }
  * (DangString**) rv = dang_strings_concat (2,a);
  return TRUE;
}

static DANG_SIMPLE_C_FUNC_DECLARE(do_debug_to_string)
{
  DangValueType *type = func_data;
  char *str = type->to_string (type, args[0]);
  DANG_UNUSED (error);
  * (DangString **) rv_out = dang_string_new (str);
  dang_free (str);
  return TRUE;
}

static DangFunction *
try_sig__debug_string (DangMatchQuery *query, void *data, DangError **error)
{
  DangFunctionParam param;
  DangSignature *sig;
  DangFunction *rv;
  DANG_UNUSED (data);
  DANG_UNUSED (error);
  if (query->n_elements != 1
   || query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT)
    return NULL;
  param.dir = DANG_FUNCTION_PARAM_IN;
  param.name = NULL;
  param.type = query->elements[0].info.simple_input;
  sig = dang_signature_new (dang_value_type_string (), 1, &param);
  rv = dang_function_new_simple_c (sig, do_debug_to_string, param.type, NULL);
  dang_signature_unref (sig);
  return rv;
}


static char
get_end_char_for_start_char (char c)
{
  switch (c)
    {
    case '(': return ')';
    case '[': return ']';
    case '{': return '}';
    default: return c;
    }
}

typedef struct _HexDataState HexDataState;
struct _HexDataState
{
  unsigned len, alloced;
  uint8_t *data;
  uint8_t has_nibble;
  uint8_t nibble;
  char end_char;
};
static DangTokenizerResult
hex_data_tokenize (DangLiteralTokenizer *lit_tokenizer,
		   void                 *state,
		   unsigned              len,
		   const char           *text,
		   unsigned             *text_used_out,
		   DangToken           **token_out,
		   DangError           **error)
{
  HexDataState *hex_data = state;
  uint8_t nib, nib2;
  unsigned used = 0;
  DANG_UNUSED (lit_tokenizer);
  if (len == 0)
    return DANG_LITERAL_TOKENIZER_CONTINUE;
  if (hex_data->has_nibble)
    {
      nib = hex_data->nibble;
      hex_data->has_nibble = 0;
      goto have_nibble;
    }
  if (hex_data->end_char == 0)
    {
      while (used < len && isspace (text[used]))
        used++;
      if (used == len) 
        return DANG_LITERAL_TOKENIZER_CONTINUE;
      hex_data->end_char = get_end_char_for_start_char (text[used]);
      used++;
    }
  while (used < len)
    {
restart:
      switch (text[used])
	{
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
	  nib = 10 + (text[used] - 'a');
	  break;
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
	  nib = 10 + (text[used] - 'A');
	  break;
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	  nib = text[used] - '0';
	  break;
	case ' ': case '\t': case '\r': case '\n':
	  used++;
	  while (used < len && isspace (text[used]))
	    used++;
	  goto restart;
        default:
          if (text[used] == hex_data->end_char)
            {
              DangVector *vector = dang_new (DangVector, 1);
              vector->ref_count = 1;
              vector->data = hex_data->data;
              vector->len = hex_data->len;
              hex_data->data = NULL;
              *token_out
                = dang_token_literal_take (dang_value_type_vector (dang_value_type_uint8 ()),
                                           dang_memdup (&vector, sizeof (void*)));
              *text_used_out = used + 1;
              return DANG_LITERAL_TOKENIZER_DONE;
            }
          else
            {
              dang_set_error (error, "bad character '%c' hex data", text[used]);
              return DANG_LITERAL_TOKENIZER_ERROR;
            }
	}
      used++;
      if (used == len)
	{
	  /* store nibble */
	  hex_data->has_nibble = 1;
          hex_data->nibble = nib;
          return DANG_LITERAL_TOKENIZER_CONTINUE;
	}
have_nibble:
      switch (text[used])
	{
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
	  nib2 = 10 + (text[used] - 'a');
	  break;
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
	  nib2 = 10 + (text[used] - 'A');
	  break;
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	  nib2 = text[used] - '0';
	  break;
	default:
	  dang_set_error (error, "bad character '%c' mid-byte in hex data",
			  text[used]);
	  return DANG_LITERAL_TOKENIZER_ERROR;
	}
      used++;
      if (hex_data->len == hex_data->alloced)
        {
          unsigned new_alloced = hex_data->alloced ? hex_data->alloced * 2 : 16;
          hex_data->alloced = new_alloced;
          hex_data->data = dang_realloc (hex_data->data, new_alloced);
        }
      hex_data->data[hex_data->len++] = (nib << 4) + nib2;
    }
  return DANG_LITERAL_TOKENIZER_CONTINUE;
}

static void
hex_data_state_clear (DangLiteralTokenizer *lit_tokenizer,
                      void                 *state)
{
  HexDataState *hex_data = state;
  DANG_UNUSED (lit_tokenizer);
  dang_free (hex_data->data);
}

static DangLiteralTokenizer hex_data_tokenizer =
{
  "hex_data",
  sizeof (HexDataState),
  NULL,		/* no init function needed */
  hex_data_tokenize,
  hex_data_state_clear,
  DANG_LITERAL_TOKENIZER_INTERNALS_INIT
};

/* === various metafunctions === */

static void
add_type (DangNamespace *ns,
          const char    *name,
          DangValueType *type)
{
  DangError *error = NULL;
  if (!dang_namespace_add_type (ns, name, type, &error))
    dang_die ("adding type '%s' as '%s' to '%s': %s",
              type->full_name, name, ns->full_name, error->message);
}

#ifdef DANG_DEBUG
///void _dang_debug_init_operator_index (void);
///void _dang_debug_init_catch (void);
///void _dang_debug_tensor_init (void);
///void _dang_closure_factory_debug_init (void);
///void _dang_debug_init_create_closure(void);
#endif

void _dang_string_init (DangNamespace *def);
void _dang_struct_init (DangNamespace *ns);
void _dang_file_init (DangNamespace *file_ns);
void _dang_math_init (DangNamespace *);
void _dang_enum_init (DangNamespace *the_ns);

static void
add_variadic_c_family_data (DangNamespace *ns,
                            const char    *long_name,
                            const char    *name,
                            DangFunctionTrySigFunc func,
                            void *func_data)
{
  DangFunctionFamily *family;
  DangError *error = NULL;
  family = dang_function_family_new_variadic_c (long_name, func, func_data, NULL);
  if (!dang_namespace_add_function_family (ns, name, family, &error))
    dang_die ("dang_namespace_add_function_family(%s) failed: %s",
              name, error->message);
  dang_function_family_unref (family);
}
#define add_variadic_c_family(ns, long_name, name, func) \
  add_variadic_c_family_data(ns, long_name, name, func, NULL)

static DangNamespace *the_ns = NULL;
DangNamespace *
dang_namespace_default (void)
{
  DangError *error = NULL;
  if (the_ns == NULL)
    {
      DangNamespace *sys_ns;
      DangNamespace *math_ns;
      the_ns = dang_namespace_new ("*default*");
      sys_ns = dang_namespace_new ("system");
      if (!dang_namespace_add_namespace (the_ns, "system", sys_ns, &error))
        dang_die ("error adding system ns to *default*: %s", error->message);
      add_type (the_ns, "tiny", dang_value_type_int8 ());
      add_type (the_ns, "utiny", dang_value_type_uint8 ());
      add_type (the_ns, "short", dang_value_type_int16 ());
      add_type (the_ns, "ushort", dang_value_type_uint16 ());
      add_type (the_ns, "int", dang_value_type_int32 ());
      add_type (the_ns, "uint", dang_value_type_uint32 ());
      add_type (the_ns, "long", dang_value_type_int64 ());
      add_type (the_ns, "ulong", dang_value_type_uint64 ());
      add_type (the_ns, "float", dang_value_type_float ());
      add_type (the_ns, "double", dang_value_type_double ());
      add_type (the_ns, "string", dang_value_type_string ());
      add_type (the_ns, "boolean", dang_value_type_boolean ());
      add_type (the_ns, "char", dang_value_type_char ());
      add_type (the_ns, "error", dang_value_type_error ());
      add_simple (sys_ns, "println", do_system_println, NULL,
                  1,
                  DANG_FUNCTION_PARAM_IN, "str", dang_value_type_string ());
      add_simple (sys_ns, "abort", do_system_abort, NULL,
                  1,
                  DANG_FUNCTION_PARAM_IN, "str", dang_value_type_string ());
      add_simple (the_ns, "assert", do_assert, NULL,
                  1,
                  DANG_FUNCTION_PARAM_IN, "cond", dang_value_type_boolean ());
      add_simple (the_ns, "operator_not", do_operator_not,
                  dang_value_type_boolean (),
                  1,
                  DANG_FUNCTION_PARAM_IN, NULL, dang_value_type_boolean ());
      add_simple (the_ns, "operator_equal", do_operator_boolean_equal,
                  dang_value_type_boolean (),
                  2,
                  DANG_FUNCTION_PARAM_IN, NULL, dang_value_type_boolean (),
                  DANG_FUNCTION_PARAM_IN, NULL, dang_value_type_boolean ());
      add_simple (the_ns, "operator_notequal", do_operator_boolean_notequal,
                  dang_value_type_boolean (),
                  2,
                  DANG_FUNCTION_PARAM_IN, NULL, dang_value_type_boolean (),
                  DANG_FUNCTION_PARAM_IN, NULL, dang_value_type_boolean ());
      _dang_string_init (the_ns);
      _dang_struct_init (the_ns);

#define REGISTER_OPERATOR(cmd, type_suffix, sig)          \
      dang_namespace_add_simple_c (the_ns,                \
                                   "operator_" #cmd,      \
                                   sig,                   \
                                   do_operator_##cmd##_##type_suffix, \
                                   NULL)

#define DECLARE_SIGNATURES              \
      DangFunctionParam tmp_params[2];  \
      DangValueType *type; \
      DangSignature *incr_sig, *bin_op_sig, \
                    *bin_test_sig, *cmp_sig, \
                    *assign_bin_op_sig, \
                    *neg_sig, *not_sig

#define SETUP_SIGS_AND_TYPE(type_suffix) \
     do{ \
      /* Initialize 'type' */ \
      type = dang_value_type_##type_suffix (); \
      /* Set up 'incr_sig' for ++ and -- */ \
      tmp_params[0].dir = DANG_FUNCTION_PARAM_INOUT; \
      tmp_params[0].name = NULL; \
      tmp_params[0].type = type; \
      incr_sig = dang_signature_new (type, 1, tmp_params); \
      /* Set up 'bin_op_sig' for +, -, *, etc */ \
      tmp_params[0].dir = DANG_FUNCTION_PARAM_IN; \
      tmp_params[0].name = NULL; \
      tmp_params[0].type = type; \
      tmp_params[1].dir = DANG_FUNCTION_PARAM_IN; \
      tmp_params[1].name = NULL; \
      tmp_params[1].type = type; \
      bin_op_sig = dang_signature_new (type, 2, tmp_params); \
      /* Set up 'assign_bin_op_sig' for +=, -=, *=, etc */ \
      tmp_params[0].dir = DANG_FUNCTION_PARAM_INOUT; \
      tmp_params[0].name = NULL; \
      tmp_params[0].type = type; \
      tmp_params[1].dir = DANG_FUNCTION_PARAM_IN; \
      tmp_params[1].name = NULL; \
      tmp_params[1].type = type; \
      assign_bin_op_sig = dang_signature_new (NULL, 2, tmp_params); \
      /* Set up 'bin_test_sig' for <, > etc */ \
      tmp_params[0].dir = DANG_FUNCTION_PARAM_IN; \
      tmp_params[0].name = NULL; \
      tmp_params[0].type = type; \
      tmp_params[1].dir = DANG_FUNCTION_PARAM_IN; \
      tmp_params[1].name = NULL; \
      tmp_params[1].type = type; \
      bin_test_sig = dang_signature_new (dang_value_type_boolean (), 2, tmp_params); \
      /* Set up 'cmp_sig' for <=> etc */ \
      tmp_params[0].dir = DANG_FUNCTION_PARAM_IN; \
      tmp_params[0].name = NULL; \
      tmp_params[0].type = type; \
      tmp_params[1].dir = DANG_FUNCTION_PARAM_IN; \
      tmp_params[1].name = NULL; \
      tmp_params[1].type = type; \
      cmp_sig = dang_signature_new (dang_value_type_int32 (), 2, tmp_params); \
      /* Define signatures for negate and not */ \
      neg_sig = dang_signature_new (type, 1, tmp_params); \
      not_sig = dang_signature_new (dang_value_type_boolean (), 1, tmp_params); \
     }while(0)

#define CLEAR_SIG(v) do{ dang_signature_unref (v); v = NULL; }while(0)
#define CLEAR_SIGS() \
      do{ \
      type = NULL; \
      CLEAR_SIG(incr_sig); \
      CLEAR_SIG(bin_op_sig); \
      CLEAR_SIG(bin_test_sig); \
      CLEAR_SIG(cmp_sig); \
      CLEAR_SIG (assign_bin_op_sig); \
      CLEAR_SIG(not_sig); \
      CLEAR_SIG(neg_sig); \
      incr_sig = NULL; \
      }while(0)
#define REGISTER_NUMERIC_OPS(type_suffix)                            \
      do{                                                            \
      DECLARE_SIGNATURES;                                            \
      SETUP_SIGS_AND_TYPE(type_suffix);                              \
      REGISTER_OPERATOR(lessthan, type_suffix, bin_test_sig);        \
      REGISTER_OPERATOR(lesseq, type_suffix, bin_test_sig);          \
      REGISTER_OPERATOR(greaterthan, type_suffix, bin_test_sig);     \
      REGISTER_OPERATOR(greatereq, type_suffix, bin_test_sig);       \
      REGISTER_OPERATOR(equal, type_suffix, bin_test_sig);           \
      REGISTER_OPERATOR(notequal, type_suffix, bin_test_sig);        \
      REGISTER_OPERATOR(preincrement, type_suffix, incr_sig);        \
      REGISTER_OPERATOR(postincrement, type_suffix, incr_sig);       \
      REGISTER_OPERATOR(predecrement, type_suffix, incr_sig);        \
      REGISTER_OPERATOR(postdecrement, type_suffix, incr_sig);       \
      REGISTER_OPERATOR(add, type_suffix, bin_op_sig);               \
      REGISTER_OPERATOR(subtract, type_suffix, bin_op_sig);          \
      REGISTER_OPERATOR(multiply, type_suffix, bin_op_sig);          \
      REGISTER_OPERATOR(divide, type_suffix, bin_op_sig);            \
      REGISTER_OPERATOR(mod, type_suffix, bin_op_sig);               \
      REGISTER_OPERATOR(cmp, type_suffix, cmp_sig);                  \
      REGISTER_OPERATOR(negate, type_suffix, neg_sig);               \
      REGISTER_OPERATOR(not, type_suffix, not_sig);                  \
      REGISTER_OPERATOR(assign_add, type_suffix, assign_bin_op_sig); \
      REGISTER_OPERATOR(assign_subtract, type_suffix, assign_bin_op_sig);\
      REGISTER_OPERATOR(assign_multiply, type_suffix, assign_bin_op_sig);\
      REGISTER_OPERATOR(assign_divide, type_suffix, assign_bin_op_sig);\
      REGISTER_OPERATOR(assign_mod, type_suffix, assign_bin_op_sig); \
      CLEAR_SIGS();                                                  \
      }while(0)
#define REGISTER_BITWISE_OPS(type_suffix)                            \
      do{                                                            \
      DangFunctionParam tmp_params[2];  \
      DangValueType *type; \
      DangSignature *unary_op_sig, *bin_op_sig, \
                    *assign_bin_op_sig, *shift_sig, *assign_shift_sig;\
      /* Initialize 'type' */ \
      type = dang_value_type_##type_suffix (); \
      /* Set up 'unary_op_sig' (~) */ \
      tmp_params[0].dir = DANG_FUNCTION_PARAM_IN; \
      tmp_params[0].name = NULL; \
      tmp_params[0].type = type; \
      unary_op_sig = dang_signature_new (type, 1, tmp_params); \
      /* Set up 'bin_op_sig' for |, ^, & */ \
      tmp_params[0].dir = DANG_FUNCTION_PARAM_IN; \
      tmp_params[0].name = NULL; \
      tmp_params[0].type = type; \
      tmp_params[1].dir = DANG_FUNCTION_PARAM_IN; \
      tmp_params[1].name = NULL; \
      tmp_params[1].type = type; \
      bin_op_sig = dang_signature_new (type, 2, tmp_params); \
      /* Set up 'assign_bin_op_sig' for |=, &=, ^=, etc */ \
      tmp_params[0].dir = DANG_FUNCTION_PARAM_INOUT; \
      tmp_params[0].name = NULL; \
      tmp_params[0].type = type; \
      tmp_params[1].dir = DANG_FUNCTION_PARAM_IN; \
      tmp_params[1].name = NULL; \
      tmp_params[1].type = type; \
      assign_bin_op_sig = dang_signature_new (NULL, 2, tmp_params); \
      /* Set up 'shift_sig' for <<, >> */ \
      tmp_params[0].dir = DANG_FUNCTION_PARAM_IN; \
      tmp_params[0].name = NULL; \
      tmp_params[0].type = type; \
      tmp_params[1].dir = DANG_FUNCTION_PARAM_IN; \
      tmp_params[1].name = NULL; \
      tmp_params[1].type = dang_value_type_uint32(); \
      shift_sig = dang_signature_new (type, 2, tmp_params); \
      /* Set up 'assign_shift_sig' for <<=, >>= */ \
      tmp_params[0].dir = DANG_FUNCTION_PARAM_INOUT; \
      tmp_params[0].name = NULL; \
      tmp_params[0].type = type; \
      tmp_params[1].dir = DANG_FUNCTION_PARAM_IN; \
      tmp_params[1].name = NULL; \
      tmp_params[1].type = dang_value_type_uint32(); \
      assign_shift_sig = dang_signature_new (NULL, 2, tmp_params); \
      REGISTER_OPERATOR(bitwise_and, type_suffix, bin_op_sig);        \
      REGISTER_OPERATOR(bitwise_or, type_suffix, bin_op_sig);        \
      REGISTER_OPERATOR(bitwise_xor, type_suffix, bin_op_sig);        \
      REGISTER_OPERATOR(bitwise_complement, type_suffix, unary_op_sig);        \
      REGISTER_OPERATOR(left_shift, type_suffix, shift_sig);        \
      REGISTER_OPERATOR(right_shift, type_suffix, shift_sig);        \
      REGISTER_OPERATOR(assign_bitwise_and, type_suffix, assign_bin_op_sig);        \
      REGISTER_OPERATOR(assign_bitwise_or, type_suffix, assign_bin_op_sig);        \
      REGISTER_OPERATOR(assign_bitwise_xor, type_suffix, assign_bin_op_sig);        \
      REGISTER_OPERATOR(assign_left_shift, type_suffix, assign_shift_sig);        \
      REGISTER_OPERATOR(assign_right_shift, type_suffix, assign_shift_sig);        \
      dang_signature_unref (unary_op_sig); \
      dang_signature_unref (bin_op_sig); \
      dang_signature_unref (assign_bin_op_sig); \
      dang_signature_unref (shift_sig); \
      dang_signature_unref (assign_shift_sig); \
    }while(0)

      REGISTER_NUMERIC_OPS(int8);
      REGISTER_NUMERIC_OPS(uint8);
      REGISTER_NUMERIC_OPS(int16);
      REGISTER_NUMERIC_OPS(uint16);
      REGISTER_NUMERIC_OPS(int32);
      REGISTER_NUMERIC_OPS(uint32);
      REGISTER_NUMERIC_OPS(int64);
      REGISTER_NUMERIC_OPS(uint64);
      REGISTER_NUMERIC_OPS(float);
      REGISTER_NUMERIC_OPS(double);

      REGISTER_BITWISE_OPS(int8);
      REGISTER_BITWISE_OPS(uint8);
      REGISTER_BITWISE_OPS(int16);
      REGISTER_BITWISE_OPS(uint16);
      REGISTER_BITWISE_OPS(int32);
      REGISTER_BITWISE_OPS(uint32);
      REGISTER_BITWISE_OPS(int64);
      REGISTER_BITWISE_OPS(uint64);

      {
        DECLARE_SIGNATURES;
        SETUP_SIGS_AND_TYPE (string);
        REGISTER_OPERATOR(lessthan, string, bin_test_sig);
        REGISTER_OPERATOR(lesseq, string, bin_test_sig);
        REGISTER_OPERATOR(greaterthan, string, bin_test_sig);
        REGISTER_OPERATOR(greatereq, string, bin_test_sig);
        REGISTER_OPERATOR(equal, string, bin_test_sig);
        REGISTER_OPERATOR(notequal, string, bin_test_sig);
        REGISTER_OPERATOR(add, string, bin_op_sig);
        REGISTER_OPERATOR(cmp, string, cmp_sig);
        CLEAR_SIGS ();
      }
      {
        DECLARE_SIGNATURES;
        SETUP_SIGS_AND_TYPE (char);
        REGISTER_OPERATOR(lessthan, char, bin_test_sig);
        REGISTER_OPERATOR(lesseq, char, bin_test_sig);
        REGISTER_OPERATOR(greaterthan, char, bin_test_sig);
        REGISTER_OPERATOR(greatereq, char, bin_test_sig);
        REGISTER_OPERATOR(equal, char, bin_test_sig);
        REGISTER_OPERATOR(notequal, char, bin_test_sig);
        REGISTER_OPERATOR(cmp, char, cmp_sig);
        CLEAR_SIGS ();
      }
#undef REGISTER_OPERATOR

#define REGISTER_CAST_FUNCTION(from_type, to_type) \
  add_simple (the_ns, \
              "operator_cast__" #to_type, \
              operator_cast__to_##to_type##__from_##from_type, \
              dang_value_type_##to_type (), \
              1, DANG_FUNCTION_PARAM_IN, NULL, dang_value_type_##from_type ())
        REGISTER_CAST_FUNCTION(int8, int8);
        REGISTER_CAST_FUNCTION(int8, int16);
        REGISTER_CAST_FUNCTION(int8, int32);
        REGISTER_CAST_FUNCTION(int8, int64);
        REGISTER_CAST_FUNCTION(int8, uint8);
        REGISTER_CAST_FUNCTION(int8, uint16);
        REGISTER_CAST_FUNCTION(int8, uint32);
        REGISTER_CAST_FUNCTION(int8, uint64);
        REGISTER_CAST_FUNCTION(int8, float);
        REGISTER_CAST_FUNCTION(int8, double);
        REGISTER_CAST_FUNCTION(int8, char);
        REGISTER_CAST_FUNCTION(int16, int8);
        REGISTER_CAST_FUNCTION(int16, int16);
        REGISTER_CAST_FUNCTION(int16, int32);
        REGISTER_CAST_FUNCTION(int16, int64);
        REGISTER_CAST_FUNCTION(int16, uint8);
        REGISTER_CAST_FUNCTION(int16, uint16);
        REGISTER_CAST_FUNCTION(int16, uint32);
        REGISTER_CAST_FUNCTION(int16, uint64);
        REGISTER_CAST_FUNCTION(int16, float);
        REGISTER_CAST_FUNCTION(int16, double);
        REGISTER_CAST_FUNCTION(int16, char);
        REGISTER_CAST_FUNCTION(int32, int8);
        REGISTER_CAST_FUNCTION(int32, int16);
        REGISTER_CAST_FUNCTION(int32, int32);
        REGISTER_CAST_FUNCTION(int32, int64);
        REGISTER_CAST_FUNCTION(int32, uint8);
        REGISTER_CAST_FUNCTION(int32, uint16);
        REGISTER_CAST_FUNCTION(int32, uint32);
        REGISTER_CAST_FUNCTION(int32, uint64);
        REGISTER_CAST_FUNCTION(int32, float);
        REGISTER_CAST_FUNCTION(int32, double);
        REGISTER_CAST_FUNCTION(int32, char);
        REGISTER_CAST_FUNCTION(int64, int8);
        REGISTER_CAST_FUNCTION(int64, int16);
        REGISTER_CAST_FUNCTION(int64, int32);
        REGISTER_CAST_FUNCTION(int64, int64);
        REGISTER_CAST_FUNCTION(int64, uint8);
        REGISTER_CAST_FUNCTION(int64, uint16);
        REGISTER_CAST_FUNCTION(int64, uint32);
        REGISTER_CAST_FUNCTION(int64, uint64);
        REGISTER_CAST_FUNCTION(int64, float);
        REGISTER_CAST_FUNCTION(int64, double);
        REGISTER_CAST_FUNCTION(int64, char);
        REGISTER_CAST_FUNCTION(uint8, int8);
        REGISTER_CAST_FUNCTION(uint8, int16);
        REGISTER_CAST_FUNCTION(uint8, int32);
        REGISTER_CAST_FUNCTION(uint8, int64);
        REGISTER_CAST_FUNCTION(uint8, uint8);
        REGISTER_CAST_FUNCTION(uint8, uint16);
        REGISTER_CAST_FUNCTION(uint8, uint32);
        REGISTER_CAST_FUNCTION(uint8, uint64);
        REGISTER_CAST_FUNCTION(uint8, float);
        REGISTER_CAST_FUNCTION(uint8, double);
        REGISTER_CAST_FUNCTION(uint8, char);
        REGISTER_CAST_FUNCTION(uint16, int8);
        REGISTER_CAST_FUNCTION(uint16, int16);
        REGISTER_CAST_FUNCTION(uint16, int32);
        REGISTER_CAST_FUNCTION(uint16, int64);
        REGISTER_CAST_FUNCTION(uint16, uint8);
        REGISTER_CAST_FUNCTION(uint16, uint16);
        REGISTER_CAST_FUNCTION(uint16, uint32);
        REGISTER_CAST_FUNCTION(uint16, uint64);
        REGISTER_CAST_FUNCTION(uint16, float);
        REGISTER_CAST_FUNCTION(uint16, double);
        REGISTER_CAST_FUNCTION(uint16, char);
        REGISTER_CAST_FUNCTION(uint32, int8);
        REGISTER_CAST_FUNCTION(uint32, int16);
        REGISTER_CAST_FUNCTION(uint32, int32);
        REGISTER_CAST_FUNCTION(uint32, int64);
        REGISTER_CAST_FUNCTION(uint32, uint8);
        REGISTER_CAST_FUNCTION(uint32, uint16);
        REGISTER_CAST_FUNCTION(uint32, uint32);
        REGISTER_CAST_FUNCTION(uint32, uint64);
        REGISTER_CAST_FUNCTION(uint32, float);
        REGISTER_CAST_FUNCTION(uint32, double);
        REGISTER_CAST_FUNCTION(uint32, char);
        REGISTER_CAST_FUNCTION(uint64, int8);
        REGISTER_CAST_FUNCTION(uint64, int16);
        REGISTER_CAST_FUNCTION(uint64, int32);
        REGISTER_CAST_FUNCTION(uint64, int64);
        REGISTER_CAST_FUNCTION(uint64, uint8);
        REGISTER_CAST_FUNCTION(uint64, uint16);
        REGISTER_CAST_FUNCTION(uint64, uint32);
        REGISTER_CAST_FUNCTION(uint64, uint64);
        REGISTER_CAST_FUNCTION(uint64, float);
        REGISTER_CAST_FUNCTION(uint64, double);
        REGISTER_CAST_FUNCTION(uint64, char);
        REGISTER_CAST_FUNCTION(float, int8);
        REGISTER_CAST_FUNCTION(float, int16);
        REGISTER_CAST_FUNCTION(float, int32);
        REGISTER_CAST_FUNCTION(float, int64);
        REGISTER_CAST_FUNCTION(float, uint8);
        REGISTER_CAST_FUNCTION(float, uint16);
        REGISTER_CAST_FUNCTION(float, uint32);
        REGISTER_CAST_FUNCTION(float, uint64);
        REGISTER_CAST_FUNCTION(float, float);
        REGISTER_CAST_FUNCTION(float, double);
        REGISTER_CAST_FUNCTION(float, char);
        REGISTER_CAST_FUNCTION(double, int8);
        REGISTER_CAST_FUNCTION(double, int16);
        REGISTER_CAST_FUNCTION(double, int32);
        REGISTER_CAST_FUNCTION(double, int64);
        REGISTER_CAST_FUNCTION(double, uint8);
        REGISTER_CAST_FUNCTION(double, uint16);
        REGISTER_CAST_FUNCTION(double, uint32);
        REGISTER_CAST_FUNCTION(double, uint64);
        REGISTER_CAST_FUNCTION(double, float);
        REGISTER_CAST_FUNCTION(double, double);
        REGISTER_CAST_FUNCTION(double, char);
        REGISTER_CAST_FUNCTION(char, int8);
        REGISTER_CAST_FUNCTION(char, int16);
        REGISTER_CAST_FUNCTION(char, int32);
        REGISTER_CAST_FUNCTION(char, int64);
        REGISTER_CAST_FUNCTION(char, uint8);
        REGISTER_CAST_FUNCTION(char, uint16);
        REGISTER_CAST_FUNCTION(char, uint32);
        REGISTER_CAST_FUNCTION(char, uint64);
        REGISTER_CAST_FUNCTION(char, float);
        REGISTER_CAST_FUNCTION(char, double);
        REGISTER_CAST_FUNCTION(char, char);
#define REGISTER_TO_STRING(type_suffix) \
      add_simple (the_ns, "to_string", do_to_string_##type_suffix, \
                  dang_value_type_string (),                                  \
                  1,                                                         \
                  DANG_FUNCTION_PARAM_IN, "a", dang_value_type_##type_suffix ())
      REGISTER_TO_STRING(int8);
      REGISTER_TO_STRING(uint8);
      REGISTER_TO_STRING(int16);
      REGISTER_TO_STRING(uint16);
      REGISTER_TO_STRING(int32);
      REGISTER_TO_STRING(uint32);
      REGISTER_TO_STRING(int64);
      REGISTER_TO_STRING(uint64);
      REGISTER_TO_STRING(float);
      REGISTER_TO_STRING(double);
      REGISTER_TO_STRING(boolean);
      REGISTER_TO_STRING(string);
      REGISTER_TO_STRING(char);
#undef REGISTER_TO_STRING

      /* obvious string operators */

//      add_simple (the_ns, "length", do_string_length,
//                  dang_value_type_uint32 (),
//                  1,
//                  DANG_FUNCTION_PARAM_IN, "a", dang_value_type_string ());

      dang_namespace_unref (sys_ns);
      _dang_tensor_init (the_ns);
      _dang_array_init (the_ns);
      _dang_enum_init (the_ns);

      add_variadic_c_family (the_ns,
                             "debug_string", "debug_string",
                             try_sig__debug_string);

      math_ns = dang_namespace_new ("math");
      if (!dang_namespace_add_namespace (the_ns, "math", math_ns, &error))
        dang_die ("error adding system ns to *default*: %s", error->message);
      _dang_math_init (math_ns);
      dang_namespace_unref (math_ns);
#ifdef HAVE_GSL
      {
        extern void _dang_gsl_init (DangNamespace *ns);
        _dang_gsl_init (math_ns);
      }
#endif

      {
        DangNamespace *file_ns = dang_namespace_new ("file");
        if (!dang_namespace_add_namespace (the_ns, "file", file_ns, &error))
          dang_die ("error adding system ns to *default*: %s", error->message);
        _dang_file_init (file_ns);
        dang_namespace_unref (file_ns);
      }

      dang_literal_tokenizer_register (&hex_data_tokenizer);
    }

#ifdef DANG_DEBUG
/////  _dang_compile__register_debug ();
/////  _dang_builder_add_assign__register_debug ();
/////  _dang_compile_function_invocation__register_debug ();
/////  _dang_compile_member_access__register_debug ();
/////  _dang_builder_add_return__register_debug ();
/////  _dang_function_new_simple_c__register_debug ();
/////  _dang_builder_add_jump__register_debug ();
/////  _dang_compile_obey_flags__register_debug ();
/////  _dang_builder__register_debug ();
/////  _dang_debug_init_operator_index ();
/////  _dang_debug_init_catch ();
/////  _dang_value_type_debug_init ();
/////  _dang_closure_factory_debug_init ();
/////  _dang_debug_tensor_init ();
/////  _dang_debug_init_create_closure();
#endif

  return the_ns;
}

void _dang_tokens_dump_all (void);

static void clean_stubs_recursive_family (DangFunctionFamily *family)
{
  if (family->type == DANG_FUNCTION_FAMILY_CONTAINER)
    {
      DangFunction **subfunctions = family->info.container.functions.data;
      DangFunctionFamily **subfamilies = family->info.container.families.data;
      unsigned i;
      for (i = 0; i < family->info.container.functions.len; i++)
        if (subfunctions[i]->type == DANG_FUNCTION_TYPE_STUB)
          {
            DangImports *imports = subfunctions[i]->stub.imports;
            subfunctions[i]->stub.imports = NULL;
            if (imports)
              dang_imports_unref (imports);
          }
      for (i = 0; i < family->info.container.families.len; i++)
        clean_stubs_recursive_family (subfamilies[i]);
    }
  else if (family->type == DANG_FUNCTION_FAMILY_TEMPLATE)
    {
      if (family->info.templat.imports)
        {
          dang_imports_unref (family->info.templat.imports);
          family->info.templat.imports = NULL;
        }
    }
}
static void clean_stubs_recursive (DangNamespace *ns);

static void
clean_stubs_recursive_name (DangNamespaceName *name)
{
  if (name == NULL)
    return;
  clean_stubs_recursive_name (name->left);
  clean_stubs_recursive_name (name->right);
  if (name->symbol.type == DANG_NAMESPACE_SYMBOL_FUNCTIONS)
    clean_stubs_recursive_family (name->symbol.info.functions);
  else if (name->symbol.type == DANG_NAMESPACE_SYMBOL_NAMESPACE)
    clean_stubs_recursive (name->symbol.info.ns);
}

static void
clean_stubs_recursive (DangNamespace *ns)
{
  clean_stubs_recursive_name (ns->by_name);
}

void _dang_value_function_cleanup ();
void _dang_object_cleanup1 (void);      /* while type->destruct is safe */
void _dang_object_cleanup2 (void);
void _dang_struct_cleanup1 (void);      /* while type->destruct is safe */
void _dang_struct_cleanup2 (void);
void _dang_template_cleanup (void);
void _dang_union_cleanup (void);
void _dang_function_concat_cleanup (void);
void _dang_tensor_cleanup (void);
void _dang_array_cleanup (void);
void _dang_enum_cleanup (void);


void
dang_cleanup (void)
{
  DangNamespace *ns = the_ns;
  the_ns = NULL;
  if (ns != NULL)
    {
      clean_stubs_recursive (ns);
      dang_namespace_unref (ns);
    }
  _dang_function_concat_cleanup ();
  _dang_tokens_dump_all();

  /* Cleanup default values etc that require all types to be alive */
  _dang_object_cleanup1 ();
  _dang_struct_cleanup1 ();

  /* From here out, types may be freed,
     therefore, type->destruct() is not allowed */
  _dang_tensor_cleanup ();
  _dang_array_cleanup();
  _dang_value_function_cleanup ();
  _dang_template_cleanup ();
  _dang_object_cleanup2 ();
  _dang_struct_cleanup2 ();
  _dang_union_cleanup ();
  _dang_enum_cleanup ();
#ifdef DANG_DEBUG
  _dang_debug_cleanup ();
#endif
}
