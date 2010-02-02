#include <string.h>
#include "dang.h"
#include "magic.h"
#include "config.h"
#include "gskrbtreemacros.h"

#define IMPLEMENT_NUMERIC_COMPARE(func_name, ctype) \
static DANG_VALUE_COMPARE_FUNC_DECLARE(func_name)     \
{                                                     \
  ctype av = * (const ctype *) a;                     \
  ctype bv = * (const ctype *) b;                     \
  DANG_UNUSED (type);                                 \
  return (av < bv) ? -1 : (av > bv) ? 1 : 0;          \
}
#define IMPLEMENT_SMALL_INT_HASH(func_name, ctype)    \
static DANG_VALUE_HASH_FUNC_DECLARE(func_name)        \
{                                                     \
  DANG_UNUSED (type);                                 \
  return * (const ctype *) a;                         \
}
#define IMPLEMENT_NUMERIC_EQUAL(func_name, ctype)   \
static DANG_VALUE_EQUAL_FUNC_DECLARE(func_name)       \
{                                                     \
  DANG_UNUSED (type);                                 \
  return * (const ctype *) a == * (const ctype *) b;  \
}

IMPLEMENT_NUMERIC_COMPARE(int8_compare, int8_t)
IMPLEMENT_SMALL_INT_HASH(int8_hash, int8_t)
IMPLEMENT_NUMERIC_EQUAL(int8_equal, int8_t)

static char *int8_to_string (DangValueType *type, const void *data)
{
  DANG_UNUSED (type);
  return dang_strdup_printf ("%d", (int) (*(const int8_t*)data));
}

/*
 * Function: dang_value_type_int8
 *
 * Returns: the static type object representing a 8-bit signed integer.
 */
DangValueType *dang_value_type_int8(void)
{
  static DangValueType type = {
    DANG_VALUE_TYPE_MAGIC,
    0,
    "int8",
    1,                          /* sizeof */
    1,                          /* alignof */
    NULL, NULL, NULL,
    int8_compare,
    int8_hash,
    int8_equal,
    int8_to_string,
    "operator_cast__int8",
    NULL,                       /* compile_cast_func */
    DANG_VALUE_INTERNALS_INIT
  };
  return &type;
}
IMPLEMENT_NUMERIC_COMPARE(uint8_compare, uint8_t)
#define uint8_hash int8_hash
#define uint8_equal int8_equal
static char *uint8_to_string (DangValueType *type, const void *data)
{
  DANG_UNUSED (type);
  return dang_strdup_printf ("%u", (unsigned) (*(const uint8_t*)data));
}
DangValueType *dang_value_type_uint8(void)
{
  static DangValueType type = {
    DANG_VALUE_TYPE_MAGIC,
    0,
    "uint8",
    1,                          /* sizeof */
    1,                          /* alignof */
    NULL, NULL, NULL,
    uint8_compare,
    uint8_hash,
    uint8_equal,
    uint8_to_string,
    "operator_cast__uint8",
    NULL,                       /* compile_cast_func */
    DANG_VALUE_INTERNALS_INIT
  };
  return &type;
}
IMPLEMENT_NUMERIC_COMPARE(int16_compare, int16_t)
IMPLEMENT_SMALL_INT_HASH(int16_hash, int16_t)
IMPLEMENT_NUMERIC_EQUAL(int16_equal, int16_t)
static char *int16_to_string (DangValueType *type, const void *data)
{
  DANG_UNUSED (type);
  return dang_strdup_printf ("%d", (int) (*(const int16_t*)data));
}
DangValueType *dang_value_type_int16(void)
{
  static DangValueType type = {
    DANG_VALUE_TYPE_MAGIC,
    0,
    "int16",
    2,                          /* sizeof */
    2,                          /* alignof */
    NULL, NULL, NULL,
    int16_compare,
    int16_hash,
    int16_equal,
    int16_to_string,
    "operator_cast__int16",
    NULL,                       /* compile_cast_func */
    DANG_VALUE_INTERNALS_INIT
  };
  return &type;
}
IMPLEMENT_NUMERIC_COMPARE(uint16_compare, uint16_t)
#define uint16_hash int16_hash
#define uint16_equal int16_equal
static char *uint16_to_string (DangValueType *type, const void *data)
{
  DANG_UNUSED (type);
  return dang_strdup_printf ("%u", (unsigned) (*(const uint16_t*)data));
}
DangValueType *dang_value_type_uint16(void)
{
  static DangValueType type = {
    DANG_VALUE_TYPE_MAGIC,
    0,
    "uint16",
    2,                          /* sizeof */
    2,                          /* alignof */
    NULL, NULL, NULL,
    uint16_compare,
    uint16_hash,
    uint16_equal,
    uint16_to_string,
    "operator_cast__uint16",
    NULL,                       /* compile_cast_func */
    DANG_VALUE_INTERNALS_INIT
  };
  return &type;
}
IMPLEMENT_NUMERIC_COMPARE(int32_compare, int32_t)
IMPLEMENT_SMALL_INT_HASH(int32_hash, int32_t)
IMPLEMENT_NUMERIC_EQUAL(int32_equal, int32_t)
static char *int32_to_string (DangValueType *type, const void *data)
{
  DANG_UNUSED (type);
  return dang_strdup_printf ("%d", (int) (*(const int32_t*)data));
}
DangValueType *dang_value_type_int32(void)
{
  static DangValueType type = {
    DANG_VALUE_TYPE_MAGIC,
    0,
    "int32",
    4,                          /* sizeof */
    4,                          /* alignof */
    NULL, NULL, NULL,
    int32_compare,
    int32_hash,
    int32_equal,
    int32_to_string,
    "operator_cast__int32",
    NULL,                       /* compile_cast_func */
    DANG_VALUE_INTERNALS_INIT
  };
  return &type;
}
IMPLEMENT_NUMERIC_COMPARE(uint32_compare, uint32_t)
#define uint32_hash int32_hash
#define uint32_equal int32_equal
static char *uint32_to_string (DangValueType *type, const void *data)
{
  DANG_UNUSED (type);
  return dang_strdup_printf ("%u", (unsigned) (*(const uint32_t*)data));
}
DangValueType *dang_value_type_uint32(void)
{
  static DangValueType type = {
    DANG_VALUE_TYPE_MAGIC,
    0,
    "uint32",
    4,                          /* sizeof */
    4,                          /* alignof */
    NULL, NULL, NULL,
    uint32_compare,
    uint32_hash,
    uint32_equal,
    uint32_to_string,
    "operator_cast__uint32",
    NULL,                       /* compile_cast_func */
    DANG_VALUE_INTERNALS_INIT
  };
  return &type;
}

