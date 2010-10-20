#include "dsk.h"

typedef struct _DskHttpClientClass DskHttpClientClass;
typedef struct _Connection Connection;
typedef struct _Request Request;
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

struct _Request
{
  HostInfo *host_info;
  Connection *connection;       /* if assigned */
  DskHttpClientStreamTransfer *transfer;  /* iff connection!=NULL */

  /* Either the prev/next withing the connection,
     or within the unassigned_requests list. */
  Request *prev, *next;
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
  Request *unassigned_requests;
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
          ...
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
      dsk_set_error (error, "no host given");
      return NULL;
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

dsk_boolean
dsk_http_client_request  (DskHttpClient               *client,
                          DskHttpClientRequestOptions *options,
                          DskOctetSource              *post_data,
                          DskError                   **error)
{
  /* Find 'host_info' */
  HostInfo *host_info = force_host_info (options, error);
  if (host_info == NULL)
    return DSK_FALSE;

  /* Do we have an existing connection that we can
     use for this request? */
  Connection *conn;
  GSK_RBTREE_FIRST (GET_REUSABLE_CONNECTION_TREE (host_info), conn);
  if (conn != NULL && conn->n_pending < host_info->max_pipelined)
    {
      /* Connection available for reuse */
      ...
    }
  else if (host_info->n_connections < host_info->max_connections
        && client->n_connections < client->max_connections)
    {
      /* Create a new connection */
      ...
    }
  else (options->block_if_busy
     && host_info->n_unassigned_requests < host_info->max_unassigned_requests)
    {
      /* Enqueue our request in the wait-queue */
      ...
    }
  else
    {
      dsk_set_error (error, "too many connections in HTTP client");
      return DSK_FALSE;



