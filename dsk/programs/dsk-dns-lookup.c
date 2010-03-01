#include "../dsk.h"

static dsk_boolean use_ipv6 = DSK_FALSE;
static dsk_boolean no_links = DSK_FALSE;
static char *nameserver = NULL;
static dsk_boolean verbose = DSK_FALSE;

int main(int argc, char **argv)
{
  DskDnsConfigFlags cfg_flags = DSK_DNS_CONFIG_FLAGS_DEFAULT;
  dsk_boolean no_searchpath = DSK_FALSE;
  dsk_cmdline_init ("perform DNS lookups",
                    "Perform DNS lookups with Dsk DNS client.\n",
                    0);
  dsk_cmdline_permit_extra_arguments (DSK_TRUE);
  dsk_cmdline_add_boolean ("ipv6", "Lookup names in the IPv6 namespace", NULL, 0, &use_ipv6);
  dsk_cmdline_add_boolean ("ipv4", "Lookup names in the IPv4 namespace", NULL, DSK_CMDLINE_REVERSED, &use_ipv6);
  dsk_cmdline_add_boolean ("cname", "Return CNAME or POINTER records if they arise", NULL, 0, &no_links);
  dsk_cmdline_add_string ("nameserver", "Specify the nameserver to use", "IP", 0, &nameserver);
  dsk_cmdline_add_boolean ("verbose", "Print extra messages", NULL, 0, &verbose);
  dsk_cmdline_add_boolean ("no-searchpath", "Do not use /etc/resolv.conf's searchpath", NULL, 0, &no_searchpath);
  dsk_cmdline_process_args (&argc, &argv);

  if (no_searchpath)
    cfg_flags &= ~DSK_DNS_CONFIG_USE_RESOLV_CONF_SEARCHPATH;

  if (nameserver == NULL)
    {
      /* just use default config */
    }
  else
    {
      cfg_flags &= ~DSK_DNS_CONFIG_USE_RESOLV_CONF_NS;
      dsk_dns_client_add_nameserver (nameserver);
    }
  dsk_dns_client_config (cfg_flags);
  if (verbose)
    {
      dsk_dns_config_dump ();
    }

  if (argc == 1)
    dsk_error ("expected name to resolve");
  for (i = 1; i < argc; i++)
    {
      n_running++;
      dsk_dns_lookup (argv[i],
                      use_ipv6,
                      handle_dns_result,
                      NULL);
      while (n_running >= max_concurrent)
        dsk_main_run_once ();
    }
  return 0;
}