IMPLEMENT_NUMERIC_COMPARE(int64_compare, int64_t)
IMPLEMENT_SMALL_INT_HASH(int64_hash, int64_t)
IMPLEMENT_NUMERIC_EQUAL(int64_equal, int64_t)

static char *int64_to_string (DangValueType *type, const void *data)
{
  DANG_UNUSED (type);
  /* TODO: portability */
  return dang_strdup_printf ("%lld", (*(const int64_t*)data));
}
DangValueType *dang_value_type_int64(void)
{
  static DangValueType type = {
    DANG_VALUE_TYPE_MAGIC,
    0,
    "int64",
    8,                          /* sizeof */
    DANG_ALIGNOF_INT64,         /* alignof */
    NULL, NULL, NULL,
    int64_compare,
    int64_hash,
    int64_equal,
    int64_to_string,
    "operator_cast__int64",
    NULL,                       /* compile_cast_func */
    DANG_VALUE_INTERNALS_INIT
  };
  return &type;
}
IMPLEMENT_NUMERIC_COMPARE(uint64_compare, uint64_t)
#define uint64_hash int64_hash
#define uint64_equal int64_equal
static char *uint64_to_string (DangValueType *type, const void *data)
{
  DANG_UNUSED (type);
  /* TODO: portability */
  return dang_strdup_printf ("%llu", (*(const uint64_t*)data));
}
DangValueType *dang_value_type_uint64(void)
{
  static DangValueType type = {
    DANG_VALUE_TYPE_MAGIC,
    0,
    "uint64",
    8,                          /* sizeof */
    DANG_ALIGNOF_INT64,         /* alignof */
    NULL, NULL, NULL,
    uint64_compare,
    uint64_hash,
    uint64_equal,
    uint64_to_string,
    "operator_cast__uint64",
    NULL,                       /* compile_cast_func */
    DANG_VALUE_INTERNALS_INIT
  };
  return &type;
}
IMPLEMENT_NUMERIC_COMPARE(float_compare, float)
#define float_hash uint32_hash
IMPLEMENT_NUMERIC_EQUAL(float_equal, float)
static char *float_to_string (DangValueType *type, const void *data)
{
  DANG_UNUSED (type);
  return dang_strdup_printf ("%.6f", (*(const float*)data));
}
DangValueType *dang_value_type_float(void)
{
  static DangValueType type = {
    DANG_VALUE_TYPE_MAGIC,
    0,
    "float",
    4,                          /* sizeof */
    4,                          /* alignof */
    NULL, NULL, NULL,
    float_compare,
    float_hash,
    float_equal,
    float_to_string,
    "operator_cast__float",
    NULL,                       /* compile_cast_func */
    DANG_VALUE_INTERNALS_INIT
  };
  return &type;
}
IMPLEMENT_NUMERIC_COMPARE(double_compare, double)
#define double_hash uint32_hash
IMPLEMENT_NUMERIC_EQUAL(double_equal, double)
static char *double_to_string (DangValueType *type, const void *data)
{
  DANG_UNUSED (type);
  return dang_strdup_printf ("%.14f", (*(const double*)data));
}
DangValueType *dang_value_type_double(void)
{
  static DangValueType type = {
    DANG_VALUE_TYPE_MAGIC,
    0,
    "double",
    8,                          /* sizeof */
    DANG_ALIGNOF_DOUBLE,        /* alignof */
    NULL, NULL, NULL,
    double_compare,
    double_hash,
    double_equal,
    double_to_string,
    "operator_cast__double",
    NULL,                       /* compile_cast_func */
    DANG_VALUE_INTERNALS_INIT
  };
  return &type;
}
#define boolean_compare uint8_compare
#define boolean_hash uint8_hash
#define boolean_equal uint8_equal
static char *boolean_to_string (DangValueType *type, const void *data)
{
  DANG_UNUSED (type);
  return dang_strdup (*(const char*)data ? "true" : "false");
}
DangValueType *dang_value_type_boolean(void)
{
  static DangValueType type = {
    DANG_VALUE_TYPE_MAGIC,
    0,
    "boolean",
    1,                          /* sizeof */
    1,                          /* alignof */
    NULL, NULL, NULL,
    boolean_compare,
    boolean_hash,
    boolean_equal,
    boolean_to_string,
    "operator_cast__boolean",
    NULL,                       /* compile_cast_func */
    DANG_VALUE_INTERNALS_INIT
  };
  return &type;
}
#define char_compare  uint32_compare
#define char_hash     uint32_hash
#define char_equal    uint32_equal
static char *char_to_string (DangValueType *type, const void *data)
{
  dang_unichar c = * (const dang_unichar *) data;
  char buf[8];
  unsigned len = dang_utf8_encode (c, buf + 1) + 1;
  DANG_UNUSED (type);
  buf[0] = '\'';
  buf[len] = '\'';
  return dang_strndup (buf, len + 1);
}
DangValueType *dang_value_type_char(void)
{
  static DangValueType type = {
    DANG_VALUE_TYPE_MAGIC,
    0,
    "char",
    sizeof(uint32_t),           /* sizeof */
    4,                          /* alignof */
    NULL, NULL, NULL,
    char_compare,
    char_hash,
    char_equal,
    char_to_string,
    "operator_cast__char",
    NULL,                       /* compile_cast_func */
    DANG_VALUE_INTERNALS_INIT
  };
  return &type;
}
static char *reserved_pointer_to_string (DangValueType *type,
                                         const void    *pptr)
{
  const void *ptr = *(const void **) pptr;
  DANG_UNUSED (type);
  return dang_strdup_printf ("%p", ptr);
}
DangValueType *dang_value_type_reserved_pointer(void)
{
  static DangValueType type = {
    DANG_VALUE_TYPE_MAGIC,
    0,
    "reserved-pointer",
    DANG_SIZEOF_POINTER,         /* sizeof */
    DANG_ALIGNOF_POINTER,        /* alignof */
    NULL, NULL, NULL,
    NULL, NULL, NULL,           /* no compare,hash,equal */
    reserved_pointer_to_string,
    NULL, NULL,                 /* no casting */
    DANG_VALUE_INTERNALS_INIT
  };
  return &type;
}

