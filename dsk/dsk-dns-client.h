

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
  
struct _DskDnsEntry
{
  unsigned timeout;
  unsigned n_addresses;         /* if 0, then this indicates a negative entry */
  DskDnsAddress *addresses;
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
  DskDnsEntry *entry;           /* for FOUND/NOT_FOUND */
  const char *message;          /* for all other types */
};

typedef void (*DskDnsLookupFunc) (DskDnsLookupResult *result,
                                  void               *callback_data);

void    dsk_dns_lookup (const char       *name,
                        DskDnsLookupFunc  callback,
                        void             *callback_data);

