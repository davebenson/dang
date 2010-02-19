


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

/* b == any byte
   s == any string */
static dsk_boolean
gather_names_generic (const char *code,
                      unsigned       len,
                      const uint8_t *data,
                      unsigned       rdlength,
                      unsigned      *used_inout, 
                      unsigned      *n_str_info_inout,
                      StrInfo      **str_info_inout,
                      unsigned      *str_info_alloced_inout,
                      DskError     **error)
{
  unsigned old_used = *used_inout;
  ....
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
  /* owner */
  if (!gather_names (len, data, used_inout,
                     n_str_info_inout, str_info_inout, str_info_alloced_inout,
                     error))
    return DSK_FALSE;
  if (*used_inout + 10 > len)
    {
      ...
    }
  memcpy (header, data + *used_inout, 10);
  *used_inout += 10;
  type = ((guint16)header[0] << 8) | ((guint16)header[1] << 0);
  rdlength = ((guint16)header[8] << 8)  | ((guint16)header[9] << 0);
  if (*used_inout + rdlength > len)
    {
      ...
    }
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
      return NULL;
    default:
      dsk_set_error ("unknown resource-record type %u", type);
      return NULL;
    }

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
      DSK_QSORT (...);
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
    if (!parse_question (len - used, data + used,
                         &message->questions[i],
                         n_used_strs, used_strs,
                         error))
      goto cleanup;
  for (i = 0; i < total_rr; i++)
    if (!parse_rr (...)
    }

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
