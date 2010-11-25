typedef struct _DskJsonMember DskJsonMember;
typedef struct _DskJsonValue DskJsonValue;

typedef enum
{
  DSK_JSON_VALUE_BOOLEAN,
  DSK_JSON_VALUE_NULL,
  DSK_JSON_VALUE_OBJECT,
  DSK_JSON_VALUE_ARRAY,
  DSK_JSON_VALUE_STRING,
  DSK_JSON_VALUE_NUMBER
} DskJsonValueType;

struct _DskJsonValue
{
  DskJsonValueType type;
  union {
    dsk_boolean v_boolean;
    struct {
      unsigned n_members;
      DskJsonMember *members;
    } v_object;
    struct {
      unsigned n;
      DskJsonValue *values;
    } v_array;
    char *v_string;
    double v_number;
  } value;
};

struct _DskJsonMember
{
  char *name;
  DskJsonValue value;
};
