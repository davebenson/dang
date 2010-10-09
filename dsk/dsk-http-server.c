#include "../gsklistmacros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "dsk.h"

/* a compiled test of a header */
typedef struct _MatchTester MatchTester;
typedef struct _MatchTesterStandard MatchTesterStandard;


struct _MatchTester
{
  dsk_boolean (*test)(MatchTester *tester,
                      DskHttpServerRequest *request);
  void        (*destroy) (MatchTester *tester);
};

typedef struct
{
  DskHttpServerMatchType match_type;
  DskPattern *test;
} MatchTypeAndPattern;
struct _MatchTesterStandard
{
  MatchTester base;
  unsigned n_types;
  MatchTypeAndPattern types[1];
};

/* an uncompiled list of match information */
typedef struct _MatchTestList MatchTestList;
struct _MatchTestList
{
  DskHttpServerMatchType match_type;
  char *pattern_str;
  MatchTestList *prev;
};

/* info about a listening port */
struct _DskHttpServerBindInfo
{
  DskHttpServer *server;
  DskHttpServerBindInfo *next;
  DskHttpServerStreamOptions server_stream_options;
  DskOctetListener *listener;

  dsk_boolean is_local;
  unsigned bind_port;
  char *bind_local_path;
};


typedef struct _Handler Handler;
typedef enum
{
  HANDLER_TYPE_STREAMING_POST_DATA,
  HANDLER_TYPE_STREAMING_CGI
} HandlerType;
struct _Handler
{
  HandlerType handler_type;
  void (*handler_func)(void);
  void *handler_data;
  DskHookDestroy handler_destroy;
  Handler *next;
};

typedef struct _MatchTestNode MatchTestNode;
struct _MatchTestNode
{
  dsk_boolean under_construction;
  MatchTestList *test_list;     /* only while under_construction */
    
  MatchTester *tester;  /* created on demand, freed when test_list modded */

  MatchTestNode *parent;
  MatchTestNode *first_child, *last_child;
  MatchTestNode *prev_sibling, *next_sibling;

  Handler *first_handler, *last_handler;
};

struct _DskHttpServer
{
  MatchTestNode top;
  MatchTestNode *current;
  DskHttpServerStreamOptions server_stream_options;
  DskHttpServerBindInfo *bind_sites;
};

void dsk_http_server_add_match                 (DskHttpServer        *server,
                                                DskHttpServerMatchType type,
                                                const char           *pattern)
{
  MatchTestList *new_node = dsk_malloc (sizeof (MatchTestList) + strlen (pattern) + 1);
  new_node->pattern_str = (char*)(new_node + 1);
  strcpy (new_node->pattern_str, pattern);
  new_node->match_type = type;
  new_node->prev = server->current->test_list;
  server->current->test_list = new_node;
  if (server->current->tester != NULL)
    server->current->tester->destroy (server->current->tester);
}

void dsk_http_server_match_save                (DskHttpServer        *server)
{
  MatchTestNode *node = dsk_malloc0 (sizeof (MatchTestNode));
  node->under_construction = DSK_TRUE;

  node->parent = server->current;

  /* add to sibling list of current */
  node->prev_sibling = server->current->last_child;
  node->next_sibling = NULL;
  if (server->current->first_child == NULL)
    server->current->first_child = node;
  else
    server->current->last_child->next_sibling = node;
  server->current->last_child = node;

  server->current = node;
}

static dsk_boolean
match_tester_standard_test (MatchTester *tester,
                            DskHttpServerRequest *request)
{
  MatchTesterStandard *std = (MatchTesterStandard *) tester;
  unsigned i;
  for (i = 0; i < std->n_types; i++)
    {
      const char *test_str = NULL;
      char tmp_buf[64];
      switch (std->types[i].match_type)
        {
        case DSK_HTTP_SERVER_MATCH_PATH:
          test_str = request->xfer->request->path;
          break;
        case DSK_HTTP_SERVER_MATCH_HOST:
          test_str = request->xfer->request->host;
          break;
        case DSK_HTTP_SERVER_MATCH_USER_AGENT:
          test_str = request->xfer->request->user_agent;
          break;
        case DSK_HTTP_SERVER_MATCH_BIND_PORT:
          if (!request->bind_info->is_local)
            {
              snprintf (tmp_buf, sizeof (tmp_buf), "%u", 
                        request->bind_info->bind_port);
              test_str = tmp_buf;
            }
          break;
        case DSK_HTTP_SERVER_MATCH_BIND_PATH:
          if (!request->bind_info->is_local)
            test_str = request->bind_info->bind_local_path;
          break;
        }
      if (test_str == NULL)
        return DSK_FALSE;
      if (!dsk_pattern_match (std->types[i].test, test_str))
        return DSK_FALSE;
    }
  return DSK_TRUE;
}

