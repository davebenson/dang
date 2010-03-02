

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
                        dsk_boolean       is_ipv6,
                        DskDnsLookupFunc  callback,
                        void             *callback_data);


/* non-blocking lookups only hit the cache */
typedef enum
{
  DSK_DNS_LOOKUP_NONBLOCKING_NOT_FOUND,
  DSK_DNS_LOOKUP_NONBLOCKING_MUST_BLOCK,
  DSK_DNS_LOOKUP_NONBLOCKING_FOUND,
  DSK_DNS_LOOKUP_NONBLOCKING_ERROR
} DskDnsLookupNonblockingResult;

DskDnsLookupNonblockingResult
       dsk_dns_lookup_nonblocking (const char *name,
                                   DskDnsAddress *out,
                                   dsk_boolean    is_ipv6,
                                   DskError     **error);



typedef enum
{
  DSK_DNS_CACHE_ENTRY_IN_PROGRESS,
  DSK_DNS_CACHE_ENTRY_BAD_RESPONSE,
  DSK_DNS_CACHE_ENTRY_NEGATIVE,
  DSK_DNS_CACHE_ENTRY_CNAME,
  DSK_DNS_CACHE_ENTRY_ADDR,
} DskDnsCacheEntryType;

typedef struct _DskDnsCacheEntryJob DskDnsCacheEntryJob;
typedef struct _DskDnsCacheEntry DskDnsCacheEntry;
struct _DskDnsCacheEntry
{
  char *name;
  dsk_boolean is_ipv6;
  unsigned expire_time;
  DskDnsCacheEntryType type;
  union {
    DskDnsCacheEntryJob *in_progress;
    struct { char *message; } bad_response;
    char *cname;
    struct { unsigned n; DskDnsAddress *addresses; unsigned last_used; } addr;
  } info;

  DskDnsCacheEntry *expire_left, *expire_right, *expire_parent;
  dsk_boolean expire_is_red;
  DskDnsCacheEntry *name_type_left, *name_type_right, *name_type_parent;
  dsk_boolean name_type_is_red;
};
typedef void (*DskDnsCacheEntryFunc) (DskDnsCacheEntry *entry,
                                      void             *callback_data);
void              dsk_dns_lookup_cache_entry (const char       *name,
                                              dsk_boolean       is_ipv6,
                                              DskDnsCacheEntryFunc callback,
                                              void             *callback_data);


typedef enum
{
  DSK_DNS_CONFIG_USE_RESOLV_CONF_SEARCHPATH = (1<<0),
  DSK_DNS_CONFIG_USE_RESOLV_CONF_NS = (1<<1),
  DSK_DNS_CONFIG_USE_ETC_HOSTS = (1<<2)
} DskDnsConfigFlags;
#define DSK_DNS_CONFIG_FLAGS_DEFAULT \
  (DSK_DNS_CONFIG_USE_RESOLV_CONF_SEARCHPATH| \
   DSK_DNS_CONFIG_USE_RESOLV_CONF_NS| \
   DSK_DNS_CONFIG_USE_ETC_HOSTS)

/* --- interfacing with system-level sockaddr structures --- */
/* 'out' should be a pointer to a 'struct sockaddr_storage'.
 */
void dsk_dns_address_to_sockaddr (DskDnsAddress *address,
                                  unsigned       port,
                                  void          *out,
                                  unsigned      *out_len);
dsk_boolean dsk_sockaddr_to_dns_address (unsigned addr_len,
                                         const void *addr,
                                         DskDnsAddress *out,
                                         unsigned      *port_out);
