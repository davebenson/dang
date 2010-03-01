
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
static DskDnsAddress *resolv_conf_ns = NULL;
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
                           DskDnsAddress *out,
                           dsk_boolean    is_ipv6,
                           DskError     **error);

void              dsk_dns_lookup_cache_entry (const char       *name,
                                              dsk_boolean       is_ipv6,
                                              DskDnsCacheEntryFunc callback,
                                              void             *callback_data);
