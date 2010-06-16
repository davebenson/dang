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
parse_comma_sep (const char *flag_str,
                 uint8_t    *flags_out,
                 DskError  **error,
                 const char *name1,
                 ...)
{
  const char *names[256];
  const char *name = name1;
  unsigned n_flags = 0;
  va_list args;
  va_start (args, name1);
  do
    {
      flags_out[n_flags] = 0;
      names[n_flags++] = name;
      name = va_arg (args, const char *);
    }
  while (name != NULL);
  va_end (args);
  
  while (*flag_str)
    {
      const char *start = flag_str;
      const char *end;
      while (dsk_ascii_isspace (*start))
        start++;
      while (!dsk_ascii_isspace (*end) && *end != ',')
        end++;
      if (start < end)
        {
          unsigned i;
          for (i = 0; i < n_flags; i++)
            if (strncmp (start, names[i], end-start) == 0
                && names[i][end-start] == 0)
              break;
          if (i == n_flags)
            {
              dsk_set_error (error, "bad flag '%.*s'", (int)(end-start), start);
              return DSK_FALSE;
            }
          flags_out[i] = 1;
        }
      while (dsk_ascii_isspace (*end))
        end++;
      if (*end == ',')
        end++;
      flag_str = end;
    }
  return DSK_TRUE;
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
  dsk_boolean add_quotes = DSK_TRUE, protect_trigraphs = DSK_FALSE;
  DSK_UNUSED (arg_name); DSK_UNUSED (callback_data); DSK_UNUSED (error);
  if (arg_value)
    {
      uint8_t flags[4];
      if (!parse_comma_sep (arg_value, flags, error,
                            "quotes", "noquotes",
                            "trigraphs", "notrigraphs", NULL))
        return DSK_FALSE;
      if (flags[0])
        add_quotes = DSK_TRUE;
      else if (flags[1])
        add_quotes = DSK_FALSE;
      if (flags[2])
        protect_trigraphs = DSK_TRUE;
      else if (flags[3])
        protect_trigraphs = DSK_FALSE;
    }

  add_filter (dsk_c_quoter_new (add_quotes, protect_trigraphs));
  return DSK_TRUE;
}
DSK_CMDLINE_CALLBACK_DECLARE(handle_c_unquote)
{
  DSK_UNUSED (arg_name); DSK_UNUSED (callback_data); DSK_UNUSED (error);
  dsk_boolean remove_quotes = DSK_TRUE;
  if (arg_value)
    {
      uint8_t flags[2];
      if (!parse_comma_sep (arg_value, flags, error,
                            "quotes", "noquotes", NULL))
        return DSK_FALSE;
      if (flags[0])
        remove_quotes = DSK_TRUE;
      else if (flags[1])
        remove_quotes = DSK_FALSE;
    }

  add_filter (dsk_c_unquoter_new (remove_quotes));
  return DSK_TRUE;
}

DSK_CMDLINE_CALLBACK_DECLARE(handle_base64_encode)
{
  DSK_UNUSED (arg_name); DSK_UNUSED (callback_data); DSK_UNUSED (arg_value); DSK_UNUSED (error);
  add_filter (dsk_base64_encoder_new (DSK_TRUE));
  return DSK_TRUE;
}
DSK_CMDLINE_CALLBACK_DECLARE(handle_base64_encode_oneline)
{
  DSK_UNUSED (arg_name); DSK_UNUSED (callback_data); DSK_UNUSED (arg_value); DSK_UNUSED (error);
  add_filter (dsk_base64_encoder_new (DSK_FALSE));
  return DSK_TRUE;
}
DSK_CMDLINE_CALLBACK_DECLARE(handle_base64_decode)
{
  DSK_UNUSED (arg_name); DSK_UNUSED (callback_data); DSK_UNUSED (arg_value); DSK_UNUSED (error);
  add_filter (dsk_base64_decoder_new ());
  return DSK_TRUE;
}
DSK_CMDLINE_CALLBACK_DECLARE(handle_hex_encode)
{

  dsk_boolean newlines = DSK_FALSE, spaces = DSK_FALSE;
  DSK_UNUSED (arg_name); DSK_UNUSED (callback_data); DSK_UNUSED (error);
  if (arg_value)
    {
      uint8_t flags[4];
      if (!parse_comma_sep (arg_value, flags, error,
                            "newlines", "nonewlines",
                            "spaces", "nospaces", NULL))
        return DSK_FALSE;
      if (flags[0])
        newlines = DSK_TRUE;
      else if (flags[1])
        newlines = DSK_FALSE;
      if (flags[2])
        spaces = DSK_TRUE;
      else if (flags[3])
        spaces = DSK_FALSE;
    }
  add_filter (dsk_hex_encoder_new (newlines, spaces));
  return DSK_TRUE;
}
DSK_CMDLINE_CALLBACK_DECLARE(handle_hex_decode)
{
  DSK_UNUSED (arg_name); DSK_UNUSED (callback_data); DSK_UNUSED (arg_value); DSK_UNUSED (error);
  add_filter (dsk_hex_decoder_new ());
  return DSK_TRUE;
}

int main(int argc, char **argv)
{
  DskError *error = NULL;
  DskOctetFilter *filter;
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
  dsk_cmdline_add_func ("base64-encode", "do Base-64 Encoding", NULL, 0,
                        handle_base64_encode, NULL);
  dsk_cmdline_add_func ("base64-encode-oneline", "do Base-64 Encoding, without line breaks", NULL, 0,
                        handle_base64_encode_oneline, NULL);
  dsk_cmdline_add_func ("base64-decode", "do Base-64 Decoding", NULL, 0,
                        handle_base64_decode, NULL);
  dsk_cmdline_add_func ("hex-encode", "do Hex-64 Encoding (FLAGS may be 'spaces' or 'newlines')",
                        "FLAGS", DSK_CMDLINE_OPTIONAL,
                        handle_hex_encode, NULL);
  dsk_cmdline_add_func ("hex-decode", "do Hex Decoding", NULL, 0,
                        handle_hex_decode, NULL);
  dsk_cmdline_process_args (&argc, &argv);

  DskBuffer in = DSK_BUFFER_STATIC_INIT;
  DskBuffer out = DSK_BUFFER_STATIC_INIT;

  filter = dsk_octet_filter_chain_new_take (n_filters, filters);
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
      if (!dsk_octet_filter_process_buffer (filter, &out, in.size, &in, DSK_TRUE, &error))
        dsk_die ("error running filter: %s", error->message);
      while (out.size)
        {
          int rv = dsk_buffer_writev (&out, 1);
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
  if (!dsk_octet_filter_finish (filter, &in, &error))
    dsk_die ("error finishing filter: %s", error->message);
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
  dsk_assert (in.size == 0);
  dsk_assert (out.size == 0);
  dsk_object_unref (filter);
  dsk_cleanup ();
  return 0;
}
