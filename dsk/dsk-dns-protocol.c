#include <stdlib.h>
#include <string.h>
#include "dsk-common.h"
#include "dsk-object.h"
#include "dsk-error.h"
#include "dsk-dns-protocol.h"

#define N_INIT_STR_INFO         64

/* Used for binary packing/unpacking */
typedef struct _DskDnsHeader DskDnsHeader;
struct _DskDnsHeader
{
  unsigned qid:16;

#if BYTE_ORDER == BIG_ENDIAN
  unsigned qr:1;
  unsigned opcode:4;
  unsigned aa:1;
  unsigned tc:1;
  unsigned rd:1;

  unsigned ra:1;
  unsigned unused:3;
  unsigned rcode:4;
#else
  unsigned rd:1;
  unsigned tc:1;
  unsigned aa:1;
  unsigned opcode:4;
  unsigned qr:1;

  unsigned rcode:4;
  unsigned unused:3;
  unsigned ra:1;
#endif

  unsigned qdcount:16;
  unsigned ancount:16;
  unsigned nscount:16;
  unsigned arcount:16;
};


/* === parsing a binary message === */
typedef struct _StrInfo StrInfo;
struct _StrInfo
{
  uint16_t offset;
  uint16_t length;
  uint16_t flags;
};
typedef struct _UsedStr UsedStr;
struct _UsedStr
{
  unsigned offset;
  char *str;
};
typedef enum
{
  STR_INFO_FLAG_USED = 1,
  STR_INFO_FLAG_COMPUTING_LENGTH = 2,
  STR_INFO_FLAG_HAS_LENGTH = 4
} StrInfoFlags;

static int compare_str_info (const void *a, const void *b)
{
  const StrInfo *A = a;
  const StrInfo *B = b;
  if (A->offset < B->offset)
    return -1;
  else if (A->offset > B->offset)
    return 1;
  else
    return 0;
}


static dsk_boolean
compute_length (unsigned n_str_info,
                StrInfo *str_info,
                const uint8_t *data,
                unsigned i,
                const char **msg)
{
  const uint8_t *at;
  unsigned len;
  StrInfo *dst;
  str_info[i].flags |= STR_INFO_FLAG_COMPUTING_LENGTH;
  at = data + str_info[i].offset;
  len = 0;
  while (*at != 0 && (*at & 0xc0) == 0)
    {
      unsigned L = *at;
      len += L + 1;
      at += L + 1;
    }
  if (*at != 0)
    {
      StrInfo dummy;
      if ((*at & 0xc0) != 0xc0)
        {
          *msg = "bad pointer";
          return DSK_FALSE;
        }
      dummy.offset = ((*at - 0xc0) << 8) + at[1];

      /* bsearch new offset */
      dst = bsearch (&dummy, str_info, n_str_info, sizeof (StrInfo),
                     compare_str_info);
      if (dst == NULL)
        {
          *msg = "invalid offset";
          return DSK_FALSE;
        }
      if ((dst->flags & STR_INFO_FLAG_HAS_LENGTH) == 0)
        {
          if (dst->flags & STR_INFO_FLAG_COMPUTING_LENGTH)
            {
              *msg = "circular name compression";
              return DSK_FALSE;
            }
          if (!compute_length (n_str_info, str_info, 
                               data, dst - str_info,
                               msg))
            return DSK_FALSE;
        }

      len += str_info[i].length + 1;
    }

  str_info[i].length = len ? (len - 1) : 0;
  str_info[i].flags &= ~STR_INFO_FLAG_COMPUTING_LENGTH;
  str_info[i].flags |= STR_INFO_FLAG_HAS_LENGTH;
  return DSK_TRUE;
}

