#include "dsk.h"

typedef struct _DskHttpClientClass DskHttpClientClass;
typedef struct _Connection Connection;
typedef struct _DskHttpClientTransfer Transfer;
typedef struct _HostInfo HostInfo;

struct _DskHttpClientClass
{
  DskObjectClass base_class;
};

struct _Connection
{
  DskHttpClientStream *stream;
  HostInfo *host;
  unsigned n_pipelined;
  DskDispatchTimer *kill_timer;
  dsk_boolean closed;

  /* If closed:
        - 'left' and 'right' assume the rule of prev and next
          in the 'unassigned_requests' list.
     Otherwise:
        - 'left', 'right', 'parent' and 'is_red' are used
          for the host's tree of connections by n-pending. */
  Connection *left, *right, *parent;
  dsk_boolean is_red;
};

/* How to handle errors during content-reading */
typedef enum
{
  /* Provides stream immediately */
  REQUEST_MODE_NORMAL,

  /* Gives the stream an error and may (if a new attempt connects)
     give the user a new stream */
  REQUEST_MODE_RESTARTING,

  /* Buffers all content to disk or memory and
     so handle_stream is only invoked once all the data
     is downloaded correctly. */
  REQUEST_MODE_SAFE,

  /* For HEAD requests, and requests that don't trap handle_stream. */
  REQUEST_MODE_IGNORE_CONTENT
} RequestMode;

struct _DskHttpClientTransfer
{
  HostInfo *host_info;
  Connection *connection;       /* if assigned */
  DskHttpClientStreamTransfer *transfer;  /* iff connection!=NULL */
  RequestMode request_mode;

  /* Either the prev/next withing the connection,
     or within the unassigned_requests list. */
  Transfer *prev, *next;
};

struct _HostInfo
{
  /* host/port, or unix-domain socket iff port==0 */
  char *name;
  unsigned port;

  unsigned n_connections;        /* number of connections for this host */
  unsigned max_connections;      /* max_connections for this host */
  unsigned max_pipelined;

  /* set to TRUE if the HostInfo was configured
     specially for this host.  If FALSE, then
     this HostInfo can be deleted if no connections
     remain alive. */
  dsk_boolean is_configured_host;

  /* The comparator is first on n_pending, and second
     on the pointer, so GET_MIN() */
  Connection *connection_tree;

  /* List of connections that have been given a Connection:close directive
     so that no more requests should be pipelined. */
  Connection *close_connections;

  /* List of requests than have not been assigned a connection yet. */
  unsigned n_unassigned_requests;
  Transfer *unassigned_requests;
  unsigned max_unassigned_requests;

  /* host tree, sorted by key (name/port) */
  HostInfo *parent, *left, *right;
  dsk_boolean is_red;
};

struct _DskHttpClient
{
  DskObject base_instance;

  /* a tree of hosts */
  HostInfo *hosts;

  unsigned max_connections;
  unsigned max_connections_per_host;
  unsigned max_unassigned_requests;
};

DskHttpClient *dsk_http_client_new (DskHttpClientOptions *options)
{
  DskHttpClient *rv = dsk_object_new (&dsk_http_client_class);
  rv->max_connections = options->max_connections;
  rv->max_connections_per_host = options->max_connections_per_host;
  rv->max_unassigned_requests = options->max_unassigned_requests;
  return rv;
}

