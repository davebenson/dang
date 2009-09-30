#include <unistd.h>             /* for access(2) */
#include <stdio.h>
#include <string.h>
#include "dang.h"

static DangArray module_search_paths = DANG_UTIL_ARRAY_STATIC_INIT (char*);
static unsigned max_path_len = 0;
static DangArray loaded_modules = DANG_UTIL_ARRAY_STATIC_INIT (char *);

void
dang_module_add_path (const char *path)
{
  unsigned path_len = strlen (path);
  char *copy = dang_strdup (path);
  dang_util_array_append (&module_search_paths, 1, &copy);
  max_path_len = DANG_MAX (max_path_len, path_len);
}

dang_boolean
dang_module_load     (unsigned    n_names,
                      char      **names,
                      DangError **error)
{
  char *buf;
  char *last, *end;
  unsigned i;
  unsigned len = 0;
  dang_assert (n_names > 0);
  for (i = 0; i < n_names; i++)
    len += strlen (names[i]);
  len += n_names;               /* for slashes and NUL */

  last = dang_malloc (len);
  end = last;
  for (i = 0; i < n_names; i++)
    {
      strcpy (end, names[i]);
      end = strchr (end, 0);
      if (i + 1 < n_names)
        *end++ = '/';
    }

  for (i = 0; i < loaded_modules.len; i++)
    if (strcmp (((char**)loaded_modules.data)[i], last) == 0)
      return TRUE;

  len += max_path_len + 1;
  len += 5;                     /* .dang extension */
  buf = dang_malloc (len);

  for (i = 0; i < module_search_paths.len; i++)
    {
      snprintf (buf, len, "%s/%s.dang", ((char**)module_search_paths.data)[i], last);
      if (access (buf, R_OK) == 0)
        {
          DangRunFileOptions options = DANG_RUN_FILE_OPTIONS_DEFAULTS;
          options.is_module = TRUE;
          options.n_module_names = n_names;
          options.module_names = names;
          if (!dang_run_file (buf, &options, error))
            {
              dang_free (buf);
              return FALSE;
            }
          dang_util_array_append (&loaded_modules, 1, &last);
          return TRUE;
        }
    }
  dang_set_error (error, "%s.dang not found on include-path", last);
  dang_free (last);
  dang_free (buf);
  return FALSE;
}
