/* TODO: this file should be sorted according to the order
 * in RFC 2616.
 */


typedef struct _DskHttpRequestClass DskHttpRequestClass;
typedef struct _DskHttpRequest DskHttpRequest;
typedef struct _DskHttpResponseClass DskHttpResponseClass;
typedef struct _DskHttpResponse DskHttpResponse;

/* The status is the numerically status code, so
   often (for example, 200 or 404) it's almost
   easier just to use the code... but i recommend the
   enum even in those cases. */
typedef enum
{
  DSK_HTTP_STATUS_CONTINUE                      = 100,
  DSK_HTTP_STATUS_SWITCHING_PROTOCOLS           = 101,
  DSK_HTTP_STATUS_OK                            = 200,
  DSK_HTTP_STATUS_CREATED                       = 201,
  DSK_HTTP_STATUS_ACCEPTED                      = 202,
  DSK_HTTP_STATUS_NONAUTHORITATIVE_INFO         = 203,
  DSK_HTTP_STATUS_NO_CONTENT                    = 204,
  DSK_HTTP_STATUS_RESET_CONTENT                 = 205,
  DSK_HTTP_STATUS_PARTIAL_CONTENT               = 206,
  DSK_HTTP_STATUS_MULTIPLE_CHOICES              = 300,
  DSK_HTTP_STATUS_MOVED_PERMANENTLY             = 301,
  DSK_HTTP_STATUS_FOUND                         = 302,
  DSK_HTTP_STATUS_SEE_OTHER                     = 303,
  DSK_HTTP_STATUS_NOT_MODIFIED                  = 304,
  DSK_HTTP_STATUS_USE_PROXY                     = 305,
  DSK_HTTP_STATUS_TEMPORARY_REDIRECT            = 306,
  DSK_HTTP_STATUS_BAD_REQUEST                   = 400,
  DSK_HTTP_STATUS_UNAUTHORIZED                  = 401,
  DSK_HTTP_STATUS_PAYMENT_REQUIRED              = 402,
  DSK_HTTP_STATUS_FORBIDDEN                     = 403,
  DSK_HTTP_STATUS_NOT_FOUND                     = 404,
  DSK_HTTP_STATUS_METHOD_NOT_ALLOWED            = 405,
  DSK_HTTP_STATUS_NOT_ACCEPTABLE                = 406,
  DSK_HTTP_STATUS_PROXY_AUTH_REQUIRED           = 407,
  DSK_HTTP_STATUS_REQUEST_TIMEOUT               = 408,
  DSK_HTTP_STATUS_CONFLICT                      = 409,
  DSK_HTTP_STATUS_GONE                          = 410,
  DSK_HTTP_STATUS_LENGTH_REQUIRED               = 411,
  DSK_HTTP_STATUS_PRECONDITION_FAILED           = 412,
  DSK_HTTP_STATUS_ENTITY_TOO_LARGE              = 413,
  DSK_HTTP_STATUS_URI_TOO_LARGE                 = 414,
  DSK_HTTP_STATUS_UNSUPPORTED_MEDIA             = 415,
  DSK_HTTP_STATUS_BAD_RANGE                     = 416,
  DSK_HTTP_STATUS_EXPECTATION_FAILED            = 417,
  DSK_HTTP_STATUS_INTERNAL_SERVER_ERROR         = 500,
  DSK_HTTP_STATUS_NOT_IMPLEMENTED               = 501,
  DSK_HTTP_STATUS_BAD_GATEWAY                   = 502,
  DSK_HTTP_STATUS_SERVICE_UNAVAILABLE           = 503,
  DSK_HTTP_STATUS_GATEWAY_TIMEOUT               = 504,
  DSK_HTTP_STATUS_UNSUPPORTED_VERSION           = 505
} DskHttpStatus;

/*
 * The Verb is the first text transmitted
 * (from the user-agent to the server).
 */
typedef enum
{
  DSK_HTTP_VERB_GET,
  DSK_HTTP_VERB_POST,
  DSK_HTTP_VERB_PUT,
  DSK_HTTP_VERB_HEAD,
  DSK_HTTP_VERB_OPTIONS,
  DSK_HTTP_VERB_DELETE,
  DSK_HTTP_VERB_TRACE,
  DSK_HTTP_VERB_CONNECT
} DskHttpVerb;

typedef enum {
  DSK_HTTP_CONTENT_ENCODING_IDENTITY,
  DSK_HTTP_CONTENT_ENCODING_GZIP,
  DSK_HTTP_CONTENT_ENCODING_COMPRESS
} DskHttpContentEncoding;

