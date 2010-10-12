#include <string.h>
#include <ctype.h>
#include "dsk.h"

/* Relevant RFC's.
2045: MIME: Format of Message Bodies
2046: MIME: Media Types
 */

#define DEBUG_MIME_DECODER 0

/* Relevant, but the extensions are mostly unimplemented:
2183: Content-Disposition header type
2387: mime type: MultipartDecoder/Related
2388: mime type: MultipartDecoder/Form-Data
 */

/* TODO: the lines in headers can not be broken up with newlines yet! */

typedef enum
{
  /* multipart_decoder has been constructed,
     but has not yet gotten an array of options.
     (this distinction is a bit of a hack.) */
  STATE_INITED,

  /* has had set_options() called on it, successfully */
  STATE_WAITING_FOR_FIRST_SEP_LINE,

  /* Gathering header of next piece in 'buffer'.

     When the full header is gathered, a new 
     last_piece will be allocated.
     (this may cause writability notification to start.)

     However, if the 'part' is in memory mode,
     but the full data has not been received,
     then it is NOT appropriate to notify the user
     of the availability of this part, because
     memory mode implies that we gather the 
     full content before notifying the user.  */
  STATE_READING_HEADER,

  /* In these state, content must be written to
     FEED_STREAM, noting that we must
     scan for the "boundary".

     We generally process the entire buffer,
     except if there is ambiguity and then we 
     keep the whole beginning of the line around.

     So the parse algorithm is:
       while we have data to be processed:
         Scan for line starts,
	 noting what they are followed with:
	   - Is Boundary?
	     Process pending data, shutdown and break.
	   - Is Boundary prefix?
	     Process pending data, and break.
	   - Cannot be boundary?
	     Continue scanning for next line start.
	     The line is considered pending data.
   */
  STATE_CONTENT_LINE_START,
  STATE_CONTENT_MIDLINE,

  STATE_ENDED
} State;

typedef struct _DskMimeMultipartDecoderClass DskMimeMultipartDecoderClass;
struct _DskMimeMultipartDecoderClass
{
  DskObjectClass base_class;
};

struct _DskMimeMultipartDecoder
{
  DskObject base_instance;
  State state;
  DskBuffer incoming;

  /* configuration from the Content-Type header */
  char *start, *start_info, *boundary_str;

  /* cached for optimization */
  unsigned boundary_str_len;

  DskCgiVar cur_piece;
  DskBuffer cur_buffer;
  DskOctetFilter *transfer_decoder;
  DskMemPool header_storage;
#define DSK_MIME_MULTIPART_DECODER_INIT_HEADER_SPACE  1024
  char *header_pad;
  
  unsigned n_pieces;
  unsigned pieces_alloced;
  DskCgiVar *pieces;
#define DSK_MIME_MULTIPART_DECODER_N_INIT_PIECES 4
  DskCgiVar pieces_init[DSK_MIME_MULTIPART_DECODER_N_INIT_PIECES];
};



/* --- functions --- */

#if DEBUG_MIME_DECODER
static const char *
state_to_string (guint state)
{
  switch (state)
    {
#define CASE_RET_AS_STRING(st)  case st: return #st
    CASE_RET_AS_STRING(STATE_INITED);
    CASE_RET_AS_STRING(STATE_WAITING_FOR_FIRST_SEP_LINE);
    CASE_RET_AS_STRING(STATE_READING_HEADER);
    CASE_RET_AS_STRING(STATE_CONTENT_LINE_START);
    CASE_RET_AS_STRING(STATE_CONTENT_MIDLINE);
    CASE_RET_AS_STRING(STATE_ENDED);
#undef CASE_RET_AS_STRING
    }
  g_return_val_if_reached (NULL);
}
#define YELL(decoder) g_message ("at line %u, state=%s [buf-size=%u]",__LINE__,state_to_string ((decoder)->state), (decoder)->buffer.size)
#endif

