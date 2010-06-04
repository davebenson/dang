#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../dsk.h"

#define MAX_FILTERS 64

static DskOctetFilter *filters[MAX_FILTERS];
static unsigned        n_filters = 0;

static void
add_filter (DskOctetFilter *filter)
{
  if (n_filters == MAX_FILTERS)
    dsk_die ("too many filters");
  filters[n_filters++] = filter;
}

static dsk_boolean
handle_generic_compress (DskZlibMode mode, const char *arg_value, DskError **error)
{
  int level = atoi (arg_value);
  if (level < 0 || level > 9)
    {
      dsk_set_error (error, "bad level %s", arg_value);
      return DSK_FALSE;
    }
  add_filter (dsk_zlib_compressor_new (mode, level));
  return DSK_TRUE;
}
DSK_CMDLINE_CALLBACK_DECLARE(handle_zlib_compress)
{
  DSK_UNUSED (arg_name); DSK_UNUSED (callback_data);
  return handle_generic_compress (DSK_ZLIB_DEFAULT, arg_value, error);
}
DSK_CMDLINE_CALLBACK_DECLARE(handle_raw_zlib_compress)
{
  DSK_UNUSED (arg_name); DSK_UNUSED (callback_data);
  return handle_generic_compress (DSK_ZLIB_RAW, arg_value, error);
}
DSK_CMDLINE_CALLBACK_DECLARE(handle_gzip_compress)
{
  DSK_UNUSED (arg_name); DSK_UNUSED (callback_data);
  return handle_generic_compress (DSK_ZLIB_GZIP, arg_value, error);
}


static dsk_boolean
handle_generic_decompress (DskZlibMode mode, DskError **error)
{
  DSK_UNUSED (error);
  add_filter (dsk_zlib_decompressor_new (mode));
  return DSK_TRUE;
}
DSK_CMDLINE_CALLBACK_DECLARE(handle_zlib_decompress)
{
  DSK_UNUSED (arg_name); DSK_UNUSED (callback_data); DSK_UNUSED (arg_value);
  return handle_generic_decompress (DSK_ZLIB_DEFAULT, error);
}
DSK_CMDLINE_CALLBACK_DECLARE(handle_raw_zlib_decompress)
{
  DSK_UNUSED (arg_name); DSK_UNUSED (callback_data); DSK_UNUSED (arg_value);
  return handle_generic_decompress (DSK_ZLIB_RAW, error);
}
DSK_CMDLINE_CALLBACK_DECLARE(handle_gzip_decompress)
{
  DSK_UNUSED (arg_name); DSK_UNUSED (callback_data); DSK_UNUSED (arg_value);
  return handle_generic_decompress (DSK_ZLIB_GZIP, error);
}
DSK_CMDLINE_CALLBACK_DECLARE(handle_c_quote)
{
  DSK_UNUSED (arg_name); DSK_UNUSED (callback_data); DSK_UNUSED (arg_value); DSK_UNUSED (error);
  add_filter (dsk_c_quoter_new ());
  return DSK_TRUE;
}
DSK_CMDLINE_CALLBACK_DECLARE(handle_c_unquote)
{
  DSK_UNUSED (arg_name); DSK_UNUSED (callback_data); DSK_UNUSED (arg_value); DSK_UNUSED (error);
  add_filter (dsk_c_unquoter_new ());
  return DSK_TRUE;
}

DSK_CMDLINE_CALLBACK_DECLARE(handle_base64)
{
  DSK_UNUSED (arg_name); DSK_UNUSED (callback_data); DSK_UNUSED (arg_value); DSK_UNUSED (error);
  add_filter (dsk_base64_encoder_new (DSK_TRUE));
  return DSK_TRUE;
}
DSK_CMDLINE_CALLBACK_DECLARE(handle_base64_oneline)
{
  DSK_UNUSED (arg_name); DSK_UNUSED (callback_data); DSK_UNUSED (arg_value); DSK_UNUSED (error);
  add_filter (dsk_base64_encoder_new (DSK_FALSE));
  return DSK_TRUE;
}