/*
 * The Transfer-Encoding field of HTTP/1.1.
 *
 * In particular, HTTP/1.1 compliant clients and proxies
 * MUST support `chunked'.  The compressed Transfer-Encodings
 * are rarely (if ever) used.  (As opposed to the compressed Content-Encoding,
 * which sees quite a bit of use.)
 *
 * Note that:
 *   - we interpret this field, even for HTTP/1.0 clients.
 */
typedef enum {
  DSK_HTTP_TRANSFER_ENCODING_NONE    = 0,
  DSK_HTTP_TRANSFER_ENCODING_CHUNKED = 1,
  DSK_HTTP_TRANSFER_ENCODING_UNRECOGNIZED = 0x100
} DskHttpTransferEncoding;

/*
 * The Connection: header enables or disables http-keepalive.
 *
 * For HTTP/1.0, NONE should be treated like CLOSE.
 * For HTTP/1.1, NONE should be treated like KEEPALIVE.
 *
 * Use gsk_http_header_get_connection () to do this automatically -- it
 * always returns DSK_HTTP_CONNECTION_CLOSE or DSK_HTTP_CONNECTION_KEEPALIVE.
 *
 * See RFC 2616, Section 14.10.
 */
typedef enum
{
  DSK_HTTP_CONNECTION_NONE,
  DSK_HTTP_CONNECTION_CLOSE,
  DSK_HTTP_CONNECTION_KEEPALIVE
} DskHttpConnection;

/*
 * The Cache-Control response directive.
 * See RFC 2616, Section 14.9 (cache-response-directive)
 */
typedef struct _DskHttpResponseCacheDirective DskHttpResponseCacheDirective;
struct _DskHttpResponseCacheDirective
{
  /*< public (read/write) >*/
  /* the http is `public' and `private'; is_ is added
   * for C++ users.
   */
  uint8_t   is_public : 1;
  uint8_t   is_private : 1;

  uint8_t   no_cache : 1;
  uint8_t   no_store : 1;
  uint8_t   no_transform : 1;

  uint8_t   must_revalidate : 1;
  uint8_t   proxy_revalidate : 1;
  unsigned   max_age;
  unsigned   s_max_age;

  /*< public (read-only) >*/
  char   *private_name;
  char   *no_cache_name;

  /* TODO: what about cache-extensions? */
};

/*
 * The Cache-Control request directive.
 * See RFC 2616, Section 14.9 (cache-request-directive)
 */
typedef struct _DskHttpRequestCacheDirective DskHttpRequestCacheDirective;
struct _DskHttpRequestCacheDirective
{
  uint8_t no_cache : 1;
  uint8_t no_store : 1;
  uint8_t no_transform : 1;
  uint8_t only_if_cached : 1;

  uint8_t max_age;
  uint8_t min_fresh;

 /* 
  * We need to be able to indicate that max_stale is set without the 
  * optional argument. So:
  *		      0 not set
  *		     -1 set no arg
  *		     >0 set with arg.	  
  */
  int  max_stale;

  /* TODO: what about cache-extensions? */
};


/*
 * The Accept: request-header.
 *
 * See RFC 2616, Section 14.1.
 *
 * TODO: support level= accept-extension.
 */
typedef struct _DskHttpMediaOption DskHttpMediaOption;
struct _DskHttpMediaOption
{
  /*< public: read-only >*/
  char                 *type;
  char                 *subtype;
  float                 quality;                /* -1 if not present */
};


/*
 * The Accept-Charset: request-header.
 *
 * See RFC 2616, Section 14.2.
 */
typedef struct _DskHttpCharsetOption DskHttpCharsetOption;
struct _DskHttpCharsetOption
{
  /*< public: read-only >*/
  char                 *charset_name;
  float                 quality;                /* -1 if not present */
};

/*
 * The Accept-Encoding: request-header.
 *
 * See RFC 2616, Section 14.3.
 */
typedef struct _DskHttpContentEncodingOption DskHttpContentEncodingOption;
struct _DskHttpContentEncodingOption
{
  /*< public: read-only >*/
  DskHttpContentEncoding       encoding;
  float                        quality;       /* -1 if not present */
};
/*
 * for the TE: request-header.
 *
 * See RFC 2616, Section 14.39.
 */
typedef struct _DskHttpTransferEncodingOption DskHttpTransferEncodingOption;
struct _DskHttpTransferEncodingOption
{
  /*< public: read-only >*/
  DskHttpTransferEncoding      encoding;
  float                        quality;       /* -1 if not present */
};


/*
 * The Accept-Language: request-header.
 *
 * See RFC 2616, Section 14.4.
 */
typedef struct _DskHttpLanguageOption DskHttpLanguageOption;
struct _DskHttpLanguageOption
{
  /*< public: read-only >*/

