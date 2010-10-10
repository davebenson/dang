#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* These headers are exclusively to call open(2)/fstat(2):
   they are used for serving a file and could easily be moved
   to dsk-http-server-stream -- except maybe for error handling */
   
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "dsk.h"

#include "../gsklistmacros.h"

/* === Request Matching Infrastructure === */
/* a compiled test of a header */
typedef struct _MatchTester MatchTester;
typedef struct _MatchTesterStandard MatchTesterStandard;

typedef void (*FunctionPointer)();

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
  HANDLER_TYPE_CGI
} HandlerType;
struct _Handler
{
  HandlerType handler_type;
  FunctionPointer handler;
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
#define GET_HANDLER_QUEUE(match_test_node)      \
  Handler*,                                     \
  (match_test_node)->first_handler,             \
  (match_test_node)->last_handler,              \
  next

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
          test_str = request->transfer->request->path;
          break;
        case DSK_HTTP_SERVER_MATCH_HOST:
          test_str = request->transfer->request->host;
          break;
        case DSK_HTTP_SERVER_MATCH_USER_AGENT:
          test_str = request->transfer->request->user_agent;
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


static void
add_handler_generic (DskHttpServer *server,
                     HandlerType handler_type,
                     FunctionPointer handler_func,
                     void *handler_data,
                     DskHookDestroy handler_destroy)
{
  Handler *handler = dsk_malloc (sizeof (Handler));
  handler->handler_type = handler_type;
  handler->handler = handler_func;
  handler->handler_data = handler_data;
  handler->handler_destroy = handler_destroy;
  handler->next = NULL;

  /* Add to handler list */
  GSK_QUEUE_ENQUEUE (GET_HANDLER_QUEUE (server->current), handler);
}

void
dsk_http_server_register_streaming_post_handler (DskHttpServer *server,
                                                 DskHttpServerStreamingPostFunc func,
                                                 void *func_data,
                                                 DskHookDestroy destroy)
{
  add_handler_generic (server, HANDLER_TYPE_STREAMING_POST_DATA,
                       (FunctionPointer) func, func_data, destroy);
}


/* === Handling Requests === */
typedef enum
{
  REQUEST_HANDLING_INIT,
  REQUEST_HANDLING_INVOKING,
  REQUEST_HANDLING_BLOCKED_ERROR,
  REQUEST_HANDLING_BLOCKED_PASS,
  REQUEST_HANDLING_BLOCKED_INTERNAL_REDIRECT,
  REQUEST_HANDLING_WAITING,     /* for a respond() type function */
  REQUEST_HANDLING_GOT_RESPONSE_WHILE_INVOKING,
  REQUEST_HANDLING_GOT_RESPONSE
} RequestHandlingState;

typedef struct _RealServerRequest RealServerRequest;
struct _RealServerRequest
{
  DskHttpServerRequest request;
  MatchTestNode *node;
  Handler *handler;

  /* are we currently invoking the handler's function */
  RequestHandlingState state;

