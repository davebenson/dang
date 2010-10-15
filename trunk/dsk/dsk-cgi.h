
typedef struct
{
  dsk_boolean is_get;           /* if !is_get, then its a POST CGI var */
  char *key;
  unsigned value_length;        /* for POST data, value may contain NUL */
  char *value;

  /* the following are available for some POST headers */
  char *content_type;
  char *content_location;
  char *content_description;
} DskCgiVar;


/* query_string starts (and includes) the '?'  */
dsk_boolean dsk_cgi_parse_query_string (const char *query_string,
                                        unsigned   *n_cgi_var_out,
                                        DskCgiVar **cgi_var_out,
                                        DskError  **error);


/* Handles "application/x-www-form-urlencoded" (RFC XXXX)
   and     "multipart/form-data" (RFC XXXX)
   content-types */
dsk_boolean dsk_cgi_parse_post_data (const char *content_type,
                                     char      **content_type_kv_pairs,
                                     unsigned    post_data_length,
                                     const uint8_t *post_data,
                                     unsigned   *n_cgi_var_out,
                                     DskCgiVar **cgi_var_out,
                                     DskError  **error);

void        dsk_cgi_var_clear       (DskCgiVar *var);