static dsk_boolean
gather_names (unsigned       len,
              const uint8_t *data,
              unsigned      *used_inout, 
              unsigned      *n_str_info_inout,
              StrInfo      **str_info_inout,
              unsigned      *str_info_alloced_inout,
              DskError     **error)
{
  unsigned used = *used_inout;
  unsigned n_str_info = *n_str_info_inout;
  StrInfo *str_info = *str_info_inout;
  unsigned str_info_alloced = *str_info_alloced_inout;
  StrInfoFlags flags = STR_INFO_FLAG_USED;
  if (used == len)
    {
      dsk_set_error (error, "truncated at beginning of name");
      return DSK_FALSE;
    }
  while (data[used] != 0 && (data[used] & 0xc0) == 0)
    {
      if (used + data[used] + 1 > len)
        {
          dsk_set_error (error, "string of length %u truncated", data[used]);
          return DSK_FALSE;
        }
      if (n_str_info == str_info_alloced)
        {
          unsigned old_size = n_str_info * sizeof (StrInfo);
          if (str_info_alloced == N_INIT_STR_INFO)
            {
              StrInfo *n = dsk_malloc (old_size * 2);
              memcpy (n, str_info, old_size);
              str_info = n;
            }
          else
            str_info = dsk_realloc (str_info, old_size * 2);
          str_info_alloced *= 2;
        }
      str_info[n_str_info].offset = used;
      str_info[n_str_info].flags = flags;
      flags = 0;

      unsigned adv = data[used] + 1;
      used += adv;
      if (used == len)
        {
          dsk_set_error (error, "truncated at end-marker of string");
          return DSK_FALSE;
        }
    }
  if (data[used] == 0)
    used++;
  else if ((data[used] & 0xc0) != 0xc0)
    {
      dsk_set_error (error, "bad name encoding; got top two bits %u%u",
                     (data[used]>>7), (data[used]>>6)&1);
      return DSK_FALSE;
    }
  else
    {
      if (used == 1)
        {
          dsk_set_error (error, "truncated in middle of pointer");
          return DSK_FALSE;
        }
      used += 2;
    }
  *used_inout = used;
  *n_str_info_inout = n_str_info;
  *str_info_inout = str_info;
  *str_info_alloced_inout = str_info_alloced;
  return DSK_TRUE;
}


static dsk_boolean
gather_names_resource_record (unsigned       len,
                              const uint8_t *data,
                              unsigned      *used_inout, 
                              unsigned      *n_str_info_inout,
                              StrInfo      **str_info_inout,
                              unsigned      *str_info_alloced_inout,
                              DskError     **error)
{
  const char *code;
  uint8_t header[10];
  /* owner */
  if (!gather_names (len, data, used_inout,
                     n_str_info_inout, str_info_inout, str_info_alloced_inout,
                     error))
    return DSK_FALSE;
  if (*used_inout + 10 > len)
    {
      dsk_set_error (error, "truncated resource-record");
      return DSK_FALSE;
    }
  memcpy (header, data + *used_inout, 10);
  *used_inout += 10;
  DskDnsResourceRecordType type;
  unsigned rdlength;
  type = ((uint16_t)header[0] << 8) | ((uint16_t)header[1] << 0);
  rdlength = ((uint16_t)header[8] << 8)  | ((uint16_t)header[9] << 0);
  if (*used_inout + rdlength > len)
    {
      dsk_set_error (error, "truncated resource-data");
      return DSK_FALSE;
    }
  const uint8_t *rddata;
  rddata = data + *used_inout;
  switch (type)
    {
    case DSK_DNS_RR_HOST_ADDRESS: code = "d"; break;
    case DSK_DNS_RR_HOST_ADDRESS_IPV6: code = "dddd"; break;
    case DSK_DNS_RR_NAME_SERVER: code = "n"; break;
    case DSK_DNS_RR_CANONICAL_NAME: code = "n"; break;
    case DSK_DNS_RR_POINTER: code = "n"; break;
    case DSK_DNS_RR_MAIL_EXCHANGE: code = "bbn"; break;
    case DSK_DNS_RR_HOST_INFO: code = "ss"; break;
    case DSK_DNS_RR_START_OF_AUTHORITY: "nnddddd"; break;
    case DSK_DNS_RR_TEXT: code = "ss"; break;
    case DSK_DNS_RR_WILDCARD: code = ""; break;
    case DSK_DNS_RR_WELL_KNOWN_SERVICE:
    case DSK_DNS_RR_ZONE_TRANSFER:
    case DSK_DNS_RR_ZONE_MAILB:
      dsk_set_error ("unimplemented resource-record type %u", type);
      return DSK_FALSE;
    default:
      dsk_set_error ("unknown resource-record type %u", type);
      return DSK_FALSE;
    }

  unsigned init_used;
  init_used = *used_inout;
  while (*code)
    {
      switch (*code)
        {
        case 'b':
          if (*used_inout == len)
            goto truncated;
          *used_inout += 1;
        case 'd':
          if (*used_inout + 4 > len)
            goto truncated;
          *used_inout += 4;
          break;
        case 's':
          {
            unsigned c;
            if (*used_inout == len)
              goto truncated;
            c = data[*used_inout];
            if (c + 1 + *used_inout > len)
              goto truncated;
            *used_inout += c + 1;
            break;
          }
        case 'n':
          if (!gather_names (len, data, used_inout,
                             n_str_info_inout,
                             str_info_inout,
                             str_info_alloced_inout,
                             error))
            return DSK_FALSE;
          break;
        default:
          dsk_assert_not_reached ();
        }
      code++;
    }
  if (init_used + rdlength != *used_inout)
    {
      dsk_set_error (error, "mismatch between parsed size %u and stated rdlength %u", 
                     *used_inout - init_used, rdlength);
      return DSK_FALSE;
    }
  return DSK_TRUE;
truncated:
  dsk_set_error (error, "data truncated in resource-record of type %u", type);
  return DSK_FALSE;
}

