
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
        dsk_dns_lookup_cache_entry (cname, handle_cache_entry_lookup, data);
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
                        DskDnsLookupFunc  callback,
                        void             *callback_data)
{
  LookupData *lookup_data = dsk_new (LookupData, 1);
  lookup_data->n_cnames = 0;
  lookup_data->cnames = NULL;
  lookup_data->callback = callback;
  lookup_data->callback_data = callback_data;
  dsk_dns_lookup_cache_entry (name, handle_cache_entry_lookup, lookup_data);
} 

/* --- configuration --- */
static unsigned n_resolv_conf_ns = 0;
static DskIpAddress *resolv_conf_ns = NULL;
static unsigned n_resolv_conf_search_paths = 0;
static char **resolv_conf_search_paths = NULL;
static DskDnsCacheEntry *etc_hosts_tree = NULL;
static DskDnsConfigFlags config_flags = DSK_DNS_CONFIG_FLAGS_DEFAULT;
static dsk_boolean dns_initialized = DSK_FALSE;

/* --- handling system files (resolv.conf and hosts) --- */

static dsk_boolean
dsk_dns_try_init (DskError **error)
{
  /* parse /etc/hosts */
  ...

  /* parse /etc/resolv.conf */
  ...
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
