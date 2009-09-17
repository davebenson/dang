#include "dang.h"
#include "magic.h"
#include "config.h"

static void
namespace_init_assign (DangValueType   *type,
                    void            *dst,
                    const void      *src)
{
  DangNamespace *rhs = * (DangNamespace **) src;
  DANG_UNUSED (type);
  if (rhs == NULL)
    * (DangNamespace **) dst = NULL;
  else
  * (DangNamespace **) dst = dang_namespace_ref (rhs);
}
static void
namespace_assign      (DangValueType   *type,
                  void       *dst,
                  const void *src)
{
DangNamespace *lhs = * (DangNamespace **) dst;
DangNamespace *rhs = * (DangNamespace **) src;
DANG_UNUSED (type);
if (rhs != NULL)
  rhs = dang_namespace_ref (rhs);
if (lhs != NULL)
  dang_namespace_unref (lhs);
  * (DangNamespace **) dst = rhs;
}
static void
namespace_destruct      (DangValueType   *type,
                      void            *value)
{
  DangNamespace *str = * (DangNamespace **) value;
  DANG_UNUSED (type);
  if (str != NULL)
    dang_namespace_unref (str);
}
static char *
namespace_to_string (DangValueType *type,
                  const void    *value)
{
  DangNamespace *str = * (DangNamespace **) value;
  DANG_UNUSED (type);
  if (str == NULL)
    return dang_strdup ("(null)");
  else
    return dang_strdup_printf ("namespace: %s", str->full_name);
}
DangValueType *dang_value_type_namespace(void)
{
  static DangValueType type = {
    DANG_VALUE_TYPE_MAGIC,
    0,
    "namespace",
    sizeof(DangNamespace*),            /* sizeof */
    DANG_ALIGNOF_POINTER,
    namespace_init_assign,
    namespace_assign,
    namespace_destruct,
    NULL, NULL, NULL,                  /* no compare,hash,equal */
    namespace_to_string,
    NULL, NULL,                        /* no casting */
    DANG_VALUE_INTERNALS_INIT
  };
  return &type;
}
