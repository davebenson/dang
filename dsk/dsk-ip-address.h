
typedef struct _DskIpAddress DskIpAddress;
typedef enum
{
  DSK_IP_ADDRESS_IPV4,
  DSK_IP_ADDRESS_IPV6
} DskIpAddressType;

struct _DskIpAddress
{
  DskIpAddressType type;
  uint8_t address[16];          /* enough for ipv4 or ipv6 */
};
#define DSK_IP_ADDRESS_DEFAULT {0,{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}
  
dsk_boolean dsk_hostname_looks_numeric (const char *str);
dsk_boolean dsk_ip_address_parse_numeric (const char *str,
                                           DskIpAddress *out);
char *dsk_ip_address_to_string (const DskIpAddress *);

dsk_boolean dsk_ip_addresses_equal (const DskIpAddress *a,
                                    const DskIpAddress *b);


/* --- interfacing with system-level sockaddr structures --- */
/* 'out' should be a pointer to a 'struct sockaddr_storage'.
 */
void dsk_ip_address_to_sockaddr (const DskIpAddress *address,
                                  unsigned       port,
                                  void          *out,
                                  unsigned      *out_len);
dsk_boolean dsk_sockaddr_to_ip_address (unsigned addr_len,
                                         const void *addr,
                                         DskIpAddress *out,
                                         unsigned      *port_out);
