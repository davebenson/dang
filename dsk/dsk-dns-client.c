#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "dsk.h"
#include "dsk-flagged-pointer.h"
#include "../gskrbtreemacros.h"
#include "../gsklistmacros.h"

#define MAX_CNAMES                 16
#define TIME_WAIT_TO_RECYCLE_ID    60


#define NO_EXPIRE_TIME  ((unsigned)(-1))

static unsigned retry_schedule[] = { 1000, 2000, 3000, 4000 };

typedef struct _LookupData LookupData;
struct _LookupData
{
  unsigned n_cnames;
  char **cnames;
  DskDnsLookupFunc callback;
  void *callback_data;
};


typedef struct _Waiter Waiter;
struct _Waiter
{
  DskDnsCacheEntryFunc func;
  void *data;
  Waiter *next;
};

struct _DskDnsCacheEntryJob
{
  DskDnsCacheEntry *owner;

  /* users waiting on this job to finish */
  Waiter *waiters;

  unsigned ns_index;
  uint8_t attempt;
  uint8_t waiting_to_send;
  DskDispatchTimer *timer;

  /* the question, stored as binary */
  unsigned message_len;
  uint8_t *message;

  DskDnsCacheEntryJob *prev_waiting, *next_waiting;
};



typedef struct _NameserverInfo NameserverInfo;
struct _NameserverInfo
{
  DskIpAddress address;
  unsigned n_requests;
  unsigned n_responses;
};
static void
nameserver_info_init (NameserverInfo *init,
                      const DskIpAddress *addr)
{
  init->address = *addr;
  init->n_requests = init->n_responses = 0;
}


/* TODO: plugable random number generator.  or mersenne twister import */
static unsigned
random_int (unsigned n)
{
  dsk_assert (n > 0);
  if (n == 1)
    return 0;
  else
    return rand () / (RAND_MAX / n + 1);
}

static void
handle_cache_entry_lookup (DskDnsCacheEntry *entry,
                           void             *data)
{
  LookupData *lookup_data = data;
  DskDnsLookupResult result;
  switch (entry->type)
    {
    case DSK_DNS_CACHE_ENTRY_IN_PROGRESS:
      dsk_assert_not_reached ();
    case DSK_DNS_CACHE_ENTRY_BAD_RESPONSE:
      result.type = DSK_DNS_LOOKUP_RESULT_BAD_RESPONSE;
      result.addr = NULL;
      /* CONSIDER: adding cname chain somewhere */
      result.message = entry->info.bad_response.message;
      lookup_data->callback (&result, lookup_data->callback_data);
      dsk_free (lookup_data);
      return;

    case DSK_DNS_CACHE_ENTRY_NEGATIVE:
      result.type = DSK_DNS_LOOKUP_RESULT_NOT_FOUND;
      result.addr = NULL;
      result.message = "not found";
      /* CONSIDER: adding cname chain somewhere */
      lookup_data->callback (&result, lookup_data->callback_data);
      dsk_free (lookup_data);
      return;
    case DSK_DNS_CACHE_ENTRY_CNAME:
      {
        unsigned i;
        /* check existing cname list for circular references */
        for (i = 0; i < lookup_data->n_cnames; i++)
          if (strcmp (lookup_data->cnames[i], entry->info.cname) == 0)
            {
              result.type = DSK_DNS_LOOKUP_RESULT_BAD_RESPONSE;
              result.addr = NULL;
              result.message = "circular cname loop";
              /* CONSIDER: adding cname chain somewhere */
              lookup_data->callback (&result, lookup_data->callback_data);
              dsk_free (lookup_data);
              return;
            }
      
        /* add to cname list */
        dsk_dns_lookup_cache_entry (entry->info.cname, entry->is_ipv6, handle_cache_entry_lookup, data);
        return;
      }
    case DSK_DNS_CACHE_ENTRY_ADDR:
      result.type = DSK_DNS_LOOKUP_RESULT_FOUND;
      result.addr = entry->info.addr.addresses
                  + random_int (entry->info.addr.n);
      result.message = NULL;
      lookup_data->callback (&result, lookup_data->callback_data);
      dsk_free (lookup_data);
      return;
    }
}