static DANG_VALUE_COMPARE_FUNC_DECLARE(type_compare)
{
  const DangValueType *ta = * (const DangValueType**) a;
  const DangValueType *tb = * (const DangValueType**) b;
  DANG_UNUSED (type);
  return (ta < tb) ? -1 : (ta > tb) ? 1 : 0;
}
static DANG_VALUE_HASH_FUNC_DECLARE(type_hash)
{
  const DangValueType *ta = * (const DangValueType**) a;
  DANG_UNUSED (type);
  return (uint32_t) (size_t) ta;
}
static DANG_VALUE_EQUAL_FUNC_DECLARE(type_equal)
{
  const DangValueType *ta = * (const DangValueType**) a;
  const DangValueType *tb = * (const DangValueType**) b;
  DANG_UNUSED (type);
  return ta == tb;
}
static char *type_to_string (DangValueType *type,
                             const void    *pptr)
{
  const DangValueType *ptr = *(const DangValueType **) pptr;
  DANG_UNUSED (type);
  return dang_strdup (ptr ? ptr->full_name : "(null type)");
}
DangValueType *dang_value_type_type(void)
{
  static DangValueType type = {
    DANG_VALUE_TYPE_MAGIC,
    0,
    "type",
    DANG_SIZEOF_POINTER,         /* sizeof */
    DANG_ALIGNOF_POINTER,        /* alignof */
    NULL, NULL, NULL,
    type_compare,
    type_hash,
    type_equal,
    type_to_string,
    NULL, NULL,                  /* no casting */
    DANG_VALUE_INTERNALS_INIT
  };
  return &type;
}
/* --- value type: string --- */
static void
string_init_assign (DangValueType   *type,
                    void            *dst,
                    const void      *src)
{
  DangString *rhs = * (DangString **) src;
  DANG_UNUSED (type);
  if (rhs == NULL)
    * (DangString **) dst = NULL;
  else
    * (DangString **) dst = dang_string_ref_copy (rhs);
}
static void
string_assign      (DangValueType   *type,
                    void       *dst,
                    const void *src)
{
  DangString *lhs = * (DangString **) dst;
  DangString *rhs = * (DangString **) src;
  DANG_UNUSED (type);
  if (rhs != NULL)
    rhs = dang_string_ref_copy (rhs);
  if (lhs != NULL)
    dang_string_unref (lhs);
  * (DangString **) dst = rhs;
}
static void
string_destruct      (DangValueType   *type,
                      void            *value)
{
  DangString *str = * (DangString **) value;
  DANG_UNUSED (type);
  if (str != NULL)
    dang_string_unref (str);
}
static DANG_VALUE_COMPARE_FUNC_DECLARE(string_compare)
{
  DangString *sa = *(DangString**) a;
  DangString *sb = *(DangString**) b;
  DANG_UNUSED (type);
  if (sa == sb)
    return 0;
  if (sa == NULL)
    return -1;
  else if (sb == NULL)
    return 1;
  else
    return strcmp (sa->str, sb->str);
}
static DANG_VALUE_HASH_FUNC_DECLARE(string_hash)
{
  DangString *sa = *(DangString**) a;
  DANG_UNUSED (type);
  if (sa == NULL)
    return 0;
  return dang_str_hash (sa->str);
}
static DANG_VALUE_EQUAL_FUNC_DECLARE(string_equal)
{
  DangString *sa = *(DangString**) a;
  DangString *sb = *(DangString**) b;
  DANG_UNUSED (type);
  if (sa == sb)
    return TRUE;
  if (sa == NULL || sb == NULL)
    return FALSE;
  return strcmp (sa->str, sb->str) == 0;
}
static char *
string_to_string (DangValueType *type,
                  const void    *value)
{
  DangString *str = * (DangString **) value;
  DANG_UNUSED (type);
  if (str == NULL)
    return dang_strdup ("(null)");
  else
    return dang_util_c_escape (str->len, str->str, TRUE);
}
DangValueType *dang_value_type_string(void)
{
  static DangValueType type = {
    DANG_VALUE_TYPE_MAGIC,
    0,
    "string",
    sizeof(DangString*),            /* sizeof */
    DANG_ALIGNOF_POINTER,
    string_init_assign,
    string_assign,
    string_destruct,
    string_compare,
    string_hash,
    string_equal,
    string_to_string,
    "operator_cast__string",
    NULL,                       /* compile_cast_func */
    DANG_VALUE_INTERNALS_INIT
  };
  return &type;
}

