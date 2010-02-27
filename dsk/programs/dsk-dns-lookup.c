

static dsk_boolean use_ipv6 = DSK_FALSE;
static dsk_boolean no_links = DSK_FALSE;
static const char *nameserver = NULL;
static dsk_boolean verbose = DSK_FALSE;

int main(int argc, char **argv)
{
  dsk_cmdline_init ("perform DNS lookups",
                    "Perform DNS lookups with Dsk DNS client.\n",
                    0);
  dsk_cmdline_permit_extra_arguments (DSK_TRUE);
  dsk_cmdline_add_boolean ("ipv6", "Lookup names in the IPv6 namespace", NULL, 0, &use_ipv6);
  dsk_cmdline_add_boolean ("ipv4", "Lookup names in the IPv4 namespace", NULL, DSK_CMDLINE_REVERSED, &use_ipv6);
  dsk_cmdline_add_boolean ("cname", "Return CNAME or POINTER records if they arise", NULL, 0, &no_links);
  dsk_cmdline_add_string ("nameserver", "Specify the nameserver to use", "IP", 0, &nameserver);
  dsk_cmdline_add_boolean ("verbose", "Print extra messages", NULL, 0, &verbose);
  dsk_cmdline_process_args (&argc, &argv);

  if (nameserver == NULL)
    {
      if (verbose)
        {
          ...
        }
    }
  else
    {
      dsk_dns_client_suppress_resolv_conf ();
      dsk_dns_client_add_nameserver (nameserver);
    }

  if (argc == 1)
    dsk_error ("expected name to resolve");
  for (i = 1; i < argc; i++)
    {
      ...
    }
  return 0;
}

void dsk_cmdline_init        (const char     *static_short_desc,
                              const char     *long_desc,
                              DskCmdlineInitFlags flags);
  ..
}