void    dsk_dns_lookup (const char       *name,
                        dsk_boolean       is_ipv6,
                        DskDnsLookupFunc  callback,
                        void             *callback_data)
{
  LookupData *lookup_data = dsk_malloc (sizeof (LookupData));
  lookup_data->n_cnames = 0;
  lookup_data->cnames = NULL;
  lookup_data->callback = callback;
  lookup_data->callback_data = callback_data;
  dsk_dns_lookup_cache_entry (name, is_ipv6, handle_cache_entry_lookup, lookup_data);
} 

/* --- globals --- */
static DskUdpSocket *dns_udp_socket = NULL;
static DskHookTrap *dns_udp_socket_trap = NULL;
static DskDnsCacheEntry *dns_cache = NULL;
static DskDnsCacheEntry *expiration_tree = NULL;
static unsigned next_nameserver_index = 0;
static DskDnsCacheEntryJob *first_waiting_to_send = NULL;
static DskDnsCacheEntryJob *last_waiting_to_send = NULL;

/* --- configuration --- */
static unsigned n_resolv_conf_ns = 0;
static NameserverInfo *resolv_conf_ns = NULL;
static unsigned n_resolv_conf_search_paths = 0;
static char **resolv_conf_search_paths = NULL;
static DskDnsCacheEntry *etc_hosts_tree = NULL;
static DskDnsConfigFlags config_flags = DSK_DNS_CONFIG_FLAGS_DEFAULT;
static dsk_boolean dns_initialized = DSK_FALSE;

#define CACHE_ENTRY_NAME_IS_RED(n)  n->name_type_is_red
#define CACHE_ENTRY_NAME_SET_IS_RED(n,v)  n->name_type_is_red=v
#define COMPARE_DNS_CACHE_ENTRY_BY_NAME_TYPE(a,b,rv) \
  rv = strcmp ((a)->name, (b)->name); \
  if (rv == 0) \
    { \
      if (a->is_ipv6 && !b->is_ipv6) rv = -1; \
      else if (!a->is_ipv6 && b->is_ipv6) rv = -1; \
    }
#define GET_ETC_HOSTS_TREE() \
  etc_hosts_tree, DskDnsCacheEntry *, CACHE_ENTRY_NAME_IS_RED, \
  CACHE_ENTRY_NAME_SET_IS_RED, \
  name_type_parent, name_type_left, name_type_right, \
  COMPARE_DNS_CACHE_ENTRY_BY_NAME_TYPE
#define GET_CACHE_BY_NAME_TREE() \
  dns_cache, DskDnsCacheEntry *, CACHE_ENTRY_NAME_IS_RED, \
  CACHE_ENTRY_NAME_SET_IS_RED, \
  name_type_parent, name_type_left, name_type_right, \
  COMPARE_DNS_CACHE_ENTRY_BY_NAME_TYPE

#define CACHE_ENTRY_EXPIRE_IS_RED(n)  n->name_type_is_red
#define CACHE_ENTRY_EXPIRE_SET_IS_RED(n,v)  n->name_type_is_red=v
#define COMPARE_DNS_CACHE_ENTRY_BY_EXPIRE(a,b,rv) \
  rv = a->expire_time < b->expire_time ? -1 \
     : a->expire_time > b->expire_time ? 1 \
     : a < b ? -1 \
     : a > b ? 1 \
     : 0
#define GET_EXPIRATION_TREE() \
  expiration_tree, DskDnsCacheEntry *, CACHE_ENTRY_EXPIRE_IS_RED, \
  CACHE_ENTRY_EXPIRE_SET_IS_RED, \
  name_type_parent, name_type_left, name_type_right, \
  COMPARE_DNS_CACHE_ENTRY_BY_EXPIRE
  

/* --- handling system files (resolv.conf and hosts) --- */