int main(int argc, char **argv)
{
  DskError *error = NULL;
  DskBuffer swap;
  unsigned i, j;
  dsk_cmdline_init ("run various filters",
                    "Run a chain of DskOctetFilters, mostly useful for testing.\n",
                    NULL, 0);
  dsk_cmdline_add_func ("zlib-compress", "do zlib compression", "LEVEL", 0,
                        handle_zlib_compress, NULL);
  dsk_cmdline_add_func ("raw-zlib-compress", "do raw zlib compression", "LEVEL", 0,
                        handle_raw_zlib_compress, NULL);
  dsk_cmdline_add_func ("gzip-compress", "do raw zlib compression", "LEVEL", 0,
                        handle_gzip_compress, NULL);
  dsk_cmdline_add_func ("zlib-decompress", "do zlib decompression", NULL, 0,
                        handle_zlib_decompress, NULL);
  dsk_cmdline_add_func ("raw-zlib-decompress", "do raw zlib decompression", NULL, 0,
                        handle_raw_zlib_decompress, NULL);
  dsk_cmdline_add_func ("gzip-decompress", "do raw zlib decompression", NULL, 0,
                        handle_gzip_decompress, NULL);
  dsk_cmdline_add_func ("c-quote", "do c quoting", NULL, 0,
                        handle_c_quote, NULL);
  dsk_cmdline_add_func ("c-unquote", "do c unquoting", NULL, 0,
                        handle_c_unquote, NULL);
  dsk_cmdline_add_func ("base64", "do Base-64 Encoding", NULL, 0,
                        handle_base64, NULL);
  dsk_cmdline_add_func ("base64-oneline", "do Base-64 Encoding, without line breaks", NULL, 0,
                        handle_base64_oneline, NULL);
  dsk_cmdline_process_args (&argc, &argv);

  DskBuffer in = DSK_BUFFER_STATIC_INIT;
  DskBuffer out = DSK_BUFFER_STATIC_INIT;

  for (;;)
    {
      int rv = dsk_buffer_readv (&in, 0);
      if (rv < 0)
        {
          if (errno == EAGAIN || errno == EINTR)
            continue;
          dsk_die ("error reading from standard-input");
        }
      if (rv == 0)
        break;
      for (i = 0; i < n_filters; i++)
        {
          if (!dsk_octet_filter_process_buffer (filters[i], &out, in.size, &in, DSK_TRUE, &error))
            dsk_die ("error running filter: %s", error->message);
          swap = in;
          in = out;
          out = swap;
        }
      while (in.size)
        {
          int rv = dsk_buffer_writev (&in, 1);
          if (rv < 0)
            {
              if (errno == EAGAIN || errno == EINTR)
                continue;
              dsk_die ("error writing to standard-output: %s",
                       strerror (errno));
            }
        }
      dsk_assert (in.size == 0);
      dsk_assert (out.size == 0);
    }

  /* finish the filters */
  for (i = 0; i < n_filters; i++)
    {
      if (!dsk_octet_filter_finish (filters[i], &in, &error))
        dsk_die ("error finishing filter: %s", error->message);
      for (j = i + 1; in.size && j < n_filters; j++)
        {
          if (!dsk_octet_filter_process_buffer (filters[j], &out, in.size, &in, DSK_TRUE, &error))
            dsk_die ("error processing finishing filter: %s", error->message);
          swap = in;
          in = out;
          out = swap;
        }
      while (in.size != 0)
        {
          int rv = dsk_buffer_writev (&in, 1);
          if (rv < 0)
            {
              if (errno == EAGAIN || errno == EINTR)
                continue;
              dsk_die ("error writing to standard-output: %s",
                       strerror (errno));
            }
        }
    }
  dsk_assert (in.size == 0);
  dsk_assert (out.size == 0);
  for (i = 0; i < n_filters; i++)
    dsk_object_unref (filters[i]);
  dsk_cleanup ();
  return 0;
}
