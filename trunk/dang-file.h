
typedef struct _DangFileClass DangFileClass;
typedef struct _DangFile DangFile;

#include <stdio.h>

struct _DangFileClass
{
  DangObjectClass base_class;
};
struct _DangFile
{
  DangObject base_instance;
  FILE *fp;			/* NOTE: not committing to this impl :) */
};


void _dang_file_init (DangNamespace *ns);