static dsk_boolean
dsk_dns_try_init (DskError **error)
{
  char buf[2048];
  FILE *fp;
  DskDnsCacheEntry *conflict;
  unsigned lineno;

  /* parse /etc/hosts */
  fp = fopen ("/etc/hosts", "r");
  if (fp == NULL)
    {
      dsk_set_error (error, "error opening %s: %s",
                     "/etc/hosts", strerror (errno));
      return DSK_FALSE;
    }
  lineno = 0;
  while (fgets (buf, sizeof (buf), fp) != NULL)
    {
      char *at = buf;
      const char *ip;
      const char *name;
      DskIpAddress addr;
      DskDnsCacheEntry *host_entry;
      ++lineno;
      DSK_ASCII_SKIP_SPACE (at);
      if (*at == '#')
        continue;
      ip = at;
      DSK_ASCII_SKIP_NONSPACE (at);
      *at++ = 0;
      DSK_ASCII_SKIP_SPACE (at);
      name = at;
      DSK_ASCII_SKIP_NONSPACE (at);
      *at = 0;
      if (*ip == 0 || *name == 0)
        {
          dsk_warning ("parsing /etc/hosts line %u: expected ip/name pair",
                       lineno);
          continue;
        }
      if (!dsk_ip_address_parse_numeric (ip, &addr))
        {
          dsk_warning ("parsing /etc/hosts line %u: error parsing ip address",
                       lineno);
          continue;
        }
      host_entry = dsk_malloc (sizeof (DskDnsCacheEntry)
                               + sizeof (DskIpAddress)
                               + strlen (name) + 1);

      host_entry->info.addr.addresses = (DskIpAddress*)(host_entry + 1);
      host_entry->name = (char*)(host_entry->info.addr.addresses + 1);
      host_entry->is_ipv6 = addr.type == DSK_IP_ADDRESS_IPV6;
      host_entry->expire_time = NO_EXPIRE_TIME;
      host_entry->type = DSK_DNS_CACHE_ENTRY_ADDR;
      host_entry->info.addr.n = 1;
      host_entry->info.addr.i = 0;
      host_entry->info.addr.addresses[0] = addr;
retry:
      GSK_RBTREE_INSERT (GET_ETC_HOSTS_TREE (), host_entry, conflict);
      if (conflict != NULL)
        {
          GSK_RBTREE_REMOVE (GET_ETC_HOSTS_TREE (), conflict);
          goto retry;
        }
    }
  fclose (fp);

  /* parse /etc/resolv.conf */
  fp = fopen ("/etc/resolv.conf", "r");
  if (fp == NULL)
    {
      dsk_set_error (error, "error opening %s: %s",
                     "/etc/resolv.conf", strerror (errno));
      return DSK_FALSE;
    }
  lineno = 0;
  while (fgets (buf, sizeof (buf), fp) != NULL)
    {
      char *at = buf;
      char *command, *arg;
      ++lineno;
      while (*at && dsk_ascii_isspace (*at))
        at++;
      if (*at == '#')
        continue;
      DSK_ASCII_SKIP_SPACE (at);
      if (*at == '#')
        continue;
      command = at;
      while (*at && !dsk_ascii_isspace (*at))
        {
          if ('A' <= *at && *at <= 'Z')
            *at += ('a' - 'Z');
          at++;
        }
      *at++ = 0;
      DSK_ASCII_SKIP_SPACE (at);
      arg = at;
      DSK_ASCII_SKIP_NONSPACE (at);
      *at = 0;
      if (strcmp (command, "search") == 0)
        {
          const char *in;
          char *out;
          dsk_boolean dot_allowed;
          unsigned i;

          /* Add a searchpath to the list. */

          /* normalize argument (lowercase; check syntax) */
          in = out = arg;
          dot_allowed = DSK_FALSE;
          while (*in)
            {
              if (*in == '.')
                {
                  if (dot_allowed)
                    {
                      *out++ = '.';
                      dot_allowed = DSK_FALSE;
                    }
                }
              else if ('A' <= *in && *in <= 'Z')
                *out++ = *in + ('a' - 'A');
              else if (('0' <= *in && *in <= '9')
                    || ('a' <= *in && *in <= 'z')
                    || (*in == '-')
                    || (*in == '_'))
                *out++ = *in;
              else
                {
                  dsk_warning ("disallowed character '%c' in searchpath in /etc/resolv.conf line %u", *in, lineno);
                  goto next;
                }
              in++;
            }
          *out = 0;

          /* remove trailing dot, if it exists */
          if (!dot_allowed && out > arg)
            *(out-1) = 0;

          if (*arg == 0)
            {
              dsk_warning ("empty searchpath entry in /etc/resolv.conf (line %u)", lineno);
              goto next;
            }

          /* add if not already in set */
          for (i = 0; i < n_resolv_conf_search_paths; i++)
            if (strcmp (arg, resolv_conf_search_paths[i]) == 0)
              break;
          if (i < n_resolv_conf_search_paths)
            {
              dsk_warning ("searchpath '%s' appears twice in /etc/resolv.conf (line %u)",
                           arg, lineno);
            }
          else
            {
              resolv_conf_search_paths = dsk_realloc (resolv_conf_search_paths,
                                            (n_resolv_conf_search_paths+1) * sizeof (char *));
              resolv_conf_search_paths[n_resolv_conf_search_paths++] = dsk_strdup (arg);
            }
        }
      else if (strcmp (command, "nameserver") == 0)
        {
          /* add nameserver */
          DskIpAddress addr;
          unsigned i;
          if (!dsk_ip_address_parse_numeric (arg, &addr))
            {
              dsk_warning ("in /etc/resolv.conf, line %u: error parsing ip address", lineno);
              goto next;
            }
          for (i = 0; i < n_resolv_conf_ns; i++)
            if (dsk_ip_addresses_equal (&resolv_conf_ns[i].address, &addr))
              break;
          if (i < n_resolv_conf_ns)
            {
              dsk_warning ("in /etc/resolv.conf, line %u: nameserver %s already exists", lineno, arg);
            }
          else
            {
              resolv_conf_ns = dsk_realloc (resolv_conf_ns,
                                            (n_resolv_conf_ns+1) * sizeof (DskIpAddress));
              nameserver_info_init (&resolv_conf_ns[n_resolv_conf_ns++], &addr);
            }
        }
      else
        {
          dsk_warning ("unknown command '%s' in /etc/resolv.conf line %u",
                       command, lineno);
        }
next:
      ;
    }
  fclose (fp);
  if (n_resolv_conf_ns == 0)
    {
      dsk_set_error (error, "no nameservers given: cannot resolve names");
      return DSK_FALSE;
    }

  /* make UDP socket for queries */
  dns_udp_socket = dsk_udp_socket_new (DSK_FALSE, error);
  if (dns_udp_socket == NULL)
    {
      dsk_add_error_prefix (error, "initializing dns client");
      return DSK_FALSE;
    }

  return DSK_TRUE;

  dns_initialized = DSK_TRUE;
}

