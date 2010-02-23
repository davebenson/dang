

typedef struct _DskDnsAddress DskDnsAddress;
typedef struct _DskDnsLookupResult DskDnsLookupResult;
typedef struct _DskDnsEntry DskDnsEntry;

typedef enum
{
  DSK_DNS_ADDRESS_IPV4,
  DSK_DNS_ADDRESS_IPV6
} DskDnsAddressType;

struct _DskDnsAddress
{
  DskDnsAddressType type;
  uint8_t address[16];          /* enough for ipv4 or ipv6 */
};
  

typedef enum
{
  DSK_DNS_LOOKUP_RESULT_FOUND,
  DSK_DNS_LOOKUP_RESULT_NOT_FOUND,
  DSK_DNS_LOOKUP_RESULT_TIMEOUT,
  DSK_DNS_LOOKUP_RESULT_BAD_RESPONSE
} DskDnsLookupResultType;

struct _DskDnsLookupResult
{
  DskDnsLookupResultType type;
  DskDnsAddress *addr;          /* if found */
  const char *message;          /* for all other types */
};

typedef void (*DskDnsLookupFunc) (DskDnsLookupResult *result,
                                  void               *callback_data);

void    dsk_dns_lookup (const char       *name,
                        DskDnsLookupFunc  callback,
                        void             *callback_data);

typedef enum
{
  DSK_DNS_CACHE_ENTRY_IN_PROGRESS,
  DSK_DNS_CACHE_ENTRY_BAD_RESPONSE,
  DSK_DNS_CACHE_ENTRY_NEGATIVE,
  DSK_DNS_CACHE_ENTRY_CNAME,
  DSK_DNS_CACHE_ENTRY_ADDR,
} DskDnsCacheEntryType;

struct _DskDnsCacheEntry
{
  unsigned expire_time;
  DskDnsCacheEntryType type;
  union {
    DskDnsCacheEntryJob *in_progress;
    struct { char *message; } bad_response;
    char *cname;
    struct { unsigned n; DskDnsAddress *addresses; } addr;
  } info;

  DskDnsCacheEntry *expire_left, *expire_right, *expire_parent;
  dsk_boolean is_red;
};
typedef void (*DskDnsCacheEntryFunc) (DskDnsCacheEntry *entry,
                                      void             *callback_data);
DskDnsCacheEntry *dsk_dns_lookup_cache_entry (const char       *name,
                                              DskDnsCacheEntryFunc callback,
                                              void             *callback_data);