static void
match_tester_standard_destroy (MatchTester *tester)
{
  MatchTesterStandard *std = (MatchTesterStandard *) tester;
  unsigned i;
  for (i = 0; i < std->n_types; i++)
    dsk_pattern_free (std->types[i].test);
  dsk_free (std);
}

static MatchTester *
create_tester (MatchTestList *list)
{
  if (list == NULL)
    return NULL;
#define GET_MATCH_TEST_LIST(list) MatchTestList*, list, prev
#define COMPARE_MATCH_TEST_LIST_BY_TEST_TYPE(a,b,rv) \
  rv = a->match_type < b->match_type ? -1 : a->match_type > b->match_type ? 1 : 0;
  GSK_STACK_SORT (GET_MATCH_TEST_LIST (list),
                  COMPARE_MATCH_TEST_LIST_BY_TEST_TYPE);
#undef COMPARE_MATCH_TEST_LIST_BY_TEST_TYPE
#undef GET_MATCH_TEST_LIST

  /* Count the number of unique types,
     and the max count of any given type. */
  unsigned max_run_length = 0, n_unique_types = 0;
  MatchTestList *at;
  for (at = list; at; )
    {
      MatchTestList *run_end = at->prev;
      unsigned run_length = 1;
      while (run_end && run_end->match_type == at->match_type)
        {
          run_length++;
          run_end = run_end->prev;
        }
      if (run_length > max_run_length)
        max_run_length = run_length;
      n_unique_types++;
      at = run_end;
    }

  MatchTesterStandard *tester;
  tester = dsk_malloc (sizeof (MatchTesterStandard)
                       + sizeof (MatchTypeAndPattern) * (n_unique_types-1));
  tester->base.test = match_tester_standard_test;
  tester->base.destroy = match_tester_standard_destroy;
  tester->n_types = n_unique_types;
  DskPatternEntry *patterns;
  patterns = alloca (sizeof (DskPatternEntry) * max_run_length);
  n_unique_types = 0;
  for (at = list; at; )
    {
      MatchTestList *run_end = at;
      unsigned run_length = 0;
      while (run_end && run_end->match_type == at->match_type)
        {
          patterns[run_length].pattern = run_end->pattern_str;
          patterns[run_length].result = tester;
          run_length++;
          run_end = run_end->prev;
        }
      DskError *error = NULL;
      tester->types[n_unique_types].test = dsk_pattern_compile (run_length, patterns, &error);
      if (tester->types[n_unique_types].test == NULL)
        {
#if 0
          if (server->config_error_trap)
            server->config_error_trap (error, server->config_error_trap_data);
#endif
          dsk_warning ("error compiling pattern for match-type %d, %s",
                       at->match_type, error->message);
          dsk_error_unref (error);
          return NULL;
        }
      tester->types[n_unique_types].match_type = at->match_type;
      n_unique_types++;
      at = run_end;
    }
  return (MatchTester *) tester;
}

void dsk_http_server_match_restore             (DskHttpServer        *server)
{
  dsk_return_if_fail (server->current != &server->top, "cannot 'restore' after last frame");

  server->current->under_construction = DSK_FALSE;

  /* Create tester object, if needed */
  if (server->current->tester != NULL)
    server->current->tester = create_tester (server->current->test_list);

  /* Free the test information */
  while (server->current->test_list != NULL)
    {
      MatchTestList *next = server->current->test_list->prev;
      dsk_free (server->current->test_list);
      server->current->test_list = next;
    }

  /* Now back to parent context */
  server->current = server->current->parent;
}

void
dsk_http_server_register_streaming_post_handler (DskHttpServer *server,
                                                 DskHttpServerStreamingPostFunc func,
                                                 void *func_data,
                                                 DskHookDestroy destroy)
{
  ...
}