static HostInfo *
force_host_info (DskHttpClientRequestOptions *options,
                 DskError                   **error)
{
  if (options->url != NULL
   || options->host != NULL)
    {
      const char *host_start;
      unsigned host_len;
      if (options->host)
        {
          host_len = strlen (options->host);
          host_start = options->host;
        }
      else
        {
          /* pluck URL's host */
          DskUrlScanned scanned;
          if (!dsk_url_scan (options->url, &scanned, error))
            return NULL;
          if (scanned.host_start == NULL)
            {
              dsk_set_error (error, "URL does not contain a host");
              return DSK_FALSE;
            }
          host_start = scanned.host_start;
          host_len = scanned.host_end - scanned.host_start;
        }
      port = options->port;
      if (port == 0)
        port = DSK_HTTP_PORT;
#define COMPARE(unused, hi, rv)                               \
  if (port < hi->port)                                        \
    rv = -1;                                                  \
  else if (port > hi->port)                                   \
    rv = 1;                                                   \
  else                                                        \
    {                                                         \
      rv = dsk_ascii_strncmp (host_start, hi->host, host_len);\
      if (rv == 0)                                            \
        {                                                     \
          if (hi->host[host_len])                             \
            rv = -1;                                          \
        }                                                     \
    }
      GSK_RBTREE_LOOKUP_COMPARATOR(GET_HOST_TREE (client), unused, COMPARE, host_info);
#undef COMPARE
      if (host_info == NULL)
        {
          host_info = dsk_malloc (sizeof (HostInfo));
          host_info->name = dsk_strndup (host_len, host_start);
          dsk_ascii_strdown (host_info->name);
          host_info->port = port;
          goto new_host_info;
        }
    }
  else if (options->local_socket_path != NULL)
    {
#define COMPARE(unused, hi, rv)                                 \
      if (hi->port)                                             \
        return -1;                                              \
      else                                                      \
        rv = strcmp (options->local_socket_path, hi->name);
      GSK_RBTREE_LOOKUP_COMPARATOR(GET_HOST_TREE (client), unused, COMPARE, host_info);
#undef COMPARE
      if (host_info == NULL)
        {
          host_info = dsk_malloc (sizeof (HostInfo));
          host_info->name = dsk_strdup (options->local_socket_path);
          host_info->port = 0;
          goto new_host_info;
        }
    }
  else
    {
      dsk_assert_not_reached ();
    }
  return host_info;

new_host_info:
  {
    HostInfo *conflict;
    GSK_RBTREE_INSERT (GET_HOST_TREE (client), host_info, conflict);
    dsk_assert (conflict == NULL);
  }

  host_info->n_connections = 0;
  host_info->max_connections = client->max_connections_per_host;
  host_info->max_pipelined = client->max_pipelined;
  host_info->is_configured_host = DSK_FALSE;
  host_info->connection_tree = NULL;
  host_info->close_connections = NULL;
  host_info->unassigned_requests = NULL;
  host_info->n_unassigned_requests = 0;
  host_info->max_unassigned_requests = client->max_unassigned_requests;
  return host_info;
}

static void
transfer_fatal_fail (Transfer *xfer,
                     const char *msg)
{
  ...
}