#define MAYBE_DNS_INIT_RETURN(error, error_rv)         \
  do {                                                 \
    if (!dns_initialized && !dsk_dns_try_init (error)) \
      return error_rv;                                 \
  }while(0)

/* --- low-level ---*/
DskDnsLookupNonblockingResult
dsk_dns_lookup_nonblocking (const char *name,
                           DskIpAddress *out,
                           dsk_boolean    is_ipv6,
                           DskError     **error)
{
  unsigned n_cnames = 0;
  MAYBE_DNS_INIT_RETURN (error, DSK_DNS_LOOKUP_NONBLOCKING_ERROR);
  do
    {
      DskDnsCacheEntry ce;
      DskDnsCacheEntry *entry;
      ce.name = (char*) name;
      ce.is_ipv6 = is_ipv6 ? 1 : 0;
      GSK_RBTREE_LOOKUP (GET_CACHE_BY_NAME_TREE (), &ce, entry);
      if (entry == NULL)
        return DSK_DNS_LOOKUP_NONBLOCKING_MUST_BLOCK;
      switch (entry->type)
        {
        case DSK_DNS_CACHE_ENTRY_IN_PROGRESS:
          return DSK_DNS_LOOKUP_NONBLOCKING_MUST_BLOCK;
        case DSK_DNS_CACHE_ENTRY_BAD_RESPONSE:
          dsk_set_error (error, "looking up %s: %s",
                         name,
                         entry->info.bad_response.message);
          return DSK_DNS_LOOKUP_NONBLOCKING_ERROR;
        case DSK_DNS_CACHE_ENTRY_NEGATIVE:
          return DSK_DNS_LOOKUP_NONBLOCKING_NOT_FOUND;
        case DSK_DNS_CACHE_ENTRY_CNAME:
          name = entry->info.cname;
          break;
        case DSK_DNS_CACHE_ENTRY_ADDR:
          *out = entry->info.addr.addresses[entry->info.addr.i];
          if (++entry->info.addr.i == entry->info.addr.n)
            entry->info.addr.i = 0;
          return DSK_DNS_LOOKUP_NONBLOCKING_FOUND;
        default:
          dsk_assert_not_reached ();
        }
      n_cnames++;
    }
  while (n_cnames < MAX_CNAMES);
  dsk_set_error (error, "too many cnames or cname loop");
  return DSK_DNS_LOOKUP_NONBLOCKING_ERROR;
}