static char *
parse_domain_name     (unsigned              len,
                       const uint8_t        *data,
                       unsigned             *used_inout,
                       unsigned              n_used_strs,
                       UsedStr              *used_strs,
                       char                **extra_str_space_inout,
                       DskError            **error)
{
  ...
}

static dsk_boolean
parse_question (unsigned          len,
                const uint8_t    *data,
                unsigned         *used_inout,
                DskDnsQuestion   *question,
                unsigned          n_used_strs,
                UsedStr          *used_strs,
                DskError        **error)
{
  const char *name;
  uint16_t array[2];
  name = parse_domain_name (len, data, used_inout, n_used_strs, used_strs, error);
  if (*used_inout + 4 > len)
    {
      dsk_set_error (error, "data truncated in question");
      return DSK_FALSE;
    }
  memcpy (array, data + *used_inout, 4);
  *used_inout += 4;
  question->name = name;
  question->type = htons (qarray[0]);
  question->question_class = htons (qarray[0]);
  return DSK_TRUE;
}

static dsk_boolean
parse_resource_record (unsigned              len,
                       const uint8_t        *data,
                       unsigned             *used_inout,
                       DskDnsResourceRecord *rr,
                       unsigned              n_used_strs,
                       UsedStr              *used_strs,
                       char                **extra_str_space_inout,
                       DskError            **error)
{
  const char *name;
  uint8_t header[10];
  uint16_t type;
  uint16_t class;
  uint32_t ttl;
  uint16_t rdlength;
  rr->name = parse_domain_name (len, data, used_inout, n_used_strs, used_strs);
  if (*used_inout + 10 > len)
    {
      dsk_set_error (error, "data truncated in resource-record header");
      return DSK_FALSE;
    }
  memcpy (header, data + *used_inout, 10);
  *used_inout += 10;
  type     = ((uint16_t)header[0] << 8)  | ((uint16_t)header[1] << 0);
  class    = ((uint16_t)header[2] << 8)  | ((uint16_t)header[3] << 0);
  ttl      = ((uint32_t)header[4] << 24) | ((uint32_t)header[5] << 16)
           | ((uint32_t)header[6] << 8)  | ((uint32_t)header[7] << 0);
  rdlength = ((uint16_t)header[8] << 8)  | ((uint16_t)header[9] << 0);
  rr->type = type;
  rr->class_code = class;
  rr->time_to_live = ttl;
  switch (type)
    {
    case DSK_DNS_RR_HOST_ADDRESS:
      memcpy (rr->rdata.a.ip_address, data + *used_inout, 4);
      *used_inout += 4;
      break;
    case DSK_DNS_RR_HOST_ADDRESS_IPV6:
      memcpy (rr->rdata.aaaa.address, data + *used_inout, 16);
      *used_inout += 16;
      break;
    case DSK_DNS_RR_NAME_SERVER:
    case DSK_DNS_RR_CANONICAL_NAME:
    case DSK_DNS_RR_POINTER:
      rr->rdata.domain_name = parse_domain_name (len, data, used_inout, n_used_strs, used_strs);
      break;
    case DSK_DNS_RR_HOST_INFO:
      rr->rdata.hinfo.cpu = parse_length_prefixed_string (len, data, used_inout, extra_str_space_inout);
      rr->rdata.hinfo.os = parse_length_prefixed_string (len, data, used_inout, extra_str_space_inout);
      break;
    case DSK_DNS_RR_MAIL_EXCHANGE:
      {
        uint16_t pv;
        memcpy (&pv, data + *used_inout, 2);
        rr->rdata.mx.preference_value = htons (pv);
        *used_inout += 2;
      }
      rr->rdata.mx.mail_exchange_host_name = parse_domain_name (len, data, used_inout, n_used_strs, used_strs);
      break;
    case DSK_DNS_RR_START_OF_AUTHORITY:
      rr->rdata.soa.mname = parse_domain_name (len, data, used_inout, n_used_strs, used_strs);
      rr->rdata.soa.rname = parse_domain_name (len, data, used_inout, n_used_strs, used_strs);
      {
        uint32_t intervals[5];
        memcpy (intervals, data + *used_inout, 20);
	rr->rdata.soa.serial = htonl (intervals[0]);
	rr->rdata.soa.refresh_time = htonl (intervals[1]);
	rr->rdata.soa.retry_time = htonl (intervals[2]);
	rr->rdata.soa.expire_time = htonl (intervals[3]);
	rr->rdata.soa.minimum_time = htonl (intervals[4]);
        *used_inout += 20;
      }
      break;
    case DSK_DNS_RR_TEXT:
      rr->rdata.text = parse_length_prefixed_string (len, data, used_inout, extra_str_space_inout);
      break;
    default:
      dsk_set_error (error, "invalid type %u of resource-record", type);
      return DSK_FALSE;
    }
  return DSK_TRUE;
}