static void
client_stream__handle_response (DskHttpClientStreamTransfer *stream_xfer)
{
  Transfer *request = stream_xfer->user_data;
  DskHttpResponse *response = stream_xfer->response;
  if (request->funcs->handle_response != NULL)
    {
      request->invoking_handler = 1;
      request->funcs->handle_response (request);
      request->invoking_handler = 0;
    }
  switch (response->status)
    {
      /* 1xx headers */
    case DSK_HTTP_STATUS_CONTINUE:
      /* Continue should be handled by the underlying stream */
      dsk_assert_not_reached ();

    case DSK_HTTP_STATUS_SWITCHING_PROTOCOLS:
      /* This should only occur as a response to an Upgrade
         header in the request.  If it occurs, the content-body
         is an Upgrade header identifying the new protocol to use.

         We do not directly support the Upgrade header
         and we treat 101 responses as failures. */
      transfer_fatal_fail (request, "Switch Protocols response not handled");
      return;

      /* 2xx headers */
    case DSK_HTTP_STATUS_OK:
    /* "201 Created": rfc 2616 section 10.2.2.
       Technically, the body of this message is informative (URLs of resource
       created).  Fortunately, the primary URL is in the Location field.
       So the caller will probably use that instead. */
    case DSK_HTTP_STATUS_CREATED:

    /* "202 Accepted": rfc 2616 section 10.2.3.
       The body should contain information about the status of the message.
       Probably never used. */
    case DSK_HTTP_STATUS_ACCEPTED:

    case DSK_HTTP_STATUS_NONAUTHORITATIVE_INFO:

    case DSK_HTTP_STATUS_NO_CONTENT:

    case DSK_HTTP_STATUS_RESET_CONTENT:

    case DSK_HTTP_STATUS_PARTIAL_CONTENT:
      if (request->request_mode == REQUEST_MODE_SAFE)
        {
          /* If we know the content-length is large,
             make a temp file, otherwise try to use an in-memory buffer. */
          request->content_stream = dsk_object_ref (stream_xfer->content);
          if (response->content_length >= transfer->safe_max_disk)
            {
              transfer_fatal_fail (request, "safe transfer not possible: data too large");
            }
          else if (response->content_length >= transfer->safe_max_memory_size)
            {
              request->safe_content_fd = dsk_fd_make_tmp (&error);
              if (request->safe_content_fd < 0)
                {
                  transfer_fatal_fail (request, error->message);
                  dsk_error_unref (error);
                  return;
                }
              dsk_hook_trap (&request->content_stream->base_instance.readable_hook,
                             handle_safe_content_to_disk,
                             request,
                             NULL);
            }
          else if (response->content_length >= 0)
            {
              request->safe_content_slab = dsk_malloc (response->content_length);
              dsk_hook_trap (&request->content_stream->base_instance.readable_hook,
                             handle_safe_content_to_memory,
                             request,
                             NULL);
            }
          else
            {
              dsk_hook_trap (&request->content_stream->base_instance.readable_hook,
                             handle_safe_content_to_buffer,
                             request,
                             NULL);
            }
        }
      else if (request->request_mode == REQUEST_MODE_IGNORE_CONTENT)
        {
          dsk_hook_trap (&request->content_stream->base_instance.readable_hook,
                         handle_content_readable_discard, NULL, NULL);
        }
      else
        {
          /* stream to user */
          ...
        }
      return;


      /* 3xx headers */
    case DSK_HTTP_STATUS_MULTIPLE_CHOICES:
      /* not supported??? */
      ...
    case DSK_HTTP_STATUS_MOVED_PERMANENTLY:
      ...
    case DSK_HTTP_STATUS_FOUND:
      ...
    case DSK_HTTP_STATUS_SEE_OTHER:
      ...
    case DSK_HTTP_STATUS_NOT_MODIFIED:
      ...
    case DSK_HTTP_STATUS_USE_PROXY:
      ...
    case DSK_HTTP_STATUS_TEMPORARY_REDIRECT:
      /* 4xx headers */
    case DSK_HTTP_STATUS_BAD_REQUEST:
      ...
    case DSK_HTTP_STATUS_UNAUTHORIZED:
      ...
    case DSK_HTTP_STATUS_PAYMENT_REQUIRED:
      ...
    case DSK_HTTP_STATUS_FORBIDDEN:
      ...
    case DSK_HTTP_STATUS_NOT_FOUND:
      ...
    case DSK_HTTP_STATUS_METHOD_NOT_ALLOWED:
      ...
    case DSK_HTTP_STATUS_NOT_ACCEPTABLE:
      ...
    case DSK_HTTP_STATUS_PROXY_AUTH_REQUIRED:
      ...
    case DSK_HTTP_STATUS_REQUEST_TIMEOUT:
      ...
    case DSK_HTTP_STATUS_CONFLICT:
      ...
    case DSK_HTTP_STATUS_GONE:
      ...
    case DSK_HTTP_STATUS_LENGTH_REQUIRED:
      ...
    case DSK_HTTP_STATUS_PRECONDITION_FAILED:
      ...
    case DSK_HTTP_STATUS_ENTITY_TOO_LARGE:
      ...
    case DSK_HTTP_STATUS_URI_TOO_LARGE:
      ...
    case DSK_HTTP_STATUS_UNSUPPORTED_MEDIA:
      ...
    case DSK_HTTP_STATUS_BAD_RANGE:
      ...
    case DSK_HTTP_STATUS_EXPECTATION_FAILED:
      ...
      /* 5xx headers */
    case DSK_HTTP_STATUS_INTERNAL_SERVER_ERROR:
      ...
    case DSK_HTTP_STATUS_NOT_IMPLEMENTED:
      ...
    case DSK_HTTP_STATUS_BAD_GATEWAY:
      ...
    case DSK_HTTP_STATUS_SERVICE_UNAVAILABLE:
      ...
    case DSK_HTTP_STATUS_GATEWAY_TIMEOUT:
      ...
    case DSK_HTTP_STATUS_UNSUPPORTED_VERSION:
      ...

    default:
      if (response->status < 100 || response->status > 599)
        {
          ...
        }
      else
        {
          ...
        }
    }
}