static void
job_notify_waiters_and_free (DskDnsCacheEntryJob *job)
{
  /* notify waiters */
  while (job->waiters)
    {
      Waiter *w = job->waiters;
      job->waiters = w->next;

      w->func (job->owner, w->data);
      dsk_free (w);
    }

  /* free job */
  dsk_free (job->message);
  dsk_free (job);
}

static DskIOResult
job_send_message (DskDnsCacheEntryJob *job, DskError **error)
{
  return dsk_udp_socket_send_to_ip (dns_udp_socket,
                                    &resolv_conf_ns[job->ns_index].address,
                                    DSK_DNS_PORT,
                                    job->message_len, job->message,
                                    error);
}

static void clear_waiting_to_send_flag (DskDnsCacheEntryJob *job);
static void raise_waiting_to_send_flag (DskDnsCacheEntryJob *job);

static dsk_boolean
handle_socket_writable (void)
{
  while (first_waiting_to_send != NULL)
    {
      DskDnsCacheEntryJob *job = first_waiting_to_send;
      DskError *error = NULL;
      clear_waiting_to_send_flag (job);
      switch (job_send_message (job, &error))
        {
        case DSK_IO_RESULT_SUCCESS:
          /* wait for message or timeout */
          break;

        case DSK_IO_RESULT_AGAIN:
          raise_waiting_to_send_flag (job);
          return DSK_FALSE; /* do not run this trap again -
                               we have a new one */

        case DSK_IO_RESULT_EOF:
          dsk_assert_not_reached ();
          break;
        case DSK_IO_RESULT_ERROR:
          /* Treat this like a timeout */
          dsk_warning ("error sending UDP for DNS: %s", error->message);
          dsk_error_unref (error);
          break;

        default:
          dsk_assert_not_reached ();
        }
    }
  return DSK_FALSE;
}

#define GET_WAITING_TO_SEND_LIST() \
  DskDnsCacheEntryJob *, first_waiting_to_send, last_waiting_to_send, \
  prev_waiting, next_waiting
static void
raise_waiting_to_send_flag (DskDnsCacheEntryJob *job)
{
  if (!job->waiting_to_send)
    {
      if (first_waiting_to_send == NULL)
        {
          dns_udp_socket_trap
            = dsk_hook_trap (&dns_udp_socket->writable,
                             (DskHookFunc) handle_socket_writable,
                             NULL, NULL);
        }
      GSK_LIST_APPEND (GET_WAITING_TO_SEND_LIST (), job);
      job->waiting_to_send = DSK_TRUE;
    }
}
static void
clear_waiting_to_send_flag (DskDnsCacheEntryJob *job)
{
  if (job->waiting_to_send)
    {
      GSK_LIST_REMOVE (GET_WAITING_TO_SEND_LIST (), job);
      job->waiting_to_send = DSK_FALSE;
      if (first_waiting_to_send == NULL)
        {
          dsk_hook_trap_destroy (dns_udp_socket_trap);
          dns_udp_socket_trap = NULL;
        }
    }
}

static void
handle_timer_expired (DskDispatch *dispatch,
                      void        *data)
{
  DskDnsCacheEntryJob *job = data;
  DskError *error = NULL;
  (void) dispatch;
  (void) data;

  clear_waiting_to_send_flag (job);

  /* is this the last attempt? */
  if (job->attempt + 1 == DSK_N_ELEMENTS (retry_schedule))
    {
      /* Setup cache-entry */
      DskDnsCacheEntry *owner = job->owner;
      DskDnsCacheEntry *conflict;
      dsk_dispatch_remove_timer (job->timer);
      owner->type = DSK_DNS_CACHE_ENTRY_BAD_RESPONSE;
      owner->info.bad_response.message = dsk_strdup ("timed out waiting for response");
      owner->expire_time = dsk_get_current_time () + 1;
      GSK_RBTREE_INSERT (GET_EXPIRATION_TREE (), owner, conflict);
      dsk_assert (conflict == NULL);

      job_notify_waiters_and_free (job);
      return;
    }

  /* adjust timer */
  job->attempt += 1;
  dsk_dispatch_adjust_timer (job->timer,
                             retry_schedule[job->attempt]);

  /* try a different nameserver */
  job->ns_index += 1;
  if (job->ns_index == n_resolv_conf_ns)
    job->ns_index = 0;

  /* resend message */
  switch (job_send_message (job, &error))
    {
    case DSK_IO_RESULT_SUCCESS:
      /* wait for message or timeout */
      break;

    case DSK_IO_RESULT_AGAIN:
      raise_waiting_to_send_flag (job);
      break;

    case DSK_IO_RESULT_EOF:
      dsk_assert_not_reached ();
      break;
    case DSK_IO_RESULT_ERROR:
      /* Treat this like a timeout */
      dsk_warning ("error sending UDP for DNS: %s", error->message);
      dsk_error_unref (error);
      break;

    default:
      dsk_assert_not_reached ();
    }
}