DskDnsMessage *
dsk_dns_message_parse (unsigned       len,
                       const uint8_t *data,
                       DskError     **error);
{
  DskDnsHeader header;
  if (len < 12)
    {
      dsk_set_error (error, "dns packet too short (<12 bytes)");
      return NULL;
    }

  dsk_assert (sizeof (header) == 12);
  memcpy (&header, data, 12);

#if BYTE_ORDER == LITTLE_ENDIAN
  header.qid = htons (header.qid);
  header.qdcount = htons (header.qdcount);
  header.ancount = htons (header.ancount);
  header.nscount = htons (header.nscount);
  header.arcount = htons (header.arcount);
#endif
  if (!validate_rcode (header.rcode))
    {
      dsk_set_error (error, "dns message had invalid 'rcode'");
      return NULL;
    }
  if (!validate_opcode (header.opcode))
    {
      dsk_set_error (error, "dns message had invalid 'opcode'");
      return NULL;
    }

  /* obtain a list of offsets to strings.
     each name may lead to N offsets,
     but that list should be exhaustive and unique,
     b/c strings can only appear in places we recognize.
     distinguish the offset that are used from those that aren't. */
  str_info = alloca (sizeof (StrInfo) * N_INIT_STR_INFO);
  n_str_info = 0;
  str_info_alloced = N_INIT_STR_INFO;
  used = 12;
  for (i = 0; i < header.qdcount; i++)
    {
      if (!gather_names (len, data, &used, &n_str_info, &str_info,
                         &str_info_alloced, error))
        goto cleanup;
      used += 4;
    }
  total_rr = header.ancount + header.nscount + header.arcount;
  for (i = 0; i < total_rr; i++)
    {
      if (!gather_names_resource_record (len, data,
                                         &used, &n_str_info, &str_info,
                                         &str_info_alloced, &extra_space,
                                         error))
        goto cleanup;
    }

  /* compute the length of each offset, detecting cycles. */
  if (n_str_info > 0)
    {
      unsigned o;
      qsort (str_info, n_str_info, sizeof (StrInfo), compare_str_info);
      for (i = 1, o = 0;
           i < n_str_info;
           i++)
        {
          if (str_info[o].offset == str_info[i].offset)
            {
              str_info[o].flags |= str_info[i].flags;
            }
          else
            {
              str_info[++o] = str_info[i];
            }
        }
      n_str_info = o + 1;
      for (i = 0; i < n_str_info; i++)
        if ((str_info[i].flags & STR_INFO_FLAG_HAS_LENGTH) == 0
         && !compute_length (n_str_info, str_info, data, i, &msg))
          {
            dsk_set_error (error, "error decompressing name: %s", msg);
            goto cleanup;
          }
    }

  /* figure the length of string-space needed */
  for (i = 0; i < n_str_info; i++)
    if (str_info[i].flags & STR_INFO_FLAG_USED)
      {
        str_space += str_info[i].length + 1;
        n_used_strs++;
      }

  /* allocate space for the message */
  message = dsk_malloc (sizeof (DskDnsMessage)
                        + sizeof (DskDnsQuestion) * header.qdcount
                        + sizeof (DskDnsResourceRecord) * total_rr
                        + str_space);
  message->id = header.qid;
  message->is_query = header.qr ^ 1;
  message->is_authoritative = header.aa;
  message->recursion_desired = header.rd;
  message->recursion_available = header.ra;
  message->n_questions = header.qdcount;
  message->questions = (DskDnsQuestion *) (message + 1);
  message->n_answer_rr = header.ancount;
  message->answer_rr = (DskDnsResourceRecord *) (message->questions + message->n_questions);
  message->n_authority_rr = header.nscount;
  message->authority_rr = message->answer_rr + message->n_answer_rr;
  message->n_additional_rr = header.arcount;
  message->additional_rr = message->authority_rr + message->n_authority_rr;
  message->rcode = header.rcode;
  message->opcode = header.opcode;

  str_heap = (char*) (message->additional_rr + message->n_additional_rr);

  /* reserve space for the 4 sections; then build the needed
     strings, making a map 'offset' to 'char*' */
  used_strs = alloca (sizeof (UsedStr) * n_used_strs);
  str_heap_at = str_heap;
  n_used_strs = 0;
  for (i = 0; i < n_str_info; i++)
    if (str_info[i].flags & STR_INFO_FLAG_USED)
      {
        used_strs[n_used_strs].offset = str_info[i].offset;
        used_strs[n_used_strs].str = str_heap_at;
        decompress_str (data, str_info[i].offset, &str_heap_at);
        n_used_strs++;
      }

  /* parse the four sections */
  used = 12;
  for (i = 0; i < header.qdcount; i++)
    if (!parse_question (len, data, &used,
                         &message->questions[i],
                         n_used_strs, used_strs,
                         error))
      goto cleanup;
  for (i = 0; i < total_rr; i++)
    if (!parse_resource_record (len, data, &used,
                                &message->answer_rr[i],
                                n_used_strs, used_strs, &str_heap,
                                error))
      goto cleanup;

  if (str_info_alloced > N_INIT_STR_INFO)
    dsk_free (str_info);
  return message;

cleanup:
  if (str_info_alloced > N_INIT_STR_INFO)
    dsk_free (str_info);
  return NULL;
}