static void
client_stream__handle_content_complete (DskHttpClientStreamTransfer *xfer)
{
  Transfer *request = xfer->user_data;
  ...
}

static void
client_stream__handle_error (DskHttpClientStreamTransfer *xfer)
{
  Transfer *request = xfer->user_data;
  if (request->n_attempts_remaining == 0)
    {
      /* fail request */
      ...
    }
  else
    {
      /* destroy connection?
         on "destroy" all pending requests should be redistributed */
      ...
    }
}

static void
client_stream__handle_destroy (DskHttpClientStreamTransfer *xfer)
{
  Transfer *request = xfer->user_data;
  ...
}


static DskHttpClientStreamFuncs client_stream_request_funcs =
{
  client_stream__handle_response,
  client_stream__handle_content_complete,
  client_stream__handle_error,
  client_stream__handle_destroy
};

/* Assumes that 'in', 'out' and 'in->request_options' have been initialized. */
static dsk_boolean
init_request_options (DskHttpClientRequestOptions *in,
                      DskHttpClientStreamRequestOptions *out,
                      Transfer *request,
                      DskMemPool *mem_pool,
                      DskError  **error)
{
  DskHttpRequestOptions *header_options = in->request_options;
  ... = in->url
  ... = in->path
  ... = in->query
  ... = in->n_extra_get_cgi_vars
  ... = in->extra_get_cgi_vars
  ... = in->always_pipeline
  ... = in->never_pipeline
  ... = in->n_post_cgi_vars
  ... = in->post_cgi_vars
  ... = in->request_body
  ... = in->safe_mode
  ... = in->may_restart_stream
  ... = in->n_unparsed_headers
  ... = in->unparsed_headers
  ... = in->unparsed_misc_headers
  ... = in->keepalive_millis
  ... = in->connection_close
  ... = in->allow_gzip
  ... = in->has_postdata_md5sum
  ... = in->postdata_md5sum_binary
  ... = in->postdata_md5sum_hex
  ... = in->gzip_post_data
  ... = in->check_md5sum
  ... = in->max_retries
  ... = in->retry_sleep_millis
  ... = in->max_redirects
  ... = in->n_cookies
  ... = in->cookies

  header_options->... = ...;
  out->post_data = ???.
  out->post_data_length = ???.
  out->post_data_slab = ???.
  out->gzip_compression_level = ???.
  out->gzip_compress_post_data = ???.
  out->post_data_is_gzipped = ???.
  out->uncompress_content = ???.
  out->n_cookies = ???
  out->cookies = ???
  out->funcs = &client_stream_request_funcs ???
  out->user_data = request;
}

