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
        }
      else
        {
          (*cgi_var_out)[n_cgi].value = NULL;
        }
      (*cgi_var_out)[n_cgi].content_type = NULL;
      (*cgi_var_out)[n_cgi].is_get = DSK_TRUE;
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
  if (strcmp (content_main_type, "
  ...
}

void        dsk_cgi_var_clear       (DskCgiVar *var)
{
  dsk_free (var->key);
}
