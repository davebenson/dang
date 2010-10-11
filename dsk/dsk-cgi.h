
typedef struct
{
  dsk_boolean is_get;           /* if !is_get, then its a POST CGI var */
  char *key;
  char *value;
  char *content_type;           /* for some POST headers */
} DskCgiVar;


/* query_string starts (and includes) the '?'  */
dsk_boolean dsk_cgi_parse_query_string (const char *query_string,
                                        unsigned   *n_cgi_var_out,
                                        DskCgiVar **cgi_var_out,
                                        DskError  **error);


dsk_boolean dsk_cgi_parse_post_data (const char *content_type,
                                     unsigned    post_data_length,
                                     const uint8_t *post_data,
                                     unsigned   *n_cgi_var_out,
                                     DskCgiVar **cgi_var_out,
                                     DskError  **error);

void        dsk_cgi_var_clear       (DskCgiVar *var);
