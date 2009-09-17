
typedef struct _DangRunFileOptions DangRunFileOptions;
struct _DangRunFileOptions
{
  dang_boolean is_module;
  unsigned n_module_names;
  char **module_names;
};
#define DANG_RUN_FILE_OPTIONS_DEFAULTS { FALSE, 0, NULL }

dang_boolean dang_run_file (const char           *filename,
                            DangRunFileOptions   *options,
                            DangError           **error);


DangExpr *dang_do_toplevel_transforms (DangExpr   *in,
                                       DangError **error);