/* One of these functions should be called by any handler */
void dsk_http_server_request_respond_stream   (DskHttpServerRequest *request,
                                               DskOctetSource       *source);
void dsk_http_server_request_respond_data     (DskHttpServerRequest *request,
                                               size_t                length,
                                               const uint8_t        *data);
void dsk_http_server_request_respond_file     (DskHttpServerRequest *request,
                                               const char           *filename);
void dsk_http_server_request_respond_error    (DskHttpServerRequest *request,
                                               DskHttpStatus         status,
                                               const char           *message);
void dsk_http_server_request_redirect         (DskHttpServerRequest *request,
                                               DskHttpStatus         status,
                                               const char           *location);
void dsk_http_server_request_internal_redirect(DskHttpServerRequest *request,
                                               const char           *new_path);
void dsk_http_server_request_pass             (DskHttpServerRequest *request);

/* --- used to configure our response a little --- */
void dsk_http_server_request_set_content_type (DskHttpServerRequest *request,
                                               const char           *triple);
void dsk_http_server_request_set_content_type_parts
                                              (DskHttpServerRequest *request,
                                               const char           *type,
                                               const char           *subtype,
                                               const char           *charset);
static dsk_boolean
match_test_node_matches    (MatchTestNode *node,
                            DskHttpRequest *request)
{
  if (node->tester == NULL)
    return DSK_TRUE;
  else
    return node->tester->test (node->tester, request);
}

static MatchTestNode *
find_first_match_recursive (MatchTestNode *node,
                            DskHttpRequest *request)
{
  if (match_test_node_matches (node, request))
    {
      for (child = node->first_child; child; child = child->next_sibling)
        {
          MatchTestNode *rv = find_first_match_recursive (child, request);
          if (rv != NULL)
            return rv;
        }
      if (node->first_handler != NULL)
        return node;
    }
  return NULL;
}

static MatchTestNode *
find_first_match (DskHttpServer *server,
                  DskHttpRequest *request)
{
  return find_first_match_recursive (&server->top, request);
}
static MatchTestNode *
find_next_match  (DskHttpServer  *server,
                  MatchTestNode  *node,
                  DskHttpRequest *request)
{
  MatchTestNode *at;
  while (node)
    {
      for (at = node->next_sibling; at; at = at->next_sibling)
        {
          MatchTestNode *rv = find_first_match_recursive (at, request);
          if (rv)
            return rv;
        }
      node = node->parent;
      if (node->first_handler != NULL)
        return node;
    }
  return NULL;
}

typedef struct _RequestHandlerInfo RequestHandlerInfo;
struct _RequestHandlerInfo
{
  DskHttpServerStreamTransfer *transfer;
  MatchTestNode *node;
  DskHttpServerBindInfo *bind_site;
  dsk_boolean in_handler;
};

static void
handler_pass (RequestHandlerInfo *info)
{
  dsk_assert (info->in_handler == NULL);
  if (info->handler->next)
    info->handler = info->handler->next;
  else
    {
      info->node = find_next_match (info->bind_site->server, info->node,
                                    info->transfer->request);
      if (info->node == NULL)
        {
          respond_no_handler_found (info);
          return;
        }
      info->handler = info->node->first_handler;
    }
}

static dsk_boolean
handle_http_server_request_available (DskHttpServerStream *sstream,
                                      DskHttpServerBindInfo            *bind_site)
{
  MatchTestNode *node;
  DskHttpServerStreamTransfer *xfer;
  xfer = dsk_http_server_stream_get_request (stream);
  if (xfer == NULL)
    return DSK_TRUE;

  request_info = dsk_malloc0 (sizeof (RequestHandlerInfo));
  request_info->transfer = xfer;
  request_info->bind_site = bind_site;
  xfer->user_data = handler_info;

  node = find_first_match (bind_site->server, xfer->request);
  if (node == NULL)
    {
      respond_no_handler_found (info);
      return DSK_TRUE;
    }
  request_info->node = node;
  request_info->handler = node->first_handler;

  /* invoke handler */
  request_info->in_handler = DSK_TRUE;
  switch (handler->handler_type)
    {
    case HANDLER_TYPE_STREAMING_POST_DATA:
      ...
    case HANDLER_TYPE_STREAMING_CGI:
      ...
    }
  request_info->in_handler = DSK_FALSE;
  if (request_info->got_result)
    {
      switch (request_info->handler_result_type)
        {
        case HANDLER_RESULT_FILE:
          ...
        case HANDLER_RESULT_DATA:
          ...
        case HANDLER_RESULT_ERROR:
          ...
        case HANDLER_RESULT_REDIRECT:
          request_info->got_result = DSK_FALSE;
          ...
          break;
        case HANDLER_RESULT_INTERNAL_REDIRECT:
          request_info->got_result = DSK_FALSE;
          ...
          break;
        case HANDLER_RESULT_PASS:
          request_info->got_result = DSK_FALSE;
          handler_pass (request_info);
          break;
        }
    }
  return DSK_TRUE;
}

