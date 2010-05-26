#include "dsk.h"

static void
dsk_memory_source_init (DskMemorySource *source)
{
  dsk_hook_init (&source->buffer_empty, source);
}

static void
dsk_memory_source_finalize (DskMemorySource *source)
{
  dsk_hook_clear (&source->buffer_empty);
}

static DskIOResult 
dsk_memory_source_read        (DskOctetSource *source,
                               unsigned        max_len,
                               void           *data_out,
                               unsigned       *bytes_read_out,
                               DskError      **error)
{
  DskMemorySource *msource = DSK_MEMORY_SOURCE (source);
  DSK_UNUSED (error);
  *bytes_read_out = dsk_buffer_read (&msource->buffer, max_len, data_out);
  if (msource->buffer.size == 0)
    dsk_hook_set_idle_notify (&msource->buffer_empty, DSK_TRUE);
  return *bytes_read_out == 0 ? DSK_IO_RESULT_AGAIN : DSK_IO_RESULT_SUCCESS;
}

static DskIOResult
dsk_memory_source_read_buffer (DskOctetSource *source,
                               DskBuffer      *read_buffer,
                               DskError      **error)
{
  DskMemorySource *msource = DSK_MEMORY_SOURCE (source);
  DSK_UNUSED (error);
  if (dsk_buffer_drain (read_buffer, &msource->buffer) == 0)
    return DSK_IO_RESULT_AGAIN;
  dsk_hook_set_idle_notify (&msource->buffer_empty, DSK_TRUE);
  dsk_hook_set_idle_notify (&source->readable_hook, DSK_FALSE);
  return DSK_IO_RESULT_SUCCESS;
}

DSK_OBJECT_CLASS_DEFINE_CACHE_DATA (DskMemorySource);
DskMemorySourceClass dsk_memory_source_class =
{
  {
    DSK_OBJECT_CLASS_DEFINE(DskMemorySource,
                            &dsk_octet_source_class,
                            dsk_memory_source_init,
                            dsk_memory_source_finalize),
    dsk_memory_source_read,
    dsk_memory_source_read_buffer,
    NULL
  }
};

void dsk_memory_source_done_adding (DskMemorySource *source)
{
  source->done_adding = DSK_TRUE;
  dsk_hook_set_idle_notify (&DSK_OCTET_SOURCE (source)->readable_hook, DSK_TRUE);
}

void dsk_memory_source_added_data  (DskMemorySource *source)
{
  dsk_assert (!source->done_adding);
  if (source->buffer.size != 0)
    {
      dsk_warning ("dsk_memory_source_added_data: trap_count=%u", DSK_OCTET_SOURCE (source)->readable_hook.trap_count);
      dsk_hook_set_idle_notify (&DSK_OCTET_SOURCE (source)->readable_hook, DSK_TRUE);
      dsk_hook_set_idle_notify (&source->buffer_empty, DSK_FALSE);
    }
}

