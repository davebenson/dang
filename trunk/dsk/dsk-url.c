
dsk_boolean  dsk_url_scan  (const char     *url_string,
                            DskUrlScanned  *out,
                            DskError      **error)
{
			    -
  int num_slashes;
  const char *at = url_string;
  DskUrlInterpretation interpretation = DSK_URL_INTERPRETATION_UNKNOWN;
  int port;
  out->scheme_start = at;
  while (dsk_ascii_isalnum (*at))
    at++;
  if (out->scheme_start == at)
    {
      dsk_set_error (error, "no scheme found in URL");
      return DSK_FALSE;
    }
  if (*at != ':')
    {
      dsk_set_error (error, "missing : after scheme in URL");
      return DSK_FALSE;
    }
  out->scheme_end = at;
  at++;         /* skip : */

  /* Parse scheme */
  out->scheme = DSK_URL_SCHEME_UNKNOWN;
  switch (out->scheme_end - out->scheme_start)
    {
    case 3:
      if (dsk_ascii_strncasecmp (out->scheme_start, "ftp", 3) == 0)
        out->scheme = DSK_URL_SCHEME_FTP;
      break;
    case 4:
      if (dsk_ascii_strncasecmp (out->scheme_start, "http", 4) == 0)
        out->scheme = DSK_URL_SCHEME_HTTP;
      else if (dsk_ascii_strncasecmp (out->scheme_start, "file", 4) == 0)
        out->scheme = DSK_URL_SCHEME_FILE;
      break;
    case 5:
      if (dsk_ascii_strncasecmp (out->scheme_start, "https", 5) == 0)
        out->scheme = DSK_URL_SCHEME_HTTPS;
      break;
    }
      break;

  num_slashes = 0;
  while (*at == '/')
    {
      num_slashes++;
      at++;
    }
  if (out->scheme == DSK_URL_SCHEME_FILE)
    out->interpretation = DSK_URL_INTERPRETATION_ABSOLUTE;
  else
    switch (num_slashes)
      {
	case 0:
	  out->interpretation = DSK_URL_INTERPRETATION_RELATIVE;
	  break;
	case 1:
	  out->interpretation = DSK_URL_INTERPRETATION_ABSOLUTE;
	  break;
	case 2:
	  /* ``schemes including a top hierarchical element for a naming
	   *   authority'' (Section 3.2)
	   */
	  out->interpretation = DSK_URL_INTERPRETATION_REMOTE;
	  break;
	case 3:
	  /* File urls (well those are now handled above so this
	   * is pretty dubious)
	   */
	  out->interpretation = DSK_URL_INTERPRETATION_ABSOLUTE;
	  break;
	default:
          /* hmm */
	  out->interpretation = DSK_URL_INTERPRETATION_ABSOLUTE;
	  break;
      }


  switch (interpretation)
    {
      case GSK_URL_INTERPRETATION_REMOTE:
	/* rfc 2396, section 3.2.2. */
	{
	  const char *end_hostport;
	  const char *at_sign;
	  const char *colon;
	  /* basically the syntax is:
           *    USER@HOST:PORT/
           *        ^    |    ^
           *     at_sign ^  end_hostport
           *            colon
           */             
	  end_hostport = strchr (at, '/');
	  if (end_hostport == NULL)
#if 1
            end_hostport = strchr (at, 0);
#else           /* too strict for casual use ;) */
	    {
	      /* TODO: it's kinda hard to pinpoint where this
		 is specified.  See Section 3 in RFC 2396. */
	      g_set_error (error, GSK_G_ERROR_DOMAIN,
			   GSK_ERROR_INVALID_ARGUMENT,
			   _("missing / after host in URL"));
	      return NULL;
	    }
#endif
	  at_sign = memchr (at, '@', end_hostport - at);
	  out->host_start = at_sign != NULL ? (at_sign + 1) : at;
	  colon = memchr (host_start, ':', end_hostport - host_start);
	  if (at_sign != NULL)
	    {
              const char *password_sep = memchr (at, ':', at_sign - at);
              if (password_sep)
                {
                  user_name = g_strndup (at, password_sep - at);
                  password = g_strndup (password_sep + 1,
                                        at_sign - (password_sep + 1));
                }
              else
                {
                  user_name = g_strndup (at, at_sign - at);
                }
	      /* XXX: should validate username against 
	       *         GSK_URL_USERNAME_CHARSET
	       */
	    }
	  out->host_end = colon != NULL ? colon : end_hostport;

	  if (colon != NULL)
            {
              out->port_start = colon + 1;
              out->port_end = end_hostport;
              out->port = atoi (out->port_start);
            }

	  at = end_hostport;
	}

	/* fall through to parse the host-specific part of the url */
      case GSK_URL_INTERPRETATION_RELATIVE:
      case GSK_URL_INTERPRETATION_ABSOLUTE:
        {
	  const char *query_start;
	  const char *frag_start;

          out->host_start = out->host_end = NULL;
          out->username_start = out->username_end = NULL;
          out->password_start = out->password_end = NULL;
          out->port_start = out->port_end = NULL;
          out->port = 0;

	  if (num_slashes > 0
           && interpretation == GSK_URL_INTERPRETATION_ABSOLUTE)
	    at--;
	  query_start = strchr (at, '?');
	  frag_start = strchr (query_start != NULL ? query_start : at, '#');
	  if (query_start != NULL)
	    path = g_strndup (at, query_start - at);
	  else if (frag_start != NULL)
	    path = g_strndup (at, frag_start - at);
	  else
	    path = g_strdup (at);
	  if (query_start != NULL)
	    {
	      if (frag_start != NULL)
		query = g_strndup ((query_start+1), frag_start - (query_start+1));
	      else
		query = g_strdup (query_start + 1);
	    }
	  if (frag_start != NULL)
	    fragment = g_strdup (frag_start + 1);
	  break;
	}
      case GSK_URL_INTERPRETATION_UNKNOWN:
        {
	  g_set_error (error, GSK_G_ERROR_DOMAIN, GSK_ERROR_BAD_FORMAT,
		       _("cannot guess how to interpret %s:%s"),
	  	       gsk_url_scheme_name (scheme), start);
	  goto error;
	}
    }

  if (interpretation == GSK_URL_INTERPRETATION_REMOTE
  && (host == NULL || host[0] == '\0' || !isalnum (host[0])))
    {
      g_set_error (error, GSK_G_ERROR_DOMAIN, GSK_ERROR_BAD_FORMAT,
		   _("malformed host: should begin with a letter or number (%s)"),
		   host);
      goto error;
    }



  url = g_object_new (GSK_TYPE_URL, NULL);
  url->scheme = scheme;
  if (scheme == GSK_URL_SCHEME_OTHER)
    url->scheme_name = NULL;
  else
    url->scheme_name = (char *) gsk_url_scheme_name (scheme);
  url->host = host;
  url->user_name = user_name;
  url->password = password;
  url->query = query;
  url->fragment = fragment;
  url->port = port;
  url->path = path;

  if (!url_check_is_valid (url, error))
    {
      g_object_unref (url);
      return NULL;
    }
  return url;

error:
  g_free (host);
  g_free (user_name);
  g_free (password);
  g_free (query);
  g_free (fragment);
  g_free (path);
  return NULL;
