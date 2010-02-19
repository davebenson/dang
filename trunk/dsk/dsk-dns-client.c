
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


/* --- low-level ---*/
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


typedef enum
{
  DSK_DNS_TYPE_A         = 1,
  DSK_DNS_TYPE_NS        = 2,
  DSK_DNS_TYPE_CNAME     = 5,
  DSK_DNS_TYPE_SOA       = 6,
  DSK_DNS_TYPE_PTR       = 12,
  DSK_DNS_TYPE_MX        = 15,
  DSK_DNS_TYPE_TXT       = 16,
  DSK_DNS_TYPE_AAAA      = 28,
  DSK_DNS_TYPE_SRV       = 33,
  DSK_DNS_TYPE_SPF       = 99,

  DSK_DNS_TYPE_ALL       = 255
} DskDnsTypeCode;


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

struct _DskDnsMessage
{
  DskDnsHeader header;
  DskDnsQuestion *questions;
  DskDnsResourceRecord *answers;
  DskDnsResourceRecord *authority;
  DskDnsResourceRecord *additional;
};

struct _StrInfo
{
  uint16_t offset;
  uint16_t length;
  uint16_t flags;
};
struct _UsedStr
{
  unsigned offset;
  char *str;
};
typedef enum
{
  STR_INFO_FLAG_USED = 1,
  STR_INFO_FLAG_COMPUTING_LENGTH = 2
} StrInfoFlags;

static dsk_boolean
compute_length (unsigned n_str_info,
                StrInfo *str_info,
                const uint8_t *data,
                unsigned i,
                const char **msg)
{
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
      if ((*at & 0xc0) != 0xc0)
        {
          ...
        }
      new_offset = ((*at - 0xc0) << 8) + at[1];

      /* bsearch new offset */
      ...

      /* check for circular loop */
      ...

      if (!compute_length (n_str_info, str_info, data, new_i, msg))
        return DSK_FALSE;
      len += str_info[i].length + 1;
    }

  str_info[i].length = len ? (len - 1) : 0;
  str_info[i].flags &= ~STR_INFO_FLAG_COMPUTING_LENGTH;
  str_info[i].flags |= STR_INFO_FLAG_HAS_LENGTH;
  return DSK_TRUE;
}

static DskDnsMessage *
parse_message (unsigned       len,
               const uint8_t *data,
               DskError     **error)
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
                                         &str_info_alloced, error))
        goto cleanup;
    }

  /* compute the length of each offset, detecting cycles. */
  if (n_str_info > 0)
    {
      unsigned o;
      GSK_QSORT (...);
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
  message->n_questions = header.qdcount;
  message->questions = (DskDnsQuestion *) (message + 1);
  message->n_answer_rr = header.ancount;
  message->answer_rr = (DskDnsResourceRecord *) (message->questions + message->n_questions);
  message->n_ns_rr = header.nscount;
  message->ns_rr = message->answer_rr + message->n_answer_rr;
  message->n_authority_rr = header.aucount;
  message->authority_rr = message->ns_rr + message->n_ns_rr;
  str_heap = (char*) (message->authority_rr + message->n_authority_rr);

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
    if (!parse_question (...))
      goto cleanup;

////  /* question section */
////  used = 12;
////  for (i = 0; i < header.qdcount; i++)
////    {
////      DskDnsQuestion *question = parse_question (&iterator, message);
////      if (question == NULL)
////	{
////	  PARSE_FAIL ("question section");
////	  goto fail;
////	}
////      message->questions = g_slist_prepend (message->questions, question);
////    }
////  message->questions = g_slist_reverse (message->questions);
////
////  /* the other three sections are the same: a list of resource-records */
////  if (!parse_resource_record_list (&iterator, answer_count,
////				   &message->answers, "answer", message))
////    goto fail;
////  if (!parse_resource_record_list (&iterator, auth_count,
////				   &message->authority, "authority", message))
////    goto fail;
////  if (!parse_resource_record_list (&iterator, addl_count,
////				   &message->additional, "additional", message))
////    goto fail;
////
////  g_assert (g_slist_length (message->questions) == question_count);
////  g_assert (g_slist_length (message->answers) == answer_count);
////  g_assert (g_slist_length (message->authority) == auth_count);
////  g_assert (g_slist_length (message->additional) == addl_count);
////
////  if (num_bytes_parsed != NULL)
////    *num_bytes_parsed = gsk_buffer_iterator_offset (&iterator);
////  return message;
////
////fail:
////  if (message != NULL)
////    gsk_dns_message_unref (message);
////  return NULL;
