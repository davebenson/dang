#include <math.h>
#include "dang.h"

/* Period parameters */  
#define N 624
#define M 397
#define MATRIX_A 0x9908b0df   /* constant vector a */
#define UPPER_MASK 0x80000000 /* most significant w-r bits */
#define LOWER_MASK 0x7fffffff /* least significant r bits */

/* Tempering parameters */   
#define TEMPERING_MASK_B 0x9d2c5680
#define TEMPERING_MASK_C 0xefc60000
#define TEMPERING_SHIFT_U(y)  (y >> 11)
#define TEMPERING_SHIFT_S(y)  (y << 7)
#define TEMPERING_SHIFT_T(y)  (y << 15)
#define TEMPERING_SHIFT_L(y)  (y >> 18)

typedef struct _DangRand DangRand;
struct _DangRand
{
  uint32_t mt[N]; /* the array for the state vector  */
  unsigned mti; 
};
static DangRand the_rand;
static dang_boolean inited = FALSE;

static void
dang_rand_set_seed (uint32_t seed)
{
  DangRand *rand = &the_rand;
  rand->mt[0]= seed;
  for (rand->mti=1; rand->mti<N; rand->mti++)
    rand->mt[rand->mti] = 1812433253UL * 
      (rand->mt[rand->mti-1] ^ (rand->mt[rand->mti-1] >> 30)) + rand->mti; 
  inited = TRUE;
}

void
dang_rand_set_seed_array (const uint32_t *seed, unsigned seed_length)
{
  int i, j, k;
  DangRand *rand = &the_rand;

  dang_rand_set_seed (19650218UL);

  i=1; j=0;
  k = (N>seed_length ? N : seed_length);
  for (; k; k--)
    {
      rand->mt[i] = (rand->mt[i] ^
		     ((rand->mt[i-1] ^ (rand->mt[i-1] >> 30)) * 1664525UL))
	      + seed[j] + j; /* non linear */
      rand->mt[i] &= 0xffffffffUL; /* for WORDSIZE > 32 machines */
      i++; j++;
      if (i>=N)
        {
	  rand->mt[0] = rand->mt[N-1];
	  i=1;
	}
      if (j>=(int)seed_length)
	j=0;
    }
  for (k=N-1; k; k--)
    {
      rand->mt[i] = (rand->mt[i] ^
		     ((rand->mt[i-1] ^ (rand->mt[i-1] >> 30)) * 1566083941UL))
	      - i; /* non linear */
      rand->mt[i] &= 0xffffffffUL; /* for WORDSIZE > 32 machines */
      i++;
      if (i>=N)
        {
	  rand->mt[0] = rand->mt[N-1];
	  i=1;
	}
    }

  rand->mt[0] = 0x80000000UL; /* MSB is 1; assuring non-zero initial array */ 
}

uint32_t
dang_rand_int (void)
{
  uint32_t y;
  static const uint32_t mag01[2]={0x0, MATRIX_A};
  DangRand *rand = &the_rand;
  /* mag01[x] = x * MATRIX_A  for x=0,1 */

  if (rand->mti >= N) { /* generate N words at one time */
    int kk;
    
    for (kk=0;kk<N-M;kk++) {
      y = (rand->mt[kk]&UPPER_MASK)|(rand->mt[kk+1]&LOWER_MASK);
      rand->mt[kk] = rand->mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1];
    }
    for (;kk<N-1;kk++) {
      y = (rand->mt[kk]&UPPER_MASK)|(rand->mt[kk+1]&LOWER_MASK);
      rand->mt[kk] = rand->mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1];
    }
    y = (rand->mt[N-1]&UPPER_MASK)|(rand->mt[0]&LOWER_MASK);
    rand->mt[N-1] = rand->mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1];
    
    rand->mti = 0;
  }
  
  y = rand->mt[rand->mti++];
  y ^= TEMPERING_SHIFT_U(y);
  y ^= TEMPERING_SHIFT_S(y) & TEMPERING_MASK_B;
  y ^= TEMPERING_SHIFT_T(y) & TEMPERING_MASK_C;
  y ^= TEMPERING_SHIFT_L(y);
  
  return y; 
}

/* transform [0..2^32] -> [0..1] */
#define DANG_RAND_DOUBLE_TRANSFORM 2.3283064365386962890625e-10

double 
dang_rand_double (void)
{    
  /* We set all 52 bits after the point for this, not only the first
     32. Thats why we need two calls to g_rand_int */
  double retval;
  if (!inited)
    {
      dang_rand_set_seed (19650218UL);
      inited = TRUE;
    }
  retval = dang_rand_int () * DANG_RAND_DOUBLE_TRANSFORM;
  retval = (retval + dang_rand_int ()) * DANG_RAND_DOUBLE_TRANSFORM;

  /* The following might happen due to very bad rounding luck, but
   * actually this should be more than rare, we just try again then */
  if (retval >= 1.0) 
    return dang_rand_double ();

  return retval;
}

static DANG_SIMPLE_C_FUNC_DECLARE(do_rand)
{
  DANG_UNUSED (func_data);
  DANG_UNUSED (args);
  DANG_UNUSED (error);
  *(double*)rv_out = dang_rand_double ();
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE(do_cos)
{
  DANG_UNUSED (func_data);
  DANG_UNUSED (error);
  *(double*)rv_out = cos (*(double*)(args[0]));
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE(do_sin)
{
  DANG_UNUSED (func_data);
  DANG_UNUSED (error);
  *(double*)rv_out = sin (*(double*)(args[0]));
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE(do_tan)
{
  DANG_UNUSED (func_data);
  DANG_UNUSED (error);
  *(double*)rv_out = tan (*(double*)(args[0]));
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE(do_exp)
{
  DANG_UNUSED (func_data);
  DANG_UNUSED (error);
  *(double*)rv_out = exp (*(double*)(args[0]));
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE(do_log)
{
  DANG_UNUSED (func_data);
  DANG_UNUSED (error);
  *(double*)rv_out = log (*(double*)(args[0]));
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE(do_sqrt)
{
  DANG_UNUSED (func_data);
  DANG_UNUSED (error);
  *(double*)rv_out = sqrt (*(double*)(args[0]));
  return TRUE;
}

void
_dang_math_init (DangNamespace *ns)
{
  DangSignature *sig;
  DangFunctionParam params[2];
  double pi_double = M_PI;
  unsigned off;
  dang_namespace_add_simple_c_from_params (ns, "rand", do_rand,
                                           dang_value_type_double (),
                                           0);

  params[0].dir = DANG_FUNCTION_PARAM_IN;
  params[0].type = dang_value_type_double ();
  params[0].name = "arg";
  sig = dang_signature_new (dang_value_type_double (), 1, params);
  dang_namespace_add_simple_c (ns, "sin", sig, do_sin, NULL);
  dang_namespace_add_simple_c (ns, "cos", sig, do_cos, NULL);
  dang_namespace_add_simple_c (ns, "tan", sig, do_tan, NULL);
  dang_namespace_add_simple_c (ns, "exp", sig, do_exp, NULL);
  dang_namespace_add_simple_c (ns, "log", sig, do_log, NULL);
  dang_namespace_add_simple_c (ns, "sqrt", sig, do_sqrt, NULL);
  dang_namespace_add_const_global (ns, "pi", dang_value_type_double (),
                                   &pi_double, &off, NULL);
  dang_signature_unref (sig);
}
