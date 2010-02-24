
typedef enum
{
  DSK_DNS_SECTION_QUESTION      = 0x01,
  DSK_DNS_SECTION_ANSWER        = 0x02,
  DSK_DNS_SECTION_AUTHORITY     = 0x04,
  DSK_DNS_SECTION_ADDITIONAL    = 0x08,

  DSK_DNS_SECTION_ALL           = 0x0f
} DskDnsSectionCode;


typedef enum
{
  DSK_DNS_CLASS_IN      = 1,
  DSK_DNS_CLASS_ANY     = 255
} DskDnsClassCode;



/* DskDnsResourceRecordType: AKA RTYPE: 
 *       Types of `RR's or `ResourceRecord's (values match RFC 1035, 3.2.2)
 */
typedef enum
{
  /* An `A' record:  the ip address of a host. */
  DSK_DNS_RR_HOST_ADDRESS = 1,

  /* A `NS' record:  the authoritative name server for the domain */
  DSK_DNS_RR_NAME_SERVER = 2,

  /* A `CNAME' record:  indicate another name for an alias. */
  DSK_DNS_RR_CANONICAL_NAME = 5,

  /* A `HINFO' record: identifies the CPU and OS used by a host */
  DSK_DNS_RR_HOST_INFO = 13,

  /* A `MX' record */
  DSK_DNS_RR_MAIL_EXCHANGE = 15,

  /* A `PTR' record:  a pointer to another part of the domain name space */
  DSK_DNS_RR_POINTER = 12,

  /* A `SOA' record:  identifies the start of a zone of authority [???] */
  DSK_DNS_RR_START_OF_AUTHORITY = 6,

  /* A `TXT' record:  miscellaneous text */
  DSK_DNS_RR_TEXT = 16,

  /* A `WKS' record:  description of a well-known service */
  DSK_DNS_RR_WELL_KNOWN_SERVICE = 11,

  /* A `AAAA' record: for IPv6 (see RFC 1886) */
  DSK_DNS_RR_HOST_ADDRESS_IPV6 = 28,

  /* --- only allowed for queries --- */

  /* A `AXFR' record: `special zone transfer QTYPE' */
  DSK_DNS_RR_ZONE_TRANSFER = 252,

  /* A `MAILB' record: matches all mail box related RRs (e.g. MB and MG). */
  DSK_DNS_RR_ZONE_MAILB = 253,

  /* A `*' record:  matches anything. */
  DSK_DNS_RR_WILDCARD = 255

} GskDnsResourceRecordType;


typedef enum
{
  DSK_DNS_OP_QUERY  = 0,
  DSK_DNS_OP_IQUERY = 1,
  DSK_DNS_OP_STATUS = 2,
  DSK_DNS_OP_NOTIFY = 4,
  DSK_DNS_OP_UPDATE = 5,
} DskDnsOpcode;


typedef enum
{
  DSK_DNS_RCODE_NOERROR    = 0,
  DSK_DNS_RCODE_FORMERR    = 1,
  DSK_DNS_RCODE_SERVFAIL   = 2,
  DSK_DNS_RCODE_NXDOMAIN   = 3,
  DSK_DNS_RCODE_NOTIMP     = 4,
  DSK_DNS_RCODE_REFUSED    = 5,
  DSK_DNS_RCODE_YXDOMAIN   = 6,
  DSK_DNS_RCODE_YXRRSET    = 7,
  DSK_DNS_RCODE_NXRRSET    = 8,
  DSK_DNS_RCODE_NOTAUTH    = 9,
  DSK_DNS_RCODE_NOTZONE    = 10,
} DskDnsRcode;

struct _DskDnsResourceRecord
{
  DskDnsResourceRecordType  type;
  char                     *owner;     /* where the resource_record is found */
  uint32_t                  time_to_live;
  DskDnsClassCode           class_code;

  /* rdata: record type specific data */
  union
  {
    /* For DSK_DNS_RR_HOST_ADDRESS and DSK_DNS_CLASS_INTERNET */
    struct
    {
      guint8 ip_address[4];
    } a;

		/* unsupported */
    /* For DSK_DNS_RR_HOST_ADDRESS and DSK_DNS_CLASS_CHAOS */
    struct
    {
      char *chaos_name;
      guint16 chaos_address;
    } a_chaos;

    /* For DSK_DNS_RR_CNAME, DSK_DNS_RR_POINTER, DSK_DNS_RR_NAME_SERVER */
    char *domain_name;

    /* For DSK_DNS_RR_MAIL_EXCHANGE */
    struct
    {
      guint preference_value; /* "lower is better" */

      char *mail_exchange_host_name;
    } mx;

    /* For DSK_DNS_RR_TEXT */
    char *txt;

    /* For DSK_DNS_RR_HOST_INFO */
    struct
    {
      char *cpu;
      char *os;
    } hinfo;


    /* SOA: Start Of a zone of Authority.
     *
     * Comments cut-n-pasted from RFC 1035, 3.3.13.
     */
    struct
    {
      /* The domain-name of the name server that was the
	 original or primary source of data for this zone. */
      char *mname;

      /* specifies the mailbox of the
	 person responsible for this zone. */
      char *rname;

      /* The unsigned 32 bit version number of the original copy
	 of the zone.  Zone transfers preserve this value.  This
	 value wraps and should be compared using sequence space
	 arithmetic. */
      guint32 serial;

      /* A 32 bit time interval before the zone should be
	 refreshed. (cf 1034, 4.3.5) [in seconds] */
      guint32 refresh_time;

      /* A 32 bit time interval that should elapse before a
	 failed refresh should be retried. [in seconds] */
      guint32 retry_time;

      /* A 32 bit time value that specifies the upper limit on
	 the time interval that can elapse before the zone is no
	 longer authoritative. [in seconds] */
      guint32 expire_time;

      /* The unsigned 32 bit minimum TTL field that should be
	 exported with any RR from this zone. [in seconds] */
      guint32 minimum_time;
    } soa;

    struct {
      guint8 address[16];
    } aaaa;
  } rdata;
};


struct _DskDnsMessage
{
  unsigned n_questions;
  DskDnsQuestion *questions;
  unsigned n_answer_rr;
  DskDnsResourceRecord *answers;
  unsigned n_authority_rr;
  DskDnsResourceRecord *authority;
  unsigned n_additional_rr;
  DskDnsResourceRecord *additional;

  uint16_t id;     /* used by requestor to match queries and replies */

  /* Is this a query or a response? */
  uint16_t is_query : 1;

  uint16_t is_authoritative : 1;
  uint16_t is_truncated : 1;

  /* [Responses only] the `RA bit': whether the server is willing to provide
   *                                recursive services. (cf 1034, 4.3.1)
   */
  uint16_t recursion_available : 1;

  /* [Queries only] the `RD bit': whether the requester wants recursive
   *                              service for this queries. (cf 1034, 4.3.1)
   */
  uint16_t recursion_desired : 1;

};

DskDnsMessage *dsk_dns_message_parse     (unsigned       len,
                                          const uint8_t *data,
                                          DskError     **error);
uint8_t *      dsk_dns_message_serialize (DskDnsMessage *message,
                                          unsigned      *length_out);

