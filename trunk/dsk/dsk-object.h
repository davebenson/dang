
struct _DskObjectClass
{
  const char *name;
  DskObjectClass *parent_class;
  void (*finalize) (DskObject *object);
};

struct _DskObject
{
  DskObjectClass *object_class;
  unsigned ref_count;
};