static dsk_boolean
process_header_line (DskMimeMultipartDecoder *decoder,
                     const char              *line,
                     DskError               **error)
{
  DskCgiVar *cur = &decoder->cur_piece;
  if (dsk_ascii_strncasecmp (line, "content-type:", 13) == 0
   && cur->content_type == NULL)
    {
      /* Example:
	 text/plain; charset=latin-1

	 See RFC 2045, Section 5.
       */
      const char *start = line + 13;
      const char *slash = strchr (start, '/');
      const char *at;
      GPtrArray *fields = NULL;
      GSK_SKIP_WHITESPACE (start);
      if (slash == NULL)
	{
	  g_set_error (error, GSK_G_ERROR_DOMAIN,
		       GSK_ERROR_BAD_FORMAT,
		       _("content-type expected to contain a '/'"));
	  return FALSE;
	}
      piece->type = g_strndup (start, slash-start);
      slash++;
      at = slash;
#define IS_SUBTYPE_CHAR(c) (isalpha(c) || isdigit(c) || ((c)=='-'))
#define IS_SEP_TYPE_CHAR(c) (isspace(c) || ((c)==';'))
      GSK_SKIP_CHAR_TYPE (at, IS_SUBTYPE_CHAR);
      piece->subtype = g_strndup (slash, at-slash);
      for (;;)
	{
	  const char *value_start, *value_end;
	  const char *equals;
	  char *key, *value;
	  GSK_SKIP_CHAR_TYPE (at, IS_SEP_TYPE_CHAR);
	  if (*at == '\0')
	    break;
	  equals = at;
	  GSK_SKIP_CHAR_TYPE (equals, IS_SUBTYPE_CHAR);
	  if (*equals != '=')
	    {
	      g_set_error (error, GSK_G_ERROR_DOMAIN,
			   GSK_ERROR_BAD_FORMAT,
			   _("expected '=' in key-value pairs on content-type"));
	      return FALSE;
	    }
	  value_start = equals + 1;
	  value_end = value_start;
	  if (*value_start == '"')
	    {
	      value_end = strchr (value_start + 1, '"');
	      if (value_end == NULL)
		{
		  g_set_error (error, GSK_G_ERROR_DOMAIN,
			       GSK_ERROR_BAD_FORMAT,
			       _("missing terminal '\"' in key/value pair in content-type"));
		  return FALSE;
		}
	      value = g_strndup (value_start + 1, value_end - (value_start + 1));
	      value_end++;
	    }
	  else
	    {
	      GSK_SKIP_CHAR_TYPE (value_end, IS_SUBTYPE_CHAR);
	      value = g_strndup (value_start, value_end - value_start);
	    }
	  key = g_strndup (at, equals - at);
	  at = value_end;
	  if (dsk_ascii_strcasecmp (key, "charset") == 0)
	    {
	      g_free (piece->charset);
	      piece->charset = value;
	      g_free (key);
	      continue;
	    }
	  if (!fields)
	    fields = g_ptr_array_new ();
	  g_ptr_array_add (fields, key);
	  g_ptr_array_add (fields, value);
	}
      if (fields != NULL)
	{
	  g_ptr_array_add (fields, NULL);
	  piece->other_fields = (char**) g_ptr_array_free (fields, FALSE);
	}
      else
	{
	  piece->other_fields = NULL;
	}
    }
  else if (dsk_ascii_strncasecmp (line, "content-id:", 11) == 0
       && piece->id == NULL)
    {
      const char *start = strchr(line, ':') + 1;
      GSK_SKIP_WHITESPACE (start);
      piece->id = g_strchomp (g_strdup (start));
    }
  else if (dsk_ascii_strncasecmp (line, "content-location:", 17) == 0
       && piece->location == NULL)
    {
      const char *start = strchr(line, ':') + 1;
      GSK_SKIP_WHITESPACE (start);
      piece->location = g_strchomp (g_strdup (start));
    }
  else if (dsk_ascii_strncasecmp (line, "content-transfer-encoding:", 26) == 0)
    {
      const char *start = strchr(line, ':') + 1;
      GSK_SKIP_WHITESPACE (start);
      piece->transfer_encoding = g_strchomp (g_strdup (start));
    }
  else if (dsk_ascii_strncasecmp (line, "content-description:", 20) == 0)
    {
      const char *start = strchr(line, ':') + 1;
      GSK_SKIP_WHITESPACE (start);
      piece->description = g_strchomp (g_strdup (start));
    }
  else if (dsk_ascii_strncasecmp (line, "content-disposition:", 20) == 0)
    {
      const char *start = strchr(line, ':') + 1;
      GSK_SKIP_WHITESPACE (start);
      piece->disposition = g_strchomp (g_strdup (start));
    }
  else
    {
      g_message ("WARNING: could not part multipart_decoder message line: '%s'", line);
      return FALSE;
    }
  return TRUE;
}