static void
begin_dns_request (DskDnsCacheEntry *entry)
{
  /* pick dns server */
  DskDnsCacheEntryJob *job;
  unsigned dns_index = next_nameserver_index++;
  DskDnsMessage message;
  DskDnsQuestion question;
  if (next_nameserver_index == n_resolv_conf_ns)
    next_nameserver_index = 0;

  /* create job */
  entry->info.in_progress = job = dsk_malloc (sizeof (*job));
  job->owner = entry;
  job->ns_index = dns_index;
  job->waiters = NULL;
  job->attempt = 0;
  job->timer = NULL;
  job->message = NULL;
  job->waiting_to_send = DSK_FALSE;

  /* send dns question to nameserver */
  memset (&message, 0, sizeof (message));
  message.n_questions = 1;
  message.questions = &question;
  message.id = 1;
  message.is_query = 1;
  message.recursion_desired = 1;
  message.opcode = DSK_DNS_OP_QUERY;
  question.name = entry->name;
  question.query_type = entry->is_ipv6 ? DSK_DNS_RR_HOST_ADDRESS_IPV6 : DSK_DNS_RR_HOST_ADDRESS;
  question.query_class = DSK_DNS_CLASS_IN;
  DskError *error = NULL;
  job->message = dsk_dns_message_serialize (&message, &job->message_len);
  job->timer = dsk_dispatch_add_timer_millis (NULL, retry_schedule[0],
                                              handle_timer_expired,
                                              job);
  switch (job_send_message (job, &error))
    {
    case DSK_IO_RESULT_SUCCESS:

      /* make timeout timer */
      break;
    case DSK_IO_RESULT_AGAIN:
      raise_waiting_to_send_flag (job);
      break;
    case DSK_IO_RESULT_EOF:
      dsk_assert_not_reached ();
      break;
    case DSK_IO_RESULT_ERROR:
      dsk_warning ("error sending UDP for DNS: %s", error->message);
      dsk_error_unref (error);

      /* Proceed to create fail timer; we will handle this like a timeout */
      break;
    default:
      dsk_assert_not_reached ();
    }
}


static void
lookup_without_searchpath (const char       *normalized_name,
                           dsk_boolean       is_ipv6,
                           DskDnsCacheEntryFunc callback,
                           void             *callback_data)
{
  DskDnsCacheEntry ce;
  DskDnsCacheEntry *entry;
  DskError *error = NULL;
  ce.name = (char*) normalized_name;
  ce.is_ipv6 = is_ipv6;

  /* initialize */
  if (!dns_initialized && !dsk_dns_try_init (&error))
    {
      DskDnsCacheEntry entry;
      entry.name = (char*) normalized_name;
      entry.is_ipv6 = is_ipv6;
      entry.expire_time = 0;
      entry.type = DSK_DNS_CACHE_ENTRY_ERROR;
      entry.info.error = error;
      callback (&entry, callback_data);
      dsk_warning ("error initializing dns subsystem: %s", error->message);
      dsk_error_unref (error);
      return;
    }

  /* lookup in /etc/hosts if enabled */
  if (flags & DSK_DNS_CONFIG_USE_ETC_HOSTS)
    {
      GSK_RBTREE_LOOKUP (GET_ETC_HOSTS_TREE (), &ce, entry);
      if (entry != NULL)
        {
          callback (entry, callback_data);
          return;
        }
    }

  /* lookup in cache */
  GSK_RBTREE_LOOKUP (GET_CACHE_BY_NAME_TREE (), &ce, entry);

  /* --- do actual dns request --- */

  /* create new IN_PROGRESS entry */
  if (entry == NULL)
    {
      entry = dsk_malloc (sizeof (DskDnsCacheEntry) + strlen (name) + 1);
      entry->name = strcpy ((char*)(entry+1), name);
      entry->is_ipv6 = is_ipv6;
      entry->expire_time = NO_EXPIRE_TIME;
      entry->type = DSK_DNS_CACHE_ENTRY_IN_PROGRESS;
      job = dsk_malloc (sizeof (DskDnsCacheEntryJob));
      entry->info.in_progress = 
      DSK_RBTREE_INSERT (GET_CACHE_BY_NAME_TREE (), entry, conflict);
      dsk_assert (conflict == NULL);
      job->watch_list = NULL;
      ...

      /* expunge old cache entries */
      ...

      begin_dns_request (entry);
    }
  if (entry->type == DSK_DNS_CACHE_ENTRY_IN_PROGRESS)
    {
      /* This happens in an existing pending DNS lookup,
         as well as when a new CacheEntry is created. */

      /* add to watch list */
      watch = dsk_mem_pool_fixed_alloc (&watch_mempool);
      watch->callback = callback;
      watch->callback_data = callback_data;
      watch->next = job->watch_list;
      job->watch_list = watch_list;
    }
  else
    {
      (*callback) (entry, callback_data);
      return;
    }

}