/* === dsk_dns_message_serialize === */
struct _StrTreeNode
{
  unsigned offset;
  const char *str;

  /* Tree for the next word */
  StrTreeNode *subtree;

  /* child nodes */
  StrTreeNode *left, *right, *parent;
  unsigned is_red;
};

static dsk_boolean
validate_name (const char *domain_name)
{
  unsigned n_non_dot = 0;
  if (*domain_name == '.')
    ++domain_name;
  while (*domain_name == 0)
    {
      if (*domain_name == '.')
        {
          if (n_non_dot == 0)
            return DSK_FALSE;
          n_non_dot = 0;
        }
      else
        {
          if (!('a' <= *domain_name && *domain_name <= 'z')
               && *domain_name != '-' && *domain_name != '_')
            return DSK_FALSE;
          domain_name++;
          n_non_dot++;
          if (n_non_dot > 63)
            return DSK_FALSE;
        }
    }
  return DSK_TRUE;
}

static dsk_boolean
validate_question (DskDnsQuestion *question)
{
  if (!validate_name (question->name))
    return DSK_FALSE;
  /* TODO: consider validating class/rr-type */
  return DSK_TRUE;
}

static dsk_boolean
validate_resource_record (DskDnsResourceRecord *rr)
{
  if (!validate_name (rr->owner))
    return DSK_FALSE;
  switch (rr->type)
    {
    case DSK_DNS_RR_NAME_SERVER:
    case DSK_DNS_RR_CANONICAL_NAME:
    case DSK_DNS_RR_POINTER:
      return validate_name (rr->rdata.domain_name);
    case DSK_DNS_RR_HOST_INFO:
      return rr->rdata.hinfo.cpu != NULL
          && rr->rdata.hinfo.os != NULL
    case DSK_DNS_RR_MAIL_EXCHANGE:
      return validate_name (rr->rdata.mx.mail_exchange_host_name);
    case DSK_DNS_RR_START_OF_AUTHORITY:
      return validate_name (rr->rdata.soa.mname)
          && validate_name (rr->rdata.soa.rname);
    case DSK_DNS_RR_TEXT:
    case DSK_DNS_RR_HOST_ADDRESS:
    case DSK_DNS_RR_HOST_ADDRESS_IPV6:
      return DSK_TRUE;
    case DSK_DNS_RR_WELL_KNOWN_SERVICE:
    default:
      return DSK_FALSE;
    }
}

static unsigned get_name_n_components (const char *str)
{
  unsigned rv = 0;
  if (*str == '.')
    str++;
  while (*str)
    {
      if (*str == '.')
        {
          if (str[1] == 0)
            break;
          rv++;
        }
      str++;
    }
  return rv;
}
static unsigned
get_question_n_components (DskDnsQuestion *question)
{
  return get_name_n_components (question->name)
}
static unsigned
get_rr_n_components (DskDnsResourceRecord *rr)
{
  unsigned rv = get_name_n_components (rr->owner);
  switch (rr->type)
    {
    case DSK_DNS_RR_NAME_SERVER:
    case DSK_DNS_RR_CANONICAL_NAME:
    case DSK_DNS_RR_POINTER:
      rv += get_name_n_components (rr->rdata.domain_name);
      break;
    case DSK_DNS_RR_MAIL_EXCHANGE:
      rv += get_name_n_components (rr->rdata.mx.mail_exchange_host_name);
      break;
    case DSK_DNS_RR_START_OF_AUTHORITY:
      rv += get_name_n_components (rr->rdata.soa.mname);
      break;
    default:
      break;
    }
  return rv;
}

static unsigned
get_max_str_nodes (DskDnsMessage *message)
{
  unsigned max_str_nodes = 1;
  for (i = 0; i < message->n_questions; i++)
    max_str_nodes += get_question_n_components (message->questions + i);
  for (i = 0; i < message->n_answer_rr; i++)
    max_str_nodes += get_rr_n_components (message->answer_rr + i);
  for (i = 0; i < message->n_authority_rr; i++)
    max_str_nodes += get_rr_n_components (message->authority_rr + i);
  for (i = 0; i < message->n_additional_rr; i++)
    max_str_nodes += get_rr_n_components (message->additional_rr + i);
  return max_str_nodes;
}