  /* these give a language (with optional dialect specifier) */
  char                 *language;
  float                 quality;                /* -1 if not present */
};

typedef enum
{
  GSK_HTTP_AUTH_MODE_UNKNOWN,
  GSK_HTTP_AUTH_MODE_BASIC,
  GSK_HTTP_AUTH_MODE_DIGEST
} DskHttpAuthMode;

/* HTTP Authentication.
   
   These structures give map to the WWW-Authenticate/Authorization headers,
   see RFC 2617.

   The outline is:
     If you get a 401 (Unauthorized) header, the server will
     accompany that with information about how to authenticate,
     in the WWW-Authenticate header.
     
     The user-agent should prompt the user for a username/password.

     Then the client tries again: but this time with an appropriate Authorization.
     If the server is satified, it will presumably give you the content.
 */
typedef struct _DskHttpAuthenticate DskHttpAuthenticate;
struct _DskHttpAuthenticate
{
  DskHttpAuthMode mode;
  char           *auth_scheme_name;
  char           *realm;
  union
  {
    struct {
      char                   *options;
    } unknown;
    /* no members:
    struct {
    } basic;
    */
    struct {
      char                   *domain;
      char                   *nonce;
      char                   *opaque;
      dsk_boolean             is_stale;
      char                   *algorithm;
    } digest;
  } info;
};
typedef struct _DskHttpAuthorization DskHttpAuthorization;
struct _DskHttpAuthorization
{
  DskHttpAuthMode mode;
  char           *auth_scheme_name;
  union
  {
    struct {
      char                   *response;
    } unknown;
    struct {
      char                   *user;
      char                   *password;
    } basic;
    struct {
      char                   *realm;
      char                   *domain;
      char                   *nonce;
      char                   *opaque;
      char                   *algorithm;
      char                   *user;
      char                   *password;
      char                   *response_digest;
      char                   *entity_digest;
    } digest;
  } info;
};

/* an update to an existing authentication,
   to verify that we're talking to the same host. */
typedef struct _DskHttpAuthenticateInfo DskHttpAuthenticateInfo;
struct _DskHttpAuthenticateInfo
{
  char *next_nonce;
  char *response_digest;
  char *cnonce;
  unsigned has_nonce_count;
  uint32_t nonce_count;
};

/* A single `Cookie' or `Set-Cookie' header.
 *
 * See RFC 2109, HTTP State Management Mechanism 
 */
typedef struct _DskHttpCookie DskHttpCookie;
struct _DskHttpCookie
{
  /*< public: read-only >*/
  char                   *key;
  char                   *value;
  char                   *domain;
  char                   *path;
  char                   *expire_date;
  char                   *comment;
  int                     max_age;
  dsk_boolean             secure;               /* default is FALSE */
  unsigned                version;              /* default is 0, unspecified */
};

typedef struct _DskHttpHeaderMisc DskHttpHeaderMisc;
struct _DskHttpHeaderMisc
{
  char *key;            /* lowercased */
  char *value;
};

struct _DskHttpRequestClass
{
  DskObjectClass base_class;
};

struct _DskHttpRequest
{
  DskObject base_object;

  DskHttpConnection             connection_type;

  DskHttpTransferEncoding       transfer_encoding_type;
  DskHttpContentEncoding        content_encoding_type;

  unsigned accept_range_bytes : 1; /* Accept-Ranges */
  unsigned has_date : 1;           /* Date (see date member) */

  /*< public >*/
  DskHttpContentEncoding content_encoding;     /* Content-Encoding */

  char *content_type;             /* Content-Type */
  char *content_subtype;
  char *content_charset;

  /* the 'Date' header, parsed into unix-time, i.e.
     seconds since epoch (if the has_date flag is set) */
  int64_t date;

  /* From the Content-Length header; -1 to disable */
  int64_t content_length;

  /* Key/value searchable header lines.
     Sorted by key, then instance of occurance.
     All keys are lowercased.
     When serialized, the misc headers are written in alphabetical order */
  unsigned n_misc_header;
  DskHttpHeaderMisc *misc_headers;

  /*< public >*/
  /* the command: GET, PUT, POST, HEAD, etc */
  DskHttpVerb                   verb;

  /* Note that HTTP/1.1 servers must accept the entire
   * URL being included in `path'! (maybe including http:// ... */
  char                         *path;