/* --- value type: binary_data --- */
static void
binary_data_init_assign (DangValueType   *type,
                         void            *dst,
                         const void      *src)
{
  const DangBinaryData *rhs = * (DangBinaryData **) src;
  DANG_UNUSED (type);
  if (rhs == NULL)
    * (DangBinaryData **) dst = NULL;
  else
    * (DangBinaryData **) dst = dang_binary_data_ref_copy (rhs);
}
static void
binary_data_assign      (DangValueType   *type,
                         void       *dst,
                         const void *src)
{
  DangBinaryData *lhs = * (DangBinaryData **) dst;
  DangBinaryData *rhs = * (DangBinaryData **) src;
  DANG_UNUSED (type);
  if (rhs != NULL)
    rhs = dang_binary_data_ref_copy (rhs);
  if (lhs != NULL)
    dang_binary_data_unref (lhs);
  * (DangBinaryData **) dst = rhs;
}
static void
binary_data_destruct      (DangValueType   *type,
                           void            *value)
{
  DangBinaryData *str = * (DangBinaryData **) value;
  DANG_UNUSED (type);
  if (str != NULL)
    dang_binary_data_unref (str);
}
static DANG_VALUE_COMPARE_FUNC_DECLARE(binary_data_compare)
{
  DangBinaryData *sa = *(DangBinaryData**) a;
  DangBinaryData *sb = *(DangBinaryData**) b;
  DANG_UNUSED (type);
  if (sa == sb)
    return 0;
  if (sa == NULL)
    return -1;
  else if (sb == NULL)
    return 1;
  if (sa->len > sb->len)
    {
      int rv = memcmp (DANG_BINARY_DATA_PEEK_DATA (sa),
                       DANG_BINARY_DATA_PEEK_DATA (sb),
                       sb->len);
      return rv ? rv : 1;
    }
  else if (sa->len < sb->len)
    {
      int rv = memcmp (DANG_BINARY_DATA_PEEK_DATA (sa),
                       DANG_BINARY_DATA_PEEK_DATA (sb),
                       sa->len);
      return rv ? rv : -1;
    }
  else
    return memcmp (DANG_BINARY_DATA_PEEK_DATA (sa),
                   DANG_BINARY_DATA_PEEK_DATA (sb),
                   sa->len);
}
static DANG_VALUE_HASH_FUNC_DECLARE(binary_data_hash)
{
  DangBinaryData *sa = *(DangBinaryData**) a;
  DANG_UNUSED (type);
  if (sa == NULL)
    return 0;
  return dang_binary_data_hash (sa->len, DANG_BINARY_DATA_PEEK_DATA (sa));
}
static DANG_VALUE_EQUAL_FUNC_DECLARE(binary_data_equal)
{
  DangBinaryData *sa = *(DangBinaryData**) a;
  DangBinaryData *sb = *(DangBinaryData**) b;
  DANG_UNUSED (type);
  if (sa == sb)
    return TRUE;
  if (sa == NULL || sb == NULL)
    return FALSE;
  if (sa->len != sb->len)
    return FALSE;
  return memcmp (DANG_BINARY_DATA_PEEK_DATA (sa),
                 DANG_BINARY_DATA_PEEK_DATA (sb),
                 sa->len) == 0;
}
static char *
binary_data_to_string (DangValueType *type,
                       const void    *value)
{
  DangBinaryData *str = * (DangBinaryData **) value;
  DANG_UNUSED (type);
  if (str == NULL)
    return dang_strdup ("(null)");
  else
    {
      /* \hex_data{hexdata} */
      unsigned len = 8                  /* \hex_data */
                   + 1                  /* { */
                   + str->len * 2       /* HEX */
                   + 1                  /* } */
                   + 1;                 /* NUL */
      char *rv = dang_malloc (len);
      memcpy (rv, "\\hex_data{", 9);
      dang_util_c_hex_encode_inplace (rv + 9,
                                      str->len,
                                      DANG_BINARY_DATA_PEEK_DATA (str));
      rv[8 + str->len * 2 + 1] = 0;
      return rv;
    }
}
DangValueType *dang_value_type_binary_data(void)
{
  static DangValueType type = {
    DANG_VALUE_TYPE_MAGIC,
    0,
    "binary_data",
    sizeof(DangBinaryData*),            /* sizeof */
    DANG_ALIGNOF_POINTER,
    binary_data_init_assign,
    binary_data_assign,
    binary_data_destruct,
    binary_data_compare,
    binary_data_hash,
    binary_data_equal,
    binary_data_to_string,
    "operator_cast__binary_data",
    NULL,                       /* compile_cast_func */
    DANG_VALUE_INTERNALS_INIT
  };
  return &type;
}

