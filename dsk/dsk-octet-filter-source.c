
struct _DskOctetSourceFilterClass
{
  DskOctetSourceClass base_class;
};

struct _DskOctetSourceFilter
{
  DskOctetSource base_instance;
  DskBuffer buffer;
  DskOctetFilter *filter;
  DskOctetSource *sub;
};

static dsk_boolean
read_into_buffer (DskOctetSourceFilter *filter,
                  DskError            **error)
{
  ...
}

static DskIOResult
dsk_octet_source_filter_read (DskOctetSource *source,
                              unsigned        max_len,
                              void           *data_out,
                              unsigned       *bytes_read_out,
                              DskError      **error)
{
  DskOctetSourceFilter *sf = (DskOctetSourceFilter *) source;
  if (sf->buffer.size > 0)
    {
      ...
    }
  else
    {
      ...
    }

  dsk_hook_set_idle_notify (&source->readable_hook, sf->buffer.size > 0);
  if (sf->read_trap == NULL && dsk_hook_is_trapped (&source->readable_hook))
    sf->read_trap = dsk_hook_trap (sf->sub->readable_hook, ...);
  return DSK_IO_RESULT_SUCCESS;
}

static DskIOResult
dsk_octet_source_filter_read_buffer (DskOctetSource *source,
                                     DskBuffer      *read_buffer,
                                     DskError      **error)
{
  ...
}


static void       
dsk_octet_source_filter_shutdown  (DskOctetSource *source)
{
  ...
}

static void
dsk_octet_source_filter_finalize (DskOctetSourceFilter *sf)
{
  if (sf->read_trap)
    dsk_hook_trap_destroy (sf->read_trap);
  if (sf->source)
    dsk_object_unref (sf->source);
  if (sf->filter)
    dsk_object_unref (sf->filter);
  dsk_buffer_clear (&sf->buffer);
}

static DskOctetSourceFilterClass dsk_octet_source_filter_class =
{
  { DSK_OBJECT_CLASS_DEFINE (DskOctetSourceFilter, &dsk_octet_source_class, 
                             NULL, dsk_octet_source_filter_finalize),
    dsk_octet_source_filter_read,
    dsk_octet_source_filter_read_buffer,
    dsk_octet_source_filter_shutdown
  }
};

DskOctetSource *dsk_octet_filter_source (DskOctetSource *source,
                                         DskOctetFilter *filter)
{
  DskOctetSourceFilter *sf = dsk_object_new (&dsk_octet_source_filter_class);
  sf->source = dsk_object_ref (source);
  sf->filter = dsk_object_ref (filter);
  dsk_hook_set_poll_funcs (&sf->base_instance->readable_hook, &poll_funcs);
  return DSK_OCTET_SOURCE (sf);
}