  unsigned                  n_charset_options;
  DskHttpCharsetOption     *charset_options;              /* Accept-CharSet */
  unsigned n_content_encoding_options;
  DskHttpContentEncodingOption*content_encoding_options;     /* Accept-Encoding */
  unsigned n_transfer_encoding_options;
  DskHttpTransferEncodingOption*transfer_encodings_options;  /* TE */
  unsigned                  n_accept_options;
  DskHttpMediaOption      *accept_options;           /* Accept */
  DskHttpAuthorization     *authorization;                /* Authorization */
  unsigned                  n_language_options;
  DskHttpLanguageOption    *languages_options;             /* Accept-Languages */
  char                     *host;                         /* Host */

  dsk_boolean               had_if_match;
  char                    **if_match;             /* If-Match */
  dsk_time_t                if_modified_since;    /* If-Modified-Since */
  char                     *user_agent;           /* User-Agent */

  char                     *referrer;             /* Referer */

  char                     *from;      /* The From: header (sect 14.22) */

  /* List of Cookie: headers. */
  unsigned                  n_cookies;
  DskHttpCookie            *cookies;

  DskHttpAuthorization     *proxy_authorization;

  int                       keep_alive_seconds;   /* -1 if not used */

  /* rarely used: */
  int                       max_forwards;         /* -1 if not used */

  /* Nonstandard User-Agent information.
     Many browsers provide this data to allow
     dynamic content to take advantage of the
     client configuration.  (0 indicated "not supplied").  */
  unsigned                  ua_width, ua_height;
  char                     *ua_color;
  char                     *ua_os;
  char                     *ua_cpu;
  char                     *ua_language;

  DskHttpRequestCacheDirective *cache_control;        /* Cache-Control */
};

struct _DskHttpResponseClass
{
  DskObjectClass base_class;
};
struct _DskHttpResponse
{
  DskObject base_object;

  uint8_t http_major_version;             /* always 1 */
  uint8_t http_minor_version;
  DskHttpConnection connection_type;

  DskHttpTransferEncoding transfer_encoding_type;
  DskHttpContentEncoding content_encoding_type;

  unsigned accept_range_bytes : 1; /* Accept-Ranges */
  unsigned has_date : 1;           /* Date (see date member) */

  /* Content-Encoding */
  DskHttpContentEncoding content_encoding;

  /* Content-Type */
  char *content_type;
  char *content_subtype;
  char *content_charset;

  /* the 'Date' header, parsed into unix-time, i.e.
     seconds since epoch (if the has_date flag is set) */
  int64_t date;

  /* From the Content-Length header; -1 to disable */
  int64_t content_length;

  /* Key/value searchable header lines.
     Sorted by key, then instance of occurance.
     All keys are lowercased.
     When serialized, the misc headers are written in alphabetical order */
  unsigned n_misc_header;
  DskHttpHeaderMisc *misc_headers;
  
  /* initially allowed_verbs == 0;
   * since it is an error not to allow any verbs;
   * otherwise it is a bitwise-OR: (1 << GSK_HTTP_VERB_*)
   */
  unsigned                  allowed_verbs;

  DskHttpResponseCacheDirective *cache_control;        /* Cache-Control */

  unsigned                  has_md5sum : 1;
  unsigned char             md5sum[16];           /* Content-MD5 (14.15) */

  /* List of Set-Cookie: headers. */
  unsigned                  n_set_cookies;
  DskHttpCookie            *set_cookies;

  /* The `Location' to redirect to. */
  char                     *location;

  /* -1 for no expiration */
  dsk_time_t                expires;

  /* The ``Entity-Tag'', cf RFC 2616, Sections 14.24, 14.26, 14.44. */
  char                     *etag;

  DskHttpAuthenticate      *proxy_authenticate;

  /* This is the WWW-Authenticate: header line. */
  DskHttpAuthenticate      *authenticate;

  /* If `retry_after_relative', the retry_after is the number 
   * of seconds to wait before retrying; otherwise,
   * it is a unix-time indicting when to retry.
   *
   * (Corresponding to the `Retry-After' header, cf RFC 2616, 14.37)
   */
  unsigned                  has_retry_after : 1;
  dsk_boolean               retry_after_relative;
  long                      retry_after;

  /* The Last-Modified header.  If != -1, this is the unix-time
   * the message-body-contents were last modified. (RFC 2616, section 14.29)
   */
  long                      last_modified;

  char                     *server;        /* The Server: header */
};
extern DskHttpRequestClass dsk_http_request_class;
extern DskHttpResponseClass dsk_http_response_class;

DskHttpRequest  *dsk_http_request_parse_buffer  (DskBuffer *buffer,
                                                 unsigned   header_len,
                                                 DskError **error);
DskHttpResponse *dsk_http_response_parse_buffer (DskBuffer *buffer,
                                                 unsigned   header_len,
                                                 DskError **error);