///DangValueType *dang_value_type_integer(void); TODO
/* --- value type: error --- */
static void
error_init_assign (DangValueType   *type,
                   void            *dst,
                   const void      *src)
{
  DangError *rhs = * (DangError **) src;
  DANG_UNUSED (type);
  if (rhs == NULL)
    * (DangError **) dst = NULL;
  else
    * (DangError **) dst = dang_error_ref (rhs);
}
static void
error_assign      (DangValueType   *type,
                   void       *dst,
                   const void *src)
{
  DangError *lhs = * (DangError **) dst;
  DangError *rhs = * (DangError **) src;
  DANG_UNUSED (type);
  if (rhs != NULL)
    dang_error_ref (rhs);
  if (lhs != NULL)
    dang_error_unref (lhs);
  * (DangError **) dst = rhs;
}
static void
error_destruct      (DangValueType   *type,
                     void            *value)
{
  DangError *str = * (DangError **) value;
  DANG_UNUSED (type);
  if (str != NULL)
    dang_error_unref (str);
}
static DANG_VALUE_COMPARE_FUNC_DECLARE(error_compare)
{
  DangError *ea = * (DangError**) a;
  DangError *eb = * (DangError**) b;
  DANG_UNUSED (type);
  if (ea == eb)
    return 0;
  if (ea == NULL)
    return -1;
  else if (eb == NULL)
    return 1;
  else
    return strcmp (ea->message, eb->message);           /* ??? */
}
static DANG_VALUE_HASH_FUNC_DECLARE(error_hash)
{
  DangError *ea = * (DangError**) a;
  DANG_UNUSED (type);
  return ea ? dang_str_hash (ea->message) : 0;
}
static DANG_VALUE_EQUAL_FUNC_DECLARE(error_equal)
{
  DangError *ea = * (DangError**) a;
  DangError *eb = * (DangError**) b;
  DANG_UNUSED (type);
  if (ea == eb)
    return TRUE;
  if (ea == NULL || eb == NULL)
    return FALSE;
  else
    return strcmp (ea->message, eb->message) == 0;           /* ??? */
}
static char *
error_to_string (DangValueType *type,
                 const void    *value)
{
  DangError *error = * (DangError **) value;
  DANG_UNUSED (type);
  if (error == NULL)
    return dang_strdup ("error:NULL");
  else
    return dang_strdup_printf("error: %s", error->message);
}
DangValueType *dang_value_type_error(void)
{
  static DangValueType type = {
    DANG_VALUE_TYPE_MAGIC,
    0,
    "error",
    sizeof(DangError*),            /* sizeof */
    DANG_ALIGNOF_POINTER,
    error_init_assign,
    error_assign,
    error_destruct,
    error_compare,
    error_hash,
    error_equal,
    error_to_string,
    NULL, NULL,                  /* no casting */
    DANG_VALUE_INTERNALS_INIT
  };
  return &type;
}

/* --- value type: void --- */
static char *
void_to_string (DangValueType *type,
                const void    *value)
{
  DANG_UNUSED (type);
  DANG_UNUSED (value);
  return dang_strdup ("(void)");
}
DangValueType *dang_value_type_void(void)
{
  static DangValueType type = {
    DANG_VALUE_TYPE_MAGIC,
    0,
    "void",
    0,
    1,          /* align */
    NULL, NULL, NULL,
    NULL, NULL, NULL,           /* no compare,hash,equal */
    void_to_string,
    NULL, NULL,                  /* no casting */
    DANG_VALUE_INTERNALS_INIT
  };
  return &type;
}


/* --- api --- */
void *
dang_value_copy (DangValueType *type,
                 const void    *orig_value)
{
  void *rv = dang_malloc (type->sizeof_instance);
  dang_assert (orig_value != NULL);
  if (type->init_assign != NULL)
    type->init_assign (type, rv, orig_value);
  else
    memcpy (rv, orig_value, type->sizeof_instance);
  return rv;
}

void
dang_value_bulk_copy (DangValueType *type,
                      void *dst,
                      const void *src,
                      unsigned N)
{
  unsigned size = type->sizeof_instance;
  if (type->init_assign)
    {
      while (N--)
        {
          type->init_assign (type, dst, src);
          dst = (char*)dst + size;
          src = (char*)src + size;
        }
    }
  else
    {
      memcpy (dst, src, N * size);
    }
}
void
dang_value_bulk_destruct (DangValueType *type,
                          void *to_kill,
                          unsigned N)
{
  char *k = to_kill;
  if (type->destruct)
    {
      unsigned skip = type->sizeof_instance;
      while (N--)
        {
          type->destruct (type, k);
          k += skip;
        }
    }
}

void
dang_value_assign    (DangValueType *type,
                      void *dst,
                      const void *src)
{
  if (type->assign)
    type->assign (type, dst, src);
  else
    memcpy (dst, src, type->sizeof_instance);
}
void
dang_value_init_assign (DangValueType *type,
                        void *dst,
                        const void *src)
{
  if (type->init_assign)
    type->init_assign (type, dst, src);
  else
    memcpy (dst, src, type->sizeof_instance);
}
void
dang_value_destroy (DangValueType *type,
                    void *dst)
{
  if (type->destruct)
    type->destruct (type, dst);
  dang_free (dst);
}

char *
dang_value_to_string (DangValueType *type,
                      const void    *data)
{
  dang_assert (type->to_string != NULL);
  return type->to_string (type, data);
}

dang_boolean dang_value_type_is_autocast (DangValueType *lvalue,
                                          DangValueType *rvalue)
{
  if (lvalue == dang_value_type_void ())
    lvalue = NULL;
  if (rvalue == dang_value_type_void ())
    rvalue = NULL;
  if (lvalue == NULL && rvalue == NULL)
    return TRUE;
  if (lvalue == NULL || rvalue == NULL)
    return FALSE;
  while (rvalue != NULL)
    {
      if (rvalue == lvalue)
        return TRUE;
      rvalue = rvalue->internals.parent;
    }
  return FALSE;
}

#define GET_IS_RED(n)  (n)->is_red
#define SET_IS_RED(n,v)  (n)->is_red = v
#define COMPARE_ELEMENTS_BY_NAME(a,b, rv) rv = strcmp(a->name, b->name)
#define GET_TYPE_ELEMENT_TREE(type) \
  (type)->internals.element_tree, DangValueElement*, GET_IS_RED, SET_IS_RED, parent, left, right, \
  COMPARE_ELEMENTS_BY_NAME