static dsk_boolean
handle_listener_ready (DskOctetListener *listener,
                       DskHttpServerBindInfo         *bind_site)
{
  DskOctetSource *source;
  DskOctetSink *sink;
  switch (dsk_octet_listener_accept (listener, NULL, &source, &sink, &error))
    {
    case DSK_IO_RESULT_ERROR:
      //
      ...
      return DSK_TRUE;
    case DSK_IO_RESULT_AGAIN:
      return DSK_TRUE;
    case DSK_IO_RESULT_SUCCESS:
      break;
    }

  http_stream = dsk_http_server_stream_new (sink, source,
                                            &bind_site->server_options);
  dsk_hook_trap (&http_stream->request_available,
                 (DskHookFunc) handle_http_server_request_available,
                 bind_site,             //???
                 NULL);
  dsk_object_unref (http_stream);
}

static void
bind_site_destroyed (DskHttpServerBindInfo *bind_site)
{
  DskHttpServerBindInfo **p;
  for (p = &bind_site->server->bind_sites;
       *p != NULL;
       p = &((*p)->next))
    if (*p == bind_site)
      {
        *p = bind_site->next;
        dsk_object_unref (bind_site->listener);
        dsk_free (bind_site);
        return;
      }
}

static dsk_boolean
do_bind (DskHttpServer *server,
         const DskOctetListenerSocketOptions *options,
         DskError **error)
{
  DskOctetListener *listener = dsk_octet_listener_socket_new (options, error);
  if (listener == NULL)
    return DSK_FALSE;
  bind_site = dsk_malloc (sizeof (DskHttpServerBindInfo));
  bind_site->listener = listener;
  bind_site->server = server;
  bind_site->next = server->bind_sites;
  bind_site->server_stream_options = server->server_stream_options;
  server->bind_sites = bind_site;

  dsk_hook_trap (&listener->incoming,
                 (DskHookFunc) handle_listener_ready,
                 bind_site,
                 (DskHookDestroy) bind_site_destroyed);

  return DSK_TRUE;
}

dsk_boolean dsk_http_server_bind_tcp           (DskHttpServer        *server,
                                                DskIpAddress         *bind_addr,
                                                unsigned              port,
                                                DskError            **error)
{
  DskOctetListenerSocketOptions listener_opts = DSK_OCTET_LISTENER_SOCKET_OPTIONS_DEFAULT;
  listener_opts.is_local = DSK_FALSE;
  if (bind_addr)
    listener_opts.bind_address = *bind_addr;
  listener_opts.bind_port = port;
  bind_info = do_bind (server, &listener_opts, error);
  if (bind_info == NULL)
    return DSK_FALSE;
  bind_info->is_local = DSK_FALSE;
  bind_info->bind_port = port;
  return DSK_TRUE;
}


dsk_boolean dsk_http_server_bind_local         (DskHttpServer        *server,
                                                const char           *path,
                                                DskError            **error)
{
  DskOctetListenerSocketOptions listener_opts = DSK_OCTET_LISTENER_SOCKET_OPTIONS_DEFAULT;
  listener_opts.is_local = DSK_TRUE;
  listener_opts.local_path = path;
  bind_info = do_bind (server, &listener_opts, error);
  if (bind_info == NULL)
    return DSK_FALSE;
  bind_info->is_local = DSK_TRUE;
  bind_info->bind_local_path = dsk_strdup (path);
  return DSK_TRUE;
}

