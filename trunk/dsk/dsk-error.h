
typedef struct _DskErrorClass DskErrorClass;
typedef struct _DskError DskError;

struct _DskErrorClass
{
  DskObjectClass base_class;
};

struct _DskError
{
  DskObject base_instance;
  char *message;
};

extern DskErrorClass dsk_error_class;

DskError *dsk_error_new        (const char *format,
                                ...);
DskError *dsk_error_new_valist (const char *format,
                                va_list     args);
DskError *dsk_error_new_literal(const char *message);
DskError *dsk_error_ref        (DskError   *error);
void      dsk_error_unref      (DskError   *error);