static dsk_boolean
done_with_header (DskMimeMultipartDecoder *decoder,
                  DskError               **error)
{
  ...
}

static dsk_boolean
done_with_content_body (DskMimeMultipartDecoder *decoder,
                        DskError               **error)
{
  ...
}


/* Process data from the incoming buffer into decoded buffer,
   scanning for lines starting with "--boundary",
   where "boundary" is replaced with multipart_decoder->boundary.
   
   If we reach the end of this piece, we must enqueue it.
   After that, we may be ready to start a new piece (and we try to do so),
   or we may done (depending on whether the boundary-mark
   ends with an extra "--").
 */
gboolean
dsk_mime_multipart_decoder_feed  (DskMimeMultipartDecoder *decoder,
                                  size_t                   length,
                                  const uint8_t           *data,
                                  size_t                  *n_parts_ready_out,
                                  DskError                *error)
{
  char tmp_buf[256];
  dsk_buffer_append (&decoder->incoming, length, data);
  switch (decoder->state)
    {
    state__READING_HEADER:
    case STATE_READING_HEADER:
      {
        int nl = dsk_buffer_index_of (&decoder->incoming, '\n');
        if (nl < 0)
          goto return_true;
        char *line_buf = (nl + 1 > sizeof (tmp_buf)) ? dsk_malloc (nl+1) : tmp_buf;
        dsk_buffer_read (&decoder->incoming, nl + 1, line_buf);

        /* cut-off newline (and possible "CR" character) */
        if (nl > 0 && line_buf[nl-1] == '\r')
          nl--;
        line_buf[nl] = '\0';    /* chomp */

        /* If the line is empty (or it be corrupt and start with a NUL??? (FIXME)),
           it terminates the header */
        if (line_buf[0] == '\0')
          {
            dsk_boolean rv = done_with_header (decoder, error);
            if (line_buf != tmp_buf)
              dsk_free (line_buf);
            if (!rv)
              return DSK_FALSE;
            decoder->state = STATE_CONTENT_LINE_START;
            goto state__CONTENT_LINE_START;
          }
        else
          {
            dsk_boolean rv = process_header_line (decoder, line_buf, error);
            if (line_buf != tmp_buf)
              dsk_free (line_buf);
            if (!rv)
              return DSK_FALSE;
            goto state__READING_HEADER;
          }
      }
      break;
    case STATE_CONTENT_MIDLINE:
      {
        int nl = dsk_buffer_index_of (&decoder->incoming, '\n');
        if (!dsk_octet_filter_process_buffer (decoder->octet_filter,
                                              &decoder->cur_buffer,
                                              nl < 0 ? decoder->incoming.size : (nl + 1),
                                              &decoder->incoming,
                                              DSK_TRUE,  /* discard from incoming */
                                              error))
          {
            dsk_add_error_prefix (error, "in mime-multipart decoding");
            return DSK_FALSE;
          }
        if (nl < 0)
          {
            /* fed all data into decoder */
            goto return_true;
          }
        decoder->state = STATE_CONTENT_LINE_START;
        goto state__CONTENT_LINE_START;
      }

    state__CONTENT_LINE_START:
    case STATE_CONTENT_LINE_START:
      /* Is this a boundary/terminal mark? */
      if (decoder->incoming.size >= decoder->boundary_str_len + 5)
        {
          int got = dsk_buffer_peek (&decoder->incoming,
                                     2 + decoder->boundary_str_len + 2 + 2,
                                     decoder->boundary_buf);
          int nl_index;
          if (decoder->boundary_buf[0] == '-'
           && decoder->boundary_buf[1] == '-'
           && memcmp (decoder->boundary_buf + 2, decoder->boundary_str,
                      decoder->boundary_str_len) == 0)
            {
              int rem = got - decoder->boundary_str_len - 2;
              const char *at = decoder->boundary_buf + decoder->boundary_str_len + 2;
              const char *nl = memchr (at, '\n', rem);
              if (nl == NULL)
                goto return_true;
              unsigned discard = nl + 1 - decoder->boundary_buf;
              if (at[0] == '-' && at[1] == '-')
                {
                  dsk_buffer_discard (&decoder->incoming, discard);

                  if (!done_with_content_body (decoder, error))
                    return DSK_FALSE;

                  decoder->state = STATE_ENDED;

                  /* TODO: complain about garbage after terminator? */

                  goto return_true;
                }
              else if (dsk_ascii_isspace (*at))
                {
                  /* TODO: more precise check for spaces up to 'nl'? */
                  dsk_buffer_discard (&decoder->incoming, discard);

                  if (!handle_content_complete (decoder, error))
                    return DSK_FALSE;

                  decoder->state = STATE_READING_HEADER;
                  goto state__READING_HEADER;
                }
              else
                {
                  /* treat as non-boundary */
                  nl_index = dsk_buffer_index_of (&decoder->incoming, '\n');
                }
            }
        }
      else
        {
          nl_index = dsk_buffer_index_of (&decoder->incoming, '\n');
          if (nl_index < 0)
            /* wait for more data */
            goto return_true;
        }

      /* handle non-boundary text */
      if (!dsk_octet_filter_process_buffer (decoder->octet_filter,
                                            &decoder->cur_buffer,
                                            nl_index < 0 ? decoder->incoming.size : (nl_index + 1),
                                            &decoder->incoming,
                                            DSK_TRUE,       /* discard from incoming */
                                            error))
        {
          dsk_add_error_prefix (error, "in mime-multipart decoding");
          return DSK_FALSE;
        }
      if (nl_index < 0)
        {
          decoder->state = STATE_CONTENT_MIDLINE;
          goto return_true;
        }
      decoder->state = STATE_CONTENT_LINE_START;
      goto state__CONTENT_LINE_START;
    }

return_true:
  if (n_parts_ready_out)
    *n_parts_ready_out = decoder->n_cgi_vars;
  return DSK_TRUE;
}