static int
compare_dot_terminated_strs (const char *a,
                             const char *b)
{
#define IS_END_CHAR(c)  ((c) == 0 || (c) == '.')
  char ca, cb;
  while (!IS_END_CHAR (*a) && !IS_END_CHAR (*b))
    {
      if (*a < *b) return -1;
      else if (*a > *b) return 1;
      a++, b++;
    }
  ca = (*a == '.') ? 0 : *a;
  cb = (*b == '.') ? 0 : *b;
  return (ca < cb) ? -1 : (ca > cb) ? 1 : 0;
#undef IS_END_CHAR
}

#define COMPARE_STR_TREE_NODES(a,b, rv) \
         rv = compare_dot_terminated_strs (a->str, b->str)
#define COMPARE_STR_TO_TREE_NODE(a,b, rv) \
         rv = compare_dot_terminated_strs (a, b->str)

#define STR_NODE_GET_TREE(top) \
  (top), StrTreeNode*,  \
  GSK_STD_GET_IS_RED, GSK_STD_SET_IS_RED, \
  parent, left, right, \
  COMPARE_STR_TREE_NODES

static unsigned 
get_name_size (const char     *name,
               StrTreeNode   **p_top,
               StrTreeNode   **p_heap)
{
  const char *end = strchr (name, 0);
  StrTreeNode *p_at = p_top;
  dsk_boolean is_first = DSK_TRUE;
  dsk_boolean use_ptr = DSK_FALSE;
  unsigned str_size = 0;
  if (end == name)
    return 1;
  for (;;)
    {
      const char *cstart;
      StrTreeNode *child;
      while (end >= name && end[-1] == '.')
        end--;
      if (end == name)
        break;
      cstart = end;
      while (cstart >= name && cstart[-1] != '.')
        cstart--;
      if (*p_at != NULL)
        /* lookup child with this name */
        GSK_RBTREE_LOOKUP_COMPARATOR (STR_NODE_GET_TREE (*p_at),
                                      cstart, COMPARE_STR_TO_TREE_NODE,
                                      child);
      else
        child = NULL;
      if (is_first)
        {
          use_ptr = (child != NULL);
          is_first = DSK_FALSE;
        }
      if (child == NULL)
        {
          /* create new node */
          child = *p_heap++;
          memset (child, 0, sizeof (StrTreeNode));

          /* insert node */
          GSK_RBTREE_INSERT (STR_NODE_GET_TREE (p_at), 
                             child, conflict);
          dsk_assert (conflict == NULL);

          /* increase size */
          str_size += (end - cstart) + 1;
        }
      p_at = &child->subtree;

      if (cstart == name)
        break;
      end = cstart - 1;
    }
  return str_size + (use_ptr ? 2 : 1);
}

static unsigned 
get_question_size (DskDnsQuestion *question,
                   StrTreeNode   **p_top,
                   StrTreeNode   **p_heap)
{
  return get_name_size (question->name, p_top, p_heap) + 4;
}
static unsigned
get_rr_size (DskDnsResourceRecord *rr,
             StrTreeNode         **p_top,
             StrTreeNode         **p_heap)
{
  unsigned rv = get_name_size (rr->owner, p_top, p_heap) + 10;
  switch (rr->type)
    {
    case DSK_DNS_RR_HOST_ADDRESS:
      rv += 4;
      break;
    case DSK_DNS_RR_NAME_SERVER:
    case DSK_DNS_RR_CANONICAL_NAME:
    case DSK_DNS_RR_POINTER:
      rv += get_name_size (rr->rdata.domain_name, p_top, p_heap);
      break;
    case DSK_DNS_RR_HOST_INFO:
      rv += 4;
      break;
    case DSK_DNS_RR_MAIL_EXCHANGE:
      rv += get_name_size (rr->rdata.mx.mail_exchange_host_name, p_top, p_heap);
      rv += 2;
      break;
    case DSK_DNS_RR_START_OF_AUTHORITY:
      rv += get_name_size (rr->rdata.soa.mname, p_top, p_heap);
      rv += get_name_size (rr->rdata.soa.rname, p_top, p_heap);
      rv += 20;
      break;
    case DSK_DNS_RR_TEXT:
      rv += strlen (rr->rdata.text) + 1;
      break;
    case DSK_DNS_RR_HOST_ADDRESS_IPV6:
      rv += 16;
      break;
    default:
      dsk_assert_not_reached ();
    }
}