/* NOTE: we call with 'name' taken from another cache entry when resolving cnames.
   SO this must copy the string BEFORE it ousts anything from its cache. */
void
dsk_dns_lookup_cache_entry (const char       *name,
                            dsk_boolean       is_ipv6,
                            DskDnsCacheEntryFunc callback,
                            void             *callback_data)
{
  DskDnsCacheEntry ce;
  DskDnsCacheEntry *entry;
  DskDnsConfigFlags flags = config_flags;
  DskError *error = NULL;
  char normalized_name[DSK_DNS_MAX_NAMELEN];
  const char *in = name;
  char *out = normalized_name;
  dsk_boolean last_was_dot = DSK_TRUE;          /* to inhibit initial '.'s */
  dsk_boolean ends_with_dot = DSK_FALSE;

  /* normalize name */
  while (*in)
    {
      if (*in == '.')
        {
          if (!last_was_dot)
            {
              *out++ = '.';
              if (out == normalized_name + DSK_DNS_MAX_NAMELEN)
                goto name_too_long;
              last_was_dot = DSK_TRUE;
            }
        }
      else
        {
          last_was_dot = DSK_FALSE;
          if (('0' <= *in && *in <= '9')
            || ('a' <= *in && *in <= 'z')
            || *in == '-' || *in == '_')
            *out++ = *in;
          else if ('A' <= *in && *in <= 'Z')
            *out++ = *in + ('a' - 'A');
          else
            {
              ce.name = (char*)name;
              ce.is_ipv6 = is_ipv6;
              ce.type = DSK_DNS_CACHE_ENTRY_BAD_RESPONSE;
              ce.info.bad_response.message = (char*)"illegal char in domain-name";
              callback (&ce, callback_data);
              return;
            }
          if (out == normalized_name + DSK_DNS_MAX_NAMELEN)
            goto name_too_long;
        }
      in++;
    }
  *out = 0;
  if (normalized_name[0] == 0)
    {
      ce.name = (char*)name;
      ce.is_ipv6 = is_ipv6;
      ce.type = DSK_DNS_CACHE_ENTRY_BAD_RESPONSE;
      ce.info.bad_response.message = (char*)"empty domain name cannot be looked up";
      callback (&ce, callback_data);
      return;
    }
  if (last_was_dot)
    {
      --out;
      *out = 0;
      ends_with_dot = DSK_TRUE;
    }
  /* ensure dns system is ready */
  if (!dns_initialized && !dsk_dns_try_init (error))
    {
      ce.name = (char*) name;
      ce.is_ipv6 = is_ipv6;
      ce.expire_time = NO_EXPIRE_TIME;
      ce.type = DSK_DNS_CACHE_ENTRY_BAD_RESPONSE;
      ce.info.bad_response.message = error->message;
      callback (&ce, callback_data);
      dsk_error_unref (error);
      return;
    }

  if (ends_with_dot)
    {
      lookup_without_searchpath (normalized_name, is_ipv6, callback, callback_data);
      return;
    }
  else
    {
      /* iterate through searchpath, eventually trying "no search path" */
      ...
    }
}