static void
dsk_mime_multipart_decoder_init (DskMimeMultipartDecoder *decoder)
{
  decoder->header_pad = dsk_malloc (DSK_MIME_MULTIPART_DECODER_INIT_HEADER_SPACE);
  dsk_mem_pool_init_buf (&decoder->header_storage,
                         DSK_MIME_MULTIPART_DECODER_INIT_HEADER_SPACE,
                         decoder->header_pad);
  decoder->pieces = decoder->pieces_init;
  decoder->pieces_alloced = DSK_MIME_MULTIPART_DECODER_N_INIT_PIECES;
}

DSK_OBJECT_CLASS_DEFINE_CACHE_DATA(DskMimeMultipartDecoder);
static DskMimeMultipartDecoderClass dsk_mime_multipart_decoder_class =
{
  DSK_OBJECT_CLASS_DEFINE (DskMimeMultipartDecoder,
                           &dsk_object_class,
                           dsk_mime_multipart_decoder_init,
                           dsk_mime_multipart_decoder_finalize)
};

DskMimeMultipartDecoder *
dsk_mime_multipart_decoder_new (char **kv_pairs, DskError **error)
{
  DskMimeMultipartDecoder *decoder;
  decoder = dsk_object_new (&dsk_mime_multipart_decoder_class);
  for (i = 0; kv_pairs[2*i] != NULL; i++)
    {
      const char *key = kv_pairs[2*i+0];
      const char *value = kv_pairs[2*i+1];
      if (dsk_ascii_strcasecmp (key, "start") == 0)
	{
	  dsk_free (rv->start);
	  rv->start = dsk_strdup (value);
	}
      else if (g_ascii_strcasecmp (key, "start-info") == 0)
	{
	  dsk_free (rv->start_info);
	  rv->start_info = dsk_strdup (value);
	}
      else if (g_ascii_strcasecmp (key, "boundary") == 0)
	{
	  dsk_free (rv->boundary_str);
	  rv->boundary_str = dsk_strdup (value);
	  rv->boundary_str_len = strlen (rv->boundary_str);
	}
      else
	{
	  g_message ("WARNING: mime-multipart_decoder: ignoring Key %s", key);
	}
    }
  return decoder;
}