static void
pack_message_header (DskDnsMessage *message,
                     uint8_t       *out)
{
  DskDnsHeader header;
  header.qid = htons (message->id);
  header.qr = 1 ^ message->is_query;
  header.opcode = message->opcode;
  header.aa = message->is_authoritative;
  header.tc = 0;        //message->is_truncated;
  header.rd = message->recursion_desired;
  header.ra = message->recursion_available;
  header.unused = 0;
  header.rcode = message->rcode;
  header.qdcount = htons (message->n_questions);
  header.ancount = htons (message->n_answer_rr);
  header.nscount = htons (message->n_authority_rr);
  header.arcount = htons (message->n_additional_rr);
  dsk_assert (sizeof (DskDnsHeader) == 12);
  memcpy (out, &header, 12);
}
static void
write_pointer (uint8_t **data_inout,
               unsigned offset)
{
  /* write pointer */
  uint8_t bytes[2];
  bytes[0] = 0xc0 | (up->offset >> 8);
  bytes[1] = up->offset;
  memcpy (*data_inout, bytes, 2);
  *data_inout += 2;
}

static void
pack_domain_name  (const char     *name,
                   uint8_t        *data_start,      /* for computing offsets */
                   uint8_t       **data_inout,
                   StrTreeNode    *top)
{
  StrTreeNode *up = NULL;

  if (name[0] == '.')
    name++;
  if (name[0] == 0)
    {
      **data_inout = 0;
      *data_inout += 1;
      return;
    }
  end = strchr (name, 0);
  end--;
  if (*end == '.')
    end--;

  /* scan up tree until we find one with offset==0;
     or until we run out of components */
  while (name < end)
    {
      const char *beg = end;
      StrTreeNode *cur;
      while (beg > name && *beg != '.')
        beg--;
      GSK_RBTREE_LOOKUP_COMPARATOR (STR_NODE_GET_TREE (top),
                                    beg, COMPARE_STR_TO_TREE_NODE,
                                    cur);
      dsk_assert (cur != NULL);
      if (cur->offset == 0)
        {
          /* write remaining strings -- first calculate the size
             then start at the end. */
          unsigned packed_str_size = (end + 1) - name;
          char *at = *data_inout + packed_str_size;
          while (end != name)
            {
              const char *beg = end;
              while (beg > name && *beg != '.')
                beg--;
              at -= (end-beg);
              memcpy (at, beg, end - beg);
              at--;
              *at = (end-beg);
              end = beg;
              if (end > name)
                end--;          /* skip . */
            }
          dsk_assert (at == *data_inout);
          *data_inout += packed_str_size;

          if (up != NULL)
            write_pointer (data_inout, up->offset);
          else
            {
              /* write 0 */
              **data_inout = 0;
              *data_inout += 1;
            }
          return;
        }
      end = beg;
      if (end > name)
        end--;
      up = top;
      top = cur->subtree;
    }

  write_pointer (data_inout, up->offset);
}

static void
pack_question (DskDnsQuestion *question,
               uint8_t        *data_start,      /* for computing offsets */
               uint8_t       **data_inout,
               StrTreeNode    *top)
{
  uint16_t qarray[2];
  pack_domain_name (question->name, data_start, data_inout, top);
  qarray[0] = htons (question->type);
  qarray[1] = htons (question->question_class);
  memcpy (*data_inout, qarray, 4);
  *data_inout += 4;
}

static void
pack_len_prefixed_string (const char *str,
                          uint8_t   **data_inout)
{
  unsigned len = strlen (str);
  **data_inout = len;
  *data_inout += 1;
  memcpy (*data_inout, str, len);
  *data_inout += len;
}

