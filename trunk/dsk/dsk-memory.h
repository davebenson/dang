
typedef struct _DskMemorySourceClass DskMemorySourceClass;
typedef struct _DskMemorySource DskMemorySource;
typedef struct _DskMemorySinkClass DskMemorySinkClass;
typedef struct _DskMemorySink DskMemorySink;

#define DSK_MEMORY_SOURCE(object) DSK_OBJECT_CAST(DskMemorySource, object, &dsk_memory_source_class)
#define DSK_MEMORY_SINK(object) DSK_OBJECT_CAST(DskMemorySink, object, &dsk_memory_sink_class)

struct _DskMemorySourceClass
{
  DskOctetSourceClass base_class;
};
struct _DskMemorySource
{
  DskOctetSource base_instance;
  DskBuffer buffer;

  /* emit 'buffer_low' while buffer.size <= buffer_low_amount */
  DskHook buffer_low;
  unsigned buffer_low_amount;

  dsk_boolean done_adding;
};
struct _DskMemorySinkClass
{
  DskOctetSinkClass base_class;
};
struct _DskMemorySink
{
  DskOctetSink base_instance;
  DskBuffer buffer;
  DskHook buffer_nonempty;
  dsk_boolean got_shutdown;
  unsigned max_buffer_size;
};

#define dsk_memory_source_new()  (DskMemorySource *) dsk_object_new (&dsk_memory_source_class)
#define dsk_memory_sink_new()  (DskMemorySink *) dsk_object_new (&dsk_memory_sink_class)

/* used to pump data into the DskMemorySource */
void dsk_memory_source_done_adding (DskMemorySource *source);
void dsk_memory_source_added_data  (DskMemorySource *source);

void dsk_memory_sink_drained (DskMemorySink *sink);

extern DskMemorySourceClass dsk_memory_source_class;
extern DskMemorySinkClass dsk_memory_sink_class;
