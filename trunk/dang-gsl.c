#include "dang.h"
#include <gsl/gsl_linalg.h>

static dang_boolean
check_matrix_square (DangMatrix *m,
                     const char *func_name,
                     DangError **error)
{
  if (m->n_rows != m->n_cols)
    {
      dang_set_error (error, "matrix must be square (in %s()) (was %ux%u)",
                      func_name, m->n_rows, m->n_cols);
      return FALSE;
    }
  return TRUE;
}

static void
init_matrix (DangMatrix *out,
             unsigned    n_rows,
             unsigned    n_cols)
{
  out->data = dang_new (double, n_rows * n_cols);
  out->n_rows = n_rows;
  out->n_cols = n_cols;
}

static _gsl_matrix_view view_dang_matrix_as_gsl_matrix (DangMatrix *mat)
{
  return gsl_matrix_view_array (mat->data, mat->n_rows, mat->n_cols);
}

int gsl_linalg_exponential_ss(
  const gsl_matrix * A,
  gsl_matrix * eA,
  gsl_mode_t mode
  );

static DANG_SIMPLE_C_FUNC_DECLARE(simple_c__gsl_linalg_exponential_ss)
{
  DangMatrix *A = args[0];
  DangMatrix *eA = rv_out;
  gsl_matrix_view A_gsl, eA_gsl;
  DANG_UNUSED (func_data);
  if (!check_matrix_square (A, "exp", error))
    return FALSE;
  init_matrix (eA, A->n_rows, A->n_cols);
  A_gsl = view_dang_matrix_as_gsl_matrix (A);
  eA_gsl = view_dang_matrix_as_gsl_matrix (eA);
  if (gsl_linalg_exponential_ss (&A_gsl.matrix, &eA_gsl.matrix, GSL_MODE_DEFAULT) != 0)
    {
      dang_set_error (error, "error doing exponential");
      return FALSE;
    }
  return TRUE;
}

void _dang_gsl_init (DangNamespace *ns)
{
  DangValueType *mat_type = dang_value_type_matrix (dang_value_type_double ());
#define add_simple dang_namespace_add_simple_c_from_params
  add_simple (ns, "exp", simple_c__gsl_linalg_exponential_ss,
              mat_type,
              1,
              DANG_FUNCTION_PARAM_IN, "A", mat_type);
}
