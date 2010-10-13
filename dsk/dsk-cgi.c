#include <string.h>
#include "dsk.h"

/* query_string starts (and includes) the '?'  */
dsk_boolean dsk_cgi_parse_query_string (const char *query_string,
                                        unsigned   *n_cgi_var_out,
                                        DskCgiVar **cgi_var_out,
                                        DskError  **error)
{
  const char *at;
  unsigned n_ampersand = 0;
  unsigned idx;
  if (*query_string == '?')
    query_string++;
  for (at = query_string; *at; at++)
    if (*at == '&')
      n_ampersand++;
  /* TODO: need max_cgi_vars for security????? */
  *cgi_var_out = dsk_malloc (sizeof (DskCgiVar) * (n_ampersand+1));
  idx = 0;
  unsigned n_cgi = 0;
  for (at = query_string; *at; )
    {
      const char *start;
      if (*at == '&')
        {
          at++;
          continue;
        }
      start = at;
      const char *eq;
      eq = NULL;
      unsigned key_len, value_len = 0;
      while (*at && *at != '&')
        {
          if (*at == '=')
            {
              eq = at;

              /* handle value */
              value_len = 0;
              at++;
              while (*at && *at != '&')
                {
                  if (*at == '%')
                    {
                      if (!dsk_ascii_isxdigit (at[1])
                       || !dsk_ascii_isxdigit (at[2]))
                        {
                          unsigned i;
                          for (i = 0; i < n_cgi; i++)
                            dsk_free ((*cgi_var_out)[i].key);
                          dsk_free (*cgi_var_out);
                          dsk_set_error (error, "'%%' in CGI-variable not followed by two hexidecimal digits");
                          return DSK_FALSE;

                        }
                      value_len++;
                      at += 3;
                    }
                  else
                    {
                      value_len++;
                      at++;
                    }
                }
            }
          at++;
        }
      key_len = (eq - start);

      char *kv;
      kv = dsk_malloc (key_len + 1 + value_len + 1);
      memcpy (kv, start, key_len);
      kv[key_len] = 0;
      (*cgi_var_out)[n_cgi].key = kv;

      /* unescape value */
      if (eq)
        {
          char *out = kv + key_len + 1;
          char *value_start = out;
          (*cgi_var_out)[n_cgi].value = out;
          for (at = eq + 1; *at != '&' && *at != '\0'; at++)
            if (*at == '%')
              {
                *out++ = (dsk_ascii_xdigit_value(at[1]) << 4)
                       | dsk_ascii_xdigit_value(at[2]);
                at += 2;
              }
            else if (*at == '+')
              *out++ = ' ';
            else
              *out++ = *at;
          *out = '\0';
          (*cgi_var_out)[n_cgi].value_length = out - value_start;
        }
      else
        {
          (*cgi_var_out)[n_cgi].value = NULL;
          (*cgi_var_out)[n_cgi].value_length = 0;
        }
      (*cgi_var_out)[n_cgi].content_type = NULL;
      (*cgi_var_out)[n_cgi].is_get = DSK_TRUE;

      (*cgi_var_out)[n_cgi].content_type = NULL;
      (*cgi_var_out)[n_cgi].content_location = NULL;
      (*cgi_var_out)[n_cgi].content_description = NULL;
      (*cgi_var_out)[n_cgi].content_disposition = NULL;
      n_cgi++;
    }
  *n_cgi_var_out = n_cgi;
  return DSK_TRUE;
}

dsk_boolean dsk_cgi_parse_post_data (const char *content_main_type,
                                     const char *content_subtype,
                                     char      **content_type_kv_pairs,
                                     unsigned    post_data_length,
                                     const uint8_t *post_data,
                                     unsigned   *n_cgi_var_out,
                                     DskCgiVar **cgi_var_out,
                                     DskError  **error)
{
  if (strcmp (content_main_type, "application") == 0
   && strcmp (content_subtype, "x-www-form-urlencoded") == 0)
    {
      char *pd_str = dsk_malloc (post_data_length + 2);
      memcpy (pd_str + 1, post_data, post_data_length);
      pd_str[0] = '?';
      pd_str[post_data_length+1] = '\0';
      if (!dsk_cgi_parse_query_string (pd_str, n_cgi_var_out, cgi_var_out,
                                       error))
        {
          dsk_free (pd_str);
          return DSK_FALSE;
        }
      dsk_free (pd_str);
      return DSK_TRUE;
    }
  else if (strcmp (content_main_type, "multipart") == 0
        && strcmp (content_subtype, "form-data") == 0)
    {
      DskMimeMultipartDecoder *decoder = dsk_mime_multipart_decoder_new (content_type_kv_pairs, error);
      if (decoder == NULL)
        return DSK_FALSE;
      if (!dsk_mime_multipart_decoder_feed (decoder,
                                            post_data_length, post_data,
                                            NULL, error)
       || !dsk_mime_multipart_decoder_done (decoder, n_cgi_var_out, error))
        {
          dsk_object_unref (decoder);
          return DSK_FALSE;
        }
      *cgi_var_out = dsk_malloc (*n_cgi_var_out * sizeof (DskCgiVar));
      dsk_mime_multipart_decoder_dequeue_all (decoder, *cgi_var_out);
      dsk_object_unref (decoder);
      return DSK_TRUE;
    }
  else
    {
      dsk_set_error (error, 
                     "parsing POST form data: unhandled content-type %s/%s",
                     content_main_type, content_subtype);
      return DSK_FALSE;
    }
}

void        dsk_cgi_var_clear       (DskCgiVar *var)
{
  if (var->key == NULL)
    {
      dsk_assert (!var->is_get);
      dsk_free (var->value);
    }
  else
    dsk_free (var->key);
}
