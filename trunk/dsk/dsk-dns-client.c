#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "dsk.h"
#include "../gskrbtreemacros.h"

#define MAX_CNAMES              16

typedef struct _LookupData LookupData;
struct _LookupData
{
  unsigned n_cnames;
  char **cnames;
  DskDnsLookupFunc callback;
  void *callback_data;
};


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
        char *cname;
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
        cname = dsk_strdup (entry->info.cname);
        dsk_dns_lookup_cache_entry (cname, entry->is_ipv6, handle_cache_entry_lookup, data);
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

/* --- configuration --- */
static unsigned n_resolv_conf_ns = 0;
static DskIpAddress *resolv_conf_ns = NULL;
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
      *at = 0;
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
      host_entry->expire_time = (unsigned)(-1);
      host_entry->type = DSK_DNS_CACHE_ENTRY_ADDR;
      host_entry->info.addr.n = 1;
      host_entry->info.addr.last_used = 0;
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
  while (fgets (buf, sizeof (buf), fp) != NULL)
    {
      const char *at = buf;
      while (*at && dsk_ascii_isspace (*at))
        at++;
      if (*at == '#')
        continue;
      ...
    }
  fclose (fp);

  /* XXX: make UDP connection to nameserver? */
  ...

  return DSK_TRUE;
}

/* --- low-level ---*/
DskDnsLookupNonblockingResult
dsk_dns_lookup_nonblocking (const char *name,
                           DskIpAddress *out,
                           dsk_boolean    is_ipv6,
                           DskError     **error)
{
  unsigned n_cnames = 0;
  do
    {
      DskDnsCacheEntry ce;
      DskDnsCacheEntry *entry;
      ce.name = (char*) name;
      ce.is_ipv6 = is_ipv6 ? 1 : 0;
      GSK_RBTREE_LOOKUP (GET_CACHE_BY_NAME_TREE (), &ce, &entry);
      if (entry == NULL)
        return DSK_DNS_LOOKUP_NONBLOCKING_MUST_BLOCK;
      if (entry->type)
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

void
dsk_dns_lookup_cache_entry (const char       *name,
                            dsk_boolean       is_ipv6,
                            DskDnsCacheEntryFunc callback,
                            void             *callback_data)
{
  DskDnsCacheEntry ce;
  DskDnsCacheEntry *entry;

  /* initialize */
  ...

  /* lookup in /etc/hosts if enabled */
  ...

  ce.name = (char*) name;
  ce.is_ipv6 = is_ipv6 ? 1 : 0;
  GSK_RBTREE_LOOKUP (GET_CACHE_BY_NAME_TREE (), &ce, &entry);
  if (entry == NULL)
    {
      /* create new IN_PROGRESS entry */
      entry = dsk_malloc (sizeof (DskDnsCacheEntry) + strlen (name) + 1);
      entry->name = strcpy ((char*)(entry+1), name);
      entry->is_ipv6 = is_ipv6;
      entry->expire_time = ...;
      entry->type = DSK_DNS_CACHE_ENTRY_IN_PROGRESS;
      job = dsk_malloc (sizeof (DskDnsCacheEntryJob));
      entry->info.in_progress = 
      DSK_RBTREE_INSERT (GET_CACHE_BY_NAME_TREE (), entry, conflict);
      dsk_assert (conflict == NULL);
      job->watch_list = NULL;
      ...


    }
  if (entry->type == DSK_DNS_CACHE_ENTRY_IN_PROGRESS)
    {
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
    }
}