dsk_boolean
dsk_http_client_request  (DskHttpClient               *client,
                          DskHttpClientRequestOptions *options,
                          DskOctetSource              *post_data,
                          DskError                   **error)
{
  /* Check for as many errors as possible. */
  if (options->url == NULL
   && options->local_socket_path == NULL
   && options->host == NULL)
    {
      dsk_set_error (error, "no host given");
      return DSK_FALSE;
    }

  /* Find 'host_info' */
  HostInfo *host_info = force_host_info (options, error);
  if (host_info == NULL)
    return DSK_FALSE;

  /* Create request object */
  request = dsk_malloc0 (sizeof (Transfer));
  request->host_info = host_info;

  /* Determine request->mode */
  ...

  /* Do we have an existing connection that we can
     use for this request? */
  Connection *conn;
  GSK_RBTREE_FIRST (GET_REUSABLE_CONNECTION_TREE (host_info), conn);

  /* If a connection has a pending request
     and the number of connections is below the "min_connections"
     for this host, then create a new connection instead of reusing the
     connection. */
  if (conn != NULL
   && conn->n_pending > 0
   && host_info->n_connections < host_info->min_connections_before_pipelining
   && client->n_connections < client->max_connections)
    {
      conn = NULL;
    }

  if (conn != NULL && conn->n_pending < host_info->max_pipelined)
    {
      /* Connection available for reuse */
      Transfer *conflict;
      GSK_RBTREE_REMOVE (GET_CONNECTION_TREE (host_info), conn);
      ++(conn->n_pending);
      GSK_RBTREE_INSERT (GET_CONNECTION_TREE (host_info), conn, conflict);
      dsk_assert (conflict == NULL);
    }
  else if (host_info->n_connections < host_info->max_connections
        && client->n_connections < client->max_connections)
    {
      /* Create a new connection */
      DskClientStreamOptions cs_options = DSK_CLIENT_STREAM_OPTIONS_DEFAULT;
      DskHttpClientStreamOptions hcs_options = DSK_HTTP_CLIENT_STREAM_OPTIONS_DEFAULT;

      /* Setup client-stream options */
      /* TODO: obey host, port, local_socket_path, has_ip_address */

      if (!dsk_client_stream_new (&cs_options, NULL, &sink, &source, error))
        {
          ...
        }

      conn = dsk_malloc0 (sizeof (Connection));;

      /* Setup http-client-stream options */
      ...

      conn->stream = dsk_http_client_stream_new (sink, source, &hcs_options);
      dsk_object_unref (sink);
      dsk_object_unref (source);
      dsk_assert (conn->stream != NULL);
      conn->n_pending = 1;
      GSK_RBTREE_INSERT (GET_CONNECTION_TREE (host_info), conn, conflict);
      dsk_assert (conflict == NULL);
    }
  else (options->block_if_busy
     && host_info->n_unassigned_requests < host_info->max_unassigned_requests)
    {
      /* Enqueue our request in the wait-queue */
      GSK_QUEUE_ENQUEUE (GET_UNASSIGNED_REQUEST_QUEUE (host_info), request);
      host_info->n_unassigned_requests += 1;
      return DSK_TRUE;
    }
  else
    {
      dsk_set_error (error, "too many connections in HTTP client");
      return DSK_FALSE;
    }

  /* --- Set up request options --- */
  DskHttpClientStreamRequestOptions request_options
    = DSK_HTTP_CLIENT_STREAM_REQUEST_OPTIONS_DEFAULT;
  DskHttpRequestOptions header_options = DSK_HTTP_REQUEST_OPTIONS_DEFAULT;
  char mem_pool_buf[1024];
  request_options.request_options = &header_options;
  DskMemPool mem_pool;
  dsk_mem_pool_init_buf (&mem_pool, sizeof (mem_pool_buf), mem_pool_buf);
  if (!init_request_options (options, &request_options, request, &mem_pool, error))
    return DSK_FALSE;


  request->connection = conn;
  request->transfer = dsk_http_client_stream_request (conn->client_stream,
                                                      &request_options,
                                                      error);
  if (request->transfer == NULL)
    {
      GSK_RBTREE_REMOVE (GET_CONNECTION_TREE (host_info), request);
      dsk_free (request);
      return DSK_FALSE;
    }
  return DSK_TRUE;
}