#define GET_TYPE_CTOR_TREE(type) \
  (type)->internals.ctor_tree, DangValueElement*, GET_IS_RED, SET_IS_RED, parent, left, right, \
  COMPARE_ELEMENTS_BY_NAME

const char *dang_value_element_type_name (DangValueElementType type)
{
  switch (type)
    {
    case DANG_VALUE_ELEMENT_TYPE_MEMBER: return "member";
    case DANG_VALUE_ELEMENT_TYPE_METHOD: return "method";
    case DANG_VALUE_ELEMENT_TYPE_CTOR: return "constructor";
    }
  return "*bad-element-type*";
}
DangValueElement *dang_value_type_lookup_element (DangValueType *type,
                                                const char    *name,
                                                dang_boolean recurse,
                                                DangValueType **base_type_out)
{
  do
    {
      DangValueElement *element = NULL;
#define COMPARE_NAME_TO_ELEMENT(a,b, rv) rv = strcmp(name,b->name);
      GSK_RBTREE_LOOKUP_COMPARATOR (GET_TYPE_ELEMENT_TREE (type),
                                    name, COMPARE_NAME_TO_ELEMENT, element);
      if (element)
        {
          if (base_type_out)
            *base_type_out = type;
          return element;
        }
      if (!recurse)
        return NULL;
      type = type->internals.parent;
    }
  while (type);
  return NULL;
}

dang_boolean     dang_value_type_find_method (DangValueType  *type,
                                              const char     *name,
                                              dang_boolean    has_object,
                                              DangMatchQuery *query,
                                              DangValueType   **method_type_out,
                                              DangValueElement **elt_out,
                                              unsigned       *index_out,
                                              DangError     **error)
{
  DangValueElement *elt;

  do
    {
      unsigned i;
      elt = dang_value_type_lookup_element (type, name, FALSE, NULL);
      if (elt != NULL)
        {
          if (elt->element_type != DANG_VALUE_ELEMENT_TYPE_METHOD)
            {
              dang_set_error (error, "no %s is a member, not a method, in %s",
                              name, type->full_name);
              return FALSE;
            }

          for (i = 0; i < elt->info.methods.len; i++)
            {
              dang_boolean skip = 0;
              DangValueMethod *method = ((DangValueMethod*)elt->info.methods.data) + i;

              if (has_object && (method->flags & DANG_METHOD_STATIC) == 0)
                skip = 1;
              if (dang_function_params_test (method->sig->n_params - skip,
                                             method->sig->params + skip,
                                             query))
                {
                  *method_type_out = type;
                  *elt_out = elt;
                  *index_out = i;
                  return TRUE;
                }
            }
        }
      type = type->internals.parent;
    }
  while(type != NULL);
  return FALSE;
}

dang_boolean
dang_value_type_find_method_by_sig (DangValueType  *type,
                                    const char     *name,
                                    DangMethodFlags flags,
                                    DangSignature  *sig,
                                    DangValueType   **method_type_out,
                                    DangValueElement **elt_out,
                                    unsigned *index_out,
                                    DangError     **error)
{
  DangValueElement *elt;

  DANG_UNUSED (flags);

  do
    {
      unsigned i, j;
      elt = dang_value_type_lookup_element (type, name, FALSE, NULL);
      if (elt != NULL)
        {
          if (elt->element_type != DANG_VALUE_ELEMENT_TYPE_METHOD)
            {
              dang_set_error (error, "no %s is a member, not a method, in %s",
                              name, type->full_name);
              return FALSE;
            }


          for (i = 0; i < elt->info.methods.len; i++)
            {
              DangValueMethod *method = ((DangValueMethod*)elt->info.methods.data) + i;
              dang_boolean first_is_instance = (flags & DANG_METHOD_STATIC) == 0;
              if (method->sig->n_params != sig->n_params)
                continue;
              for (j = 0; j < method->sig->n_params; j++)
                {
                  if (first_is_instance && j == 0)
                    {
                      if (method->sig->params[0].dir != DANG_FUNCTION_PARAM_IN
                       || sig->params[0].dir != DANG_FUNCTION_PARAM_IN
                       || !dang_value_type_is_autocast (method->sig->params[0].type,
                                                        sig->params[0].type))
                        break;
                    }
                  else
                    {
                      if (method->sig->params[j].dir != sig->params[j].dir
                       || method->sig->params[j].type != sig->params[j].type)
                        break;
                    }
                }
              if (j == method->sig->n_params)
                {
                  *method_type_out = type;
                  *elt_out = elt;
                  *index_out = i;
                  return TRUE;
                }
            }
        }
      type = type->internals.parent;
    }
  while(type != NULL);
  return FALSE;
}


void dang_value_type_add_simple_member (DangValueType *type,
                                        const char    *name,
                                        DangMemberFlags flags,
                                        DangValueType *member_type,
                                        dang_boolean   dereference,
                                        unsigned       offset)
{
  DangValueElement *element = dang_new (DangValueElement, 1);
  DangValueMember *member = &element->info.member;
  DangValueElement *conflict;
  element->element_type = DANG_VALUE_ELEMENT_TYPE_MEMBER;
  element->name = dang_strdup (name);
  member->type = DANG_VALUE_MEMBER_TYPE_SIMPLE;
  member->flags = flags;
  member->member_type = member_type;
  member->info.simple.dereference = dereference;
  member->info.simple.offset = offset;
  GSK_RBTREE_INSERT (GET_TYPE_ELEMENT_TREE (type),
                     element, conflict);
  dang_assert (conflict == NULL);
}