static void
pack_resource_record (DskDnsResourceRecord *rr,
                      uint8_t        *data_start, /* for computing offsets */
                      uint8_t       **data_inout,
                      StrTreeNode    *top)
{
  uint8_t *generic;
  unsigned rdata_len;
  pack_domain_name (rr->owner, data_start, data_inout, top);

  /* reserve space for generic part of resource-record */
  generic = *data_inout;
  *data_inout += 10;

  /* pack type-specific rdata */
  switch (rr->type)
    {
    case DSK_DNS_RR_HOST_ADDRESS:
      memcpy (*data_inout, rr->rdata.a.ip_address, 4);
      *data_inout += 4;
      break;
    case DSK_DNS_RR_HOST_ADDRESS_IPV6:
      memcpy (*data_inout, rr->rdata.aaaa.address, 16);
      *data_inout += 16;
      break;
    case DSK_DNS_RR_NAME_SERVER:
    case DSK_DNS_RR_CANONICAL_NAME:
    case DSK_DNS_RR_POINTER:
      pack_domain_name (rr->rdata.domain_name, data_start, data_inout, top);
      break;
    case DSK_DNS_RR_MAIL_EXCHANGE:
      {
        uint16_t pv_be = htons (rr->rdata.mx.preference_value);
        memcpy (*data_inout, &pv_be, 2);
        *data_inout += 2;
      }
      pack_domain_name (rr->rdata.mx.mail_exchange_host_name, data_start, data_inout, top);
      break;
    case DSK_DNS_RR_HOST_INFO:
      pack_len_prefixed_string (rr->rdata.hinfo.cpu, data_inout);
      pack_len_prefixed_string (rr->rdata.hinfo.os, data_inout);
      break;
    case DSK_DNS_RR_START_OF_AUTHORITY:
      pack_domain_name (rr->rdata.soa.mname, data_start, data_inout, top);
      pack_domain_name (rr->rdata.soa.rname, data_start, data_inout, top);
      {
        uint32_t intervals[5];
	intervals[0] = htonl (rr->rdata.soa.serial);
	intervals[1] = htonl (rr->rdata.soa.refresh_time);
	intervals[2] = htonl (rr->rdata.soa.retry_time);
	intervals[3] = htonl (rr->rdata.soa.expire_time);
	intervals[4] = htonl (rr->rdata.soa.minimum_time);
        memcpy (*data_inout, intervals, 20);
        *data_inout += 20;
      }
      break;
    case DSK_DNS_RR_TEXT:
      pack_len_prefixed_string (rr->rdata.text, data_inout);
      break;
    default:
      /* This should not happen, because validate_resource_record()
         returned TRUE, */
      dsk_assert_not_reached ();
    }

  /* write generic resource-code info */
  rdata_len = *data_inout - generic;
  data[0] = htons (rr->type);
  data[1] = htons (rr->record_class);
  data[2] = htons (rr->time_to_live >> 16);
  data[3] = htons (rr->time_to_live);
  data[4] = htons (rdata_len);
  memcpy (generic, data, 10);
}

uint8_t *
dsk_dns_message_serialize (DskDnsMessage *message,
                           unsigned      *length_out)
{
  unsigned max_str_nodes;
  StrTreeNode *nodes;
  StrTreeNode *nodes_at;                /* next node to us */
  StrTreeNode *top = NULL;
  unsigned i;
  unsigned size;
  uint8_t *rv, *at;

  /* check message contents:
     - string bounds
     - opcode / errcode validity
     - lowercased domain-names (?)
     - n_questions, etc must be less than 1<<16
   */
  if (message->n_questions > 0xffff
   || message->n_answer_rr > 0xffff
   || message->n_authority_rr > 0xffff
   || message->n_additional_rr > 0xffff)
    return NULL;
  if (!validate_rcode (message->rcode)
   || !validate_opcode (message->opcode))
    return NULL;
  for (i = 0; i < message->n_questions; i++)
    if (!validate_question (message->questions + i))
      return NULL;
  for (i = 0; i < message->n_answer_rr; i++)
    if (!validate_resource_record (message->answer_rr + i))
      return NULL;
  for (i = 0; i < message->n_authority_rr; i++)
    if (!validate_resource_record (message->authority_rr + i))
      return NULL;
  for (i = 0; i < message->n_additional_rr; i++)
    if (!validate_resource_record (message->additional_rr + i))
      return NULL;

  max_str_nodes = get_max_str_nodes (message);

  /* scan through figuring out how long the packed data will be. */
  nodes = alloca (sizeof (StrTreeNode) * max_str_nodes);
  nodes_at = nodes;
  size = 12;
  for (i = 0; i < message->n_questions; i++)
    size += get_question_size (message->questions + i, &top, &nodes_at);
  for (i = 0; i < message->n_answer_rr; i++)
    size += get_rr_size (message->answer_rr + i, &top, &nodes_at);
  for (i = 0; i < message->n_authority_rr; i++)
    size += get_rr_size (message->authority_rr + i, &top, &nodes_at);
  for (i = 0; i < message->n_additional_rr; i++)
    size += get_rr_size (message->additional_rr + i, &top, &nodes_at);
  dsk_assert (nodes_at - nodes <= max_str_nodes);

  /* pack the message */
  rv = dsk_malloc (size);
  at = rv;
  pack_message_header (message, &at);
  for (i = 0; i < message->n_questions; i++)
    pack_question (message->questions + i, rv, &at, top);
  for (i = 0; i < message->n_answer_rr; i++)
    pack_rr (message->answer_rr + i, rv, &at, top);
  for (i = 0; i < message->n_authority_rr; i++)
    pack_rr (message->authority_rr + i, rv, &at, top);
  for (i = 0; i < message->n_additional_rr; i++)
    pack_rr (message->additional_rr + i, rv, &at, top);
  dsk_assert ((unsigned)(at - rv) == size);
  *length_out = size;

  return rv;
}
