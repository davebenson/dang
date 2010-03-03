
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
  
dsk_boolean dsk_hostname_looks_numeric (const char *str);
dsk_boolean dsk_ip_address_parse_numeric (const char *str,
                                           DskIpAddress *out);
char *dsk_ip_address_to_string (const DskIpAddress *);