void
dang_value_type_add_virtual_member(DangValueType *type,
                                   const char    *name,
                                   DangMemberFlags flags,
                                   DangValueType *member_type,
                                   DangCompileVirtualMember func,
                                   void          *member_data,
                                   DangDestroyNotify destroy)
{
  DangValueElement *element = dang_new (DangValueElement, 1);
  DangValueMember *member = &element->info.member;
  DangValueElement *conflict;
  element->element_type = DANG_VALUE_ELEMENT_TYPE_MEMBER;
  element->name = dang_strdup (name);
  member->type = DANG_VALUE_MEMBER_TYPE_VIRTUAL;
  member->flags = flags;
  member->member_type = member_type;
  member->info.virt.compile = func;
  member->info.virt.member_data = member_data;
  member->info.virt.member_data_destroy = destroy;
  GSK_RBTREE_INSERT (GET_TYPE_ELEMENT_TREE (type),
                     element, conflict);
  dang_assert (conflict == NULL);
}

/* --- dang_value_type_add_simple_virtual_method --- */
static DANG_SIMPLE_C_FUNC_DECLARE (simple_c__get_virtual_method)
{
  uint32_t class_offset = DANG_POINTER_TO_UINT (func_data);
  void *object = * (void **) args[0];
  void *class;
  DangFunction *func;
  if (object == NULL)
    {
      dang_set_error (error, "null-pointer exception");
      return FALSE;
    }
  class = * (void **) object;
  dang_assert (class != NULL);
  func = *(DangFunction**)((char*)class + class_offset);
  *(DangFunction**)rv_out = func ? dang_function_ref (func) : NULL;
  return TRUE;
}

void
dang_value_type_add_simple_virtual_method (DangValueType  *type,
                                           const char     *name,
                                           DangMethodFlags flags,
                                           DangSignature  *sig,
                                           unsigned        class_offset)
{
  DangValueElement *element;
  DangValueMethod *method;
  DangValueType *rv_func_type;
  DangSignature *ssig;
  DangFunctionParam param;
  GSK_RBTREE_LOOKUP_COMPARATOR (GET_TYPE_ELEMENT_TREE (type),
                                name, COMPARE_NAME_TO_ELEMENT, element);
  if (element == NULL)
    {
      DangValueElement *conflict;
      element = dang_new (DangValueElement, 1);
      element->element_type = DANG_VALUE_ELEMENT_TYPE_METHOD;
      element->name = dang_strdup (name);
      GSK_RBTREE_INSERT (GET_TYPE_ELEMENT_TREE (type),
                         element, conflict);
      DANG_UTIL_ARRAY_INIT (&element->info.methods, DangValueMethod);
      dang_assert (conflict == NULL);
    }
  else
    {
      dang_assert (element->element_type == DANG_VALUE_ELEMENT_TYPE_METHOD);
    }
  dang_util_array_set_size (&element->info.methods, element->info.methods.len + 1);
  method = ((DangValueMethod*)element->info.methods.data) 
         + (element->info.methods.len - 1);
  method->sig = dang_signature_ref (sig);
  method->method_func_type = dang_value_type_function (sig);
  method->flags = flags;
  method->func = NULL;

  /* Make method signature */
  rv_func_type = dang_value_type_function (sig);
  param.dir = DANG_FUNCTION_PARAM_IN;
  param.name = NULL;
  param.type = type;
  ssig = dang_signature_new (rv_func_type, 1, &param);

  /* Make simple-c func to do the cast */
  method->get_func = dang_function_new_simple_c (ssig, simple_c__get_virtual_method,
                                                 DANG_UINT_TO_POINTER (class_offset), NULL);
  method->method_data = NULL;
  method->offset = class_offset;
  method->method_data_destroy = NULL;
  dang_signature_unref (ssig);
}

/* --- dang_value_type_add_simple_mutable_method --- */
static DANG_SIMPLE_C_FUNC_DECLARE (simple_c__get_mutable_method)
{
  uint32_t instance_offset = DANG_POINTER_TO_UINT (func_data);
  void *object = * (void **) args[0];
  DangFunction *func;
  if (object == NULL)
    {
      dang_set_error (error, "null-pointer exception");
      return FALSE;
    }
  func = *(DangFunction**)((char*)object + instance_offset);
  *(DangFunction**)rv_out = func ? dang_function_ref (func) : NULL;
  return TRUE;
}

void
dang_value_type_add_simple_mutable_method (DangValueType  *type,
                                           const char     *name,
                                           DangMethodFlags flags,
                                           DangSignature  *sig,
                                           unsigned        instance_offset)
{
  DangValueElement *element;
  DangValueMethod *method;
  DangValueType *rv_func_type;
  DangSignature *ssig;
  DangFunctionParam param;
  GSK_RBTREE_LOOKUP_COMPARATOR (GET_TYPE_ELEMENT_TREE (type),
                                name, COMPARE_NAME_TO_ELEMENT, element);
  if (element == NULL)
    {
      DangValueElement *conflict;
      element = dang_new (DangValueElement, 1);
      element->element_type = DANG_VALUE_ELEMENT_TYPE_METHOD;
      element->name = dang_strdup (name);
      GSK_RBTREE_INSERT (GET_TYPE_ELEMENT_TREE (type),
                         element, conflict);
      DANG_UTIL_ARRAY_INIT (&element->info.methods, DangValueMethod);
      dang_assert (conflict == NULL);
    }
  else
    {
      dang_assert (element->element_type == DANG_VALUE_ELEMENT_TYPE_METHOD);
    }
  dang_util_array_set_size (&element->info.methods, element->info.methods.len + 1);
  method = ((DangValueMethod*)element->info.methods.data) 
         + (element->info.methods.len - 1);
  method->sig = dang_signature_ref (sig);
  method->method_func_type = dang_value_type_function (sig);
  method->flags = flags;
  method->func = NULL;
  /* Make method signature */
  rv_func_type = dang_value_type_function (sig);
  param.dir = DANG_FUNCTION_PARAM_IN;
  param.name = NULL;
  param.type = type;
  ssig = dang_signature_new (rv_func_type, 1, &param);

  /* Make simple-c func to do the cast */
  method->get_func = dang_function_new_simple_c (ssig, simple_c__get_mutable_method,
                                                 DANG_UINT_TO_POINTER (instance_offset), NULL);
  method->method_data = NULL;
  method->offset = instance_offset;
  method->method_data_destroy = NULL;
}