  /* For BLOCKED_ERROR state */
  char *blocked_error;
  DskHttpStatus blocked_error_status;
};

static void
real_server_request_free (RealServerRequest *rreq)
{
  dsk_free (rreq);
}

/* An error response without the flag handling -- for use in other "respond" functions */
static void
respond_error (DskHttpServerRequest *request,
               DskHttpStatus         status,
               const char           *message)
{
  /* Configure response */
  RealServerRequest *rreq = (RealServerRequest *) request;
  DskHttpServerStreamResponseOptions resp_options = DSK_HTTP_SERVER_STREAM_RESPONSE_OPTIONS_DEFAULT;
  DskHttpResponseOptions header_options = DSK_HTTP_RESPONSE_OPTIONS_DEFAULT;
  resp_options.header_options = &header_options;
  header_options.status_code = status;
  header_options.content_type = "text/html/UTF-8";

  /* compute content */
  DskBuffer content_buffer = DSK_BUFFER_STATIC_INIT;
  DskPrint *print = dsk_print_new_buffer (&content_buffer);
  dsk_print_set_string (print, "status_message", dsk_http_status_get_message (status));
  dsk_print_set_uint (print, "status_code", status);
  dsk_print_set_string (print, "message", message);
  dsk_print_set_filtered_string (print, "html_escaped_message", message,
                                 dsk_xml_escape_new ());
  dsk_print (print, "<html>"
                    "<head><title>Error $status: $status_message</title></head>"
                    "<body>"
                    "<h1>Error $status: $status_message</h1>"
                    "<p><pre>\n"
                    "$html_escaped_message\n"
                    "</pre></p>"
                    "</body>"
                    "</html>");
  dsk_print_free (print);
  uint8_t *content_data = dsk_malloc (content_buffer.size);
  resp_options.content_data = content_data;
  resp_options.content_length = content_buffer.size;
  dsk_buffer_read (&content_buffer, content_buffer.size, content_data);

  /* Respond */
  DskError *error = NULL;
  if (!dsk_http_server_stream_respond (request->transfer, &resp_options, &error))
    {
      dsk_warning ("error responding with error (original error='%s'; second error='%s')",
                   message, error->message);
      dsk_error_unref (error);
    }
  dsk_free (content_data);

  /* Free request */
  real_server_request_free (rreq);
}

/* === One of these functions should be called by any handler === */
void dsk_http_server_request_respond          (DskHttpServerRequest *request,
                                               DskHttpServerResponseOptions *options)
{
  RealServerRequest *rreq = (RealServerRequest *) request;
  dsk_assert (rreq->state == REQUEST_HANDLING_INVOKING
          ||  rreq->state == REQUEST_HANDLING_WAITING);


  /* Transform options */
  DskHttpServerStreamResponseOptions soptions = DSK_HTTP_SERVER_STREAM_RESPONSE_OPTIONS_DEFAULT;
  DskHttpResponseOptions header_options = DSK_HTTP_RESPONSE_OPTIONS_DEFAULT;
  soptions.header_options = &header_options;
  
  dsk_boolean must_unref_content_stream = DSK_FALSE;
  soptions.content_stream = options->source;
  if (options->source_filename)
    {
      /* XXX: need a function for this */
      int fd = open (options->source_filename, O_RDONLY);
      if (fd < 0)
        {
          /* construct error message */
          int open_errno = errno;
          DskBuffer content_buffer = DSK_BUFFER_STATIC_INIT;
          DskPrint *print = dsk_print_new_buffer (&content_buffer);
          dsk_print_set_string (print, "filename", options->source_filename);
          dsk_print_set_string (print, "error_message", strerror (open_errno));
          dsk_print (print, "error opening $filename: $error_message");
          dsk_print_free (print);
          char *error_message = dsk_buffer_clear_to_string (&content_buffer);

          /* serve response */
          dsk_http_server_request_respond_error (request, 404, error_message);
          dsk_free (error_message);
          return;
        }
      struct stat stat_buf;
      if (fstat (fd, &stat_buf) < 0)
        {
          dsk_warning ("error calling fstat: %s", strerror (errno));
        }
      else
        {
          /* try to setup content-length */
          if (S_ISREG (stat_buf.st_mode))
            soptions.content_length = stat_buf.st_mode;
        }
      DskError *error = NULL;
      DskOctetStreamFdSource *fdsource;
      if (!dsk_octet_stream_new_fd (fd,
                                    DSK_FILE_DESCRIPTOR_IS_READABLE
                                    |DSK_FILE_DESCRIPTOR_IS_NOT_WRITABLE,
                                    NULL,        /* do not need stream */
                                    &fdsource,
                                    NULL,        /* no sink */
                                    &error))
        {
          dsk_add_error_prefix (&error, "opening %s", options->source_filename);
          dsk_http_server_request_respond_error (request, 404, error->message);
          dsk_error_unref (error);
          close (fd);
          return;
        }
      soptions.content_stream = (DskOctetSource *) fdsource;
      must_unref_content_stream = DSK_TRUE;
    }
  header_options.content_type = options->content_type;
  header_options.content_main_type = options->content_main_type;
  header_options.content_sub_type = options->content_sub_type;
  header_options.content_charset = options->content_charset;

  /* Do lower-level response */
  DskError *error = NULL;
  if (!dsk_http_server_stream_respond (request->transfer, &soptions, &error))
    {
      dsk_http_server_request_respond_error (request, 500, error->message);
      dsk_error_unref (error);
      if (must_unref_content_stream)
        dsk_object_unref (soptions.content_stream);
      return;
    }

  /* State transitions */
  if (rreq->state == REQUEST_HANDLING_INVOKING)
    rreq->state = REQUEST_HANDLING_GOT_RESPONSE_WHILE_INVOKING;
  else
    {
      /* free RealServerRequest */
      rreq->state = REQUEST_HANDLING_GOT_RESPONSE;
      real_server_request_free (rreq);
    }
}


void dsk_http_server_request_respond_error    (DskHttpServerRequest *request,
                                               DskHttpStatus         status,
                                               const char           *message)
{
  RealServerRequest *rreq = (RealServerRequest *) request;
  dsk_assert (rreq->state == REQUEST_HANDLING_WAITING
          ||  rreq->state == REQUEST_HANDLING_INVOKING);
  if (rreq->state == REQUEST_HANDLING_INVOKING)
    {
      rreq->state = REQUEST_HANDLING_BLOCKED_ERROR;
      rreq->blocked_error = dsk_strdup (message);
      rreq->blocked_error_status = status;
    }
  else
    {
      respond_error (request, status, message);
    }
}

void dsk_http_server_request_redirect         (DskHttpServerRequest *request,
                                               DskHttpStatus         status,
                                               const char           *location)
{
  DskHttpServerStreamResponseOptions resp_options = DSK_HTTP_SERVER_STREAM_RESPONSE_OPTIONS_DEFAULT;
  DskHttpResponseOptions header_options = DSK_HTTP_RESPONSE_OPTIONS_DEFAULT;
  if (!(300 <= status && status <= 399))
    {
      dsk_http_server_request_respond_error (request, 500,
                                             "got non-3XX code for redirect");
      return;
    }
  resp_options.header_options = &header_options;
  header_options.status_code = status;
  header_options.location = (char*)location;

  /* use dsk_print to format content body */
  DskBuffer content_buffer = DSK_BUFFER_STATIC_INIT;
  DskPrint *print = dsk_print_new_buffer (&content_buffer);
  dsk_print_set_uint (print, "status_code", status);
  dsk_print_set_string (print, "location", location);
  dsk_print_set_filtered_string (print, "location_html_escaped", location,
                                 dsk_xml_escape_new ());
  dsk_print (print, "<html><head><title>See $location_html_escaped</title></head>"
             "<body>Please go <a href=\"$location\">here</a></body>"
             "</html>\n");
  dsk_print_free (print);

  resp_options.content_length = content_buffer.size;
  uint8_t *content_data = dsk_malloc (content_buffer.size);
  dsk_buffer_read (&content_buffer, content_buffer.size, content_data);
  resp_options.content_data = content_data;
  header_options.content_type = "text/html/UTF-8";

  DskError *error = NULL;
  if (!dsk_http_server_stream_respond (request->transfer, &resp_options, &error))
    {
      dsk_warning ("error making redirect respond: %s", error->message);
      dsk_error_unref (error);
    }
  dsk_free (content_data);
}

void dsk_http_server_request_internal_redirect(DskHttpServerRequest *request,
                                               const char           *new_path)
{
  RealServerRequest *rreq = (RealServerRequest *) request;
  dsk_assert (rreq->state == REQUEST_HANDLING_INVOKING
          ||  rreq->state == REQUEST_HANDLING_WAITING);

  /* Create the new HTTP request correspond to this redirect */
  ...

  /* Do we want to handle CGI variables? */
  /* XXX: at LEAST we should flush them */
    ...

  if (!advance_to_next_handler (rreq))
    {
      respond_no_handler_found (info);
      return;
    }

  dsk_assert (rreq->handler != NULL)
  if (rreq->state == REQUEST_HANDLING_INVOKING)
    rreq->state = REQUEST_HANDLING_BLOCKED_INTERNAL_REDIRECT;
  else
    invoke_handler (rreq);
}

void dsk_http_server_request_pass             (DskHttpServerRequest *request)
{
  ...
}


static dsk_boolean
match_test_node_matches    (MatchTestNode *node,
                            DskHttpServerRequest *request)
{
  if (node->tester == NULL)
    return DSK_TRUE;
  else
    return node->tester->test (node->tester, request);
}

static MatchTestNode *
find_first_match_recursive (MatchTestNode *node,
                            DskHttpServerRequest *request)
{
  if (match_test_node_matches (node, request))
    {
      MatchTestNode *child;
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
                  DskHttpServerRequest *request)
{
  return find_first_match_recursive (&server->top, request);
}

static MatchTestNode *
find_next_match  (DskHttpServer  *server,
                  MatchTestNode  *node,
                  DskHttpServerRequest *request)
{
  MatchTestNode *at;
  DSK_UNUSED (server);
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

static dsk_boolean
advance_to_next_handler (RealServerRequest *info)
{
  if (info->handler->next != NULL)
    info->handler = info->handler->next;
  else
    {
      info->node = find_next_match (info->request.bind_info->server,
                                    info->node,
                                    &info->request);
      if (info->node == NULL)
        return DSK_FALSE;
      info->handler = info->node->first_handler;
    }
  return DSK_TRUE;
}

static void
compute_cgi_vars (RealServerRequest *info)
{
  ...

  /* do we have a handler waiting on us? */
  ...
}

static void
begin_computing_cgi_vars (RealServerRequest *info)
{
  DskHttpVerb verb = info->request.transfer->request->verb;

  /* Not a POST or PUT request, therefore we don't expect CGI vars */
  if (verb != DSK_HTTP_VERB_PUT
   && verb != DSK_HTTP_VERB_POST)
    {
      compute_cgi_vars (info);
      return;
    }
  if (post_data->done_adding)
    {
      compute_cgi_vars (info);
      return;
    }
  
  /* trap POST completion callback (callback=compute_cgi_vars) */
  ...
}

static void
invoke_handler (RealServerRequest *info)
{
restart:
  /* Do the actual handler invocation,
     with 'in_handler' acting as a reentrancy guard,
     so that dsk_http_server_request_respond_*()
     calls during handler invocation are processed in a loop
     (see 'if (info->in_handler_got_result)' below) instead
     of recursing to cause a very large stack. */
  dsk_assert (info->state == REQUEST_HANDLING_INIT);
  info->state = REQUEST_HANDLING_INVOKING;
  switch (info->handler->handler_type)
    {
    case HANDLER_TYPE_STREAMING_POST_DATA:
      {
        /* Invoke handler immediately */
        DskHttpServerStreamingPostFunc func = (DskHttpServerStreamingPostFunc) info->handler->handler;
        DskOctetSource *post_data = (DskOctetSource *) info->request.transfer->post_data;
        func (&info->request, post_data, info->handler->handler_data);
      }
      break;
    case HANDLER_TYPE_CGI:
      {
        if (!info->request.cgi_vars_computed)
          begin_computing_cgi_vars (info);      /* may finish immediately! */
        if (info->request.cgi_vars_computed)
          {
            /* Invoke handler immediately */
            DskHttpServerCGIFunc func = (DskHttpServerCGIFunc) info->handler->handler;
            func (&info->request, info->handler->handler_data);
          }
        else
          {
            /* Handler will be invoked once cgi vars are obtained 
               (ie after waiting for POST data) */
          }
      }
      break;
    }
  switch (info->state)
    {
    case REQUEST_HANDLING_INIT:
      dsk_assert_not_reached ();
    case REQUEST_HANDLING_INVOKING:
      info->state = REQUEST_HANDLING_WAITING;
      break;
    case REQUEST_HANDLING_BLOCKED_ERROR:
      {
        /* this will free the handler */
        char *msg = rreq->blocked_error;
        rreq->blocked_error = NULL;
        respond_error (rreq, msg, rreq->blocked_error_status);
        dsk_free (msg);
        return;
      }
    case REQUEST_HANDLING_BLOCKED_PASS:
      info->state = REQUEST_HANDLING_INIT;
      if (!advance_to_next_handler (info))
        {
          respond_no_handler_found (info);
          return;
        }
      else
        goto restart;
    case REQUEST_HANDLING_BLOCKED_INTERNAL_REDIRECT:
      info->state = REQUEST_HANDLING_INIT;
      goto restart;
    case REQUEST_HANDLING_WAITING:
    case REQUEST_HANDLING_GOT_RESPONSE:
      dsk_assert_not_reached ();
    case REQUEST_HANDLING_GOT_RESPONSE_WHILE_INVOKING:
      ... free handler
      return;
    }
}

static dsk_boolean
handle_http_server_request_available (DskHttpServerStream *sstream,
                                      DskHttpServerBindInfo            *bind_info)
{
  MatchTestNode *node;
  DskHttpServerStreamTransfer *xfer;
  xfer = dsk_http_server_stream_get_request (stream);
  if (xfer == NULL)
    return DSK_TRUE;

  request_info = dsk_malloc0 (sizeof (RealServerRequest));
  request_info->transfer = xfer;
  request_info->bind_info = bind_info;
  xfer->user_data = handler_info;

  node = find_first_match (bind_info->server, xfer->request);
  if (node == NULL)
    {
      respond_no_handler_found (info);
      return DSK_TRUE;
    }
  request_info->node = node;
  request_info->handler = node->first_handler;

  /* invoke handler */
  return DSK_TRUE;
}

static dsk_boolean
handle_listener_ready (DskOctetListener *listener,
                       DskHttpServerBindInfo         *bind_info)
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
    case DSK_IO_RESULT_EOF
      ...
      return DSK_FALSE;
    }

  http_stream = dsk_http_server_stream_new (sink, source,
                                            &bind_info->server_options);
  dsk_hook_trap (&http_stream->request_available,
                 (DskHookFunc) handle_http_server_request_available,
                 bind_info,             //???
                 NULL);
  dsk_object_unref (http_stream);
}

static void
bind_info_destroyed (DskHttpServerBindInfo *bind_info)
{
  DskHttpServerBindInfo **p;
  for (p = &bind_info->server->bind_infos;
       *p != NULL;
       p = &((*p)->next))
    if (*p == bind_info)
      {
        *p = bind_info->next;
        dsk_object_unref (bind_info->listener);
        dsk_free (bind_info);
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