void
dang_value_type_add_constant_method (DangValueType  *type,
                                         const char    *name,
                                         DangMethodFlags flags,
                                         DangFunction  *func)
{
  DangValueElement *element;
  DangValueMethod *method;
  GSK_RBTREE_LOOKUP_COMPARATOR (GET_TYPE_ELEMENT_TREE (type),
                                name, COMPARE_NAME_TO_ELEMENT, element);
  if (element == NULL)
    {
      DangValueElement *conflict;
      element = dang_new (DangValueElement, 1);
      element->element_type = DANG_VALUE_ELEMENT_TYPE_METHOD;
      element->name = dang_strdup (name);
      GSK_RBTREE_INSERT (GET_TYPE_ELEMENT_TREE (type),
                         element, conflict);
      DANG_UTIL_ARRAY_INIT (&element->info.methods, DangValueMethod);
      dang_assert (conflict == NULL);
    }
  else
    {
      dang_assert (element->element_type == DANG_VALUE_ELEMENT_TYPE_METHOD);
    }
  dang_util_array_set_size (&element->info.methods, element->info.methods.len + 1);
  method = ((DangValueMethod*)element->info.methods.data) 
         + (element->info.methods.len - 1);
  method->sig = dang_signature_ref (func->base.sig);
  method->method_func_type = dang_value_type_function (func->base.sig);
  method->flags = flags;
  method->func = dang_function_attach_ref (func);
  method->get_func = NULL;
  method->method_data = NULL;
  method->method_data_destroy = NULL;
}

/* --- dang_value_type_add_ctor --- */
void dang_value_type_add_ctor           (DangValueType *type,
                                         const char    *name,
                                         DangFunction  *ctor)
{
  DangValueElement *element;
  if (name == NULL)
    name = "";
  dang_assert (ctor->base.sig->return_type == type);
  GSK_RBTREE_LOOKUP_COMPARATOR (GET_TYPE_CTOR_TREE (type),
                                name, COMPARE_NAME_TO_ELEMENT, element);
  if (element == NULL)
    {
      DangValueElement *conflict;
      element = dang_new (DangValueElement, 1);
      element->element_type = DANG_VALUE_ELEMENT_TYPE_CTOR;
      element->name = dang_strdup (name);
      GSK_RBTREE_INSERT (GET_TYPE_CTOR_TREE (type),
                         element, conflict);
      element->info.ctor = dang_function_family_new (type->full_name);
      dang_assert (conflict == NULL);
    }
  else
    dang_assert (element->element_type == DANG_VALUE_ELEMENT_TYPE_CTOR);
  dang_function_family_container_add_function (element->info.ctor, ctor);
}
DangFunctionFamily *
dang_value_type_get_ctor           (DangValueType *type,
                                    const char    *name)
{
  DangValueElement *element;
  if (name == NULL)
    name = "";
  GSK_RBTREE_LOOKUP_COMPARATOR (GET_TYPE_CTOR_TREE (type),
                                name, COMPARE_NAME_TO_ELEMENT, element);
  if (element == NULL)
    return NULL;
  dang_assert (element->element_type == DANG_VALUE_ELEMENT_TYPE_CTOR);
  return element->info.ctor;
}

static void
value_element_tree_free_recursive (DangValueElement *element)
{
  if (element == NULL)
    return;
  value_element_tree_free_recursive (element->left);
  value_element_tree_free_recursive (element->right);
  dang_free (element->name);
  switch (element->element_type)
    {
    case DANG_VALUE_ELEMENT_TYPE_MEMBER:
      if (element->info.member.type == DANG_VALUE_MEMBER_TYPE_VIRTUAL
       && element->info.member.info.virt.member_data_destroy != NULL)
        element->info.member.info.virt.member_data_destroy (element->info.member.info.virt.member_data);
      break;
      
    case DANG_VALUE_ELEMENT_TYPE_METHOD:
      {
        unsigned n_methods = element->info.methods.len;
        DangValueMethod *methods = element->info.methods.data;
        unsigned i;
        for (i = 0; i < n_methods; i++)
          {
            dang_signature_unref (methods[i].sig);
            if (methods[i].func != NULL)
              dang_function_unref (methods[i].func);
            if (methods[i].get_func != NULL)
              dang_function_unref (methods[i].get_func);
            if (methods[i].method_data_destroy != NULL)
              methods[i].method_data_destroy (methods[i].method_data);
          }
        dang_free (methods);
      }
      break;
    case DANG_VALUE_ELEMENT_TYPE_CTOR:
      dang_function_family_unref (element->info.ctor);
      break;
    }
  dang_free (element);
}

void dang_value_type_cleanup (DangValueType *type)
{
  value_element_tree_free_recursive (type->internals.element_tree);
  value_element_tree_free_recursive (type->internals.ctor_tree);
}

