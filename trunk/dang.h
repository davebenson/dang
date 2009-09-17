#include <inttypes.h>
#include <stddef.h>
#include <stdarg.h>
#include <alloca.h>
#include <errno.h>

typedef struct _DangCompileFlags DangCompileFlags;
typedef union _DangCompileResult DangCompileResult;
typedef struct _DangCompileContext DangCompileContext;
typedef struct _DangSignature DangSignature;
typedef struct _DangBuilder DangBuilder;
typedef union _DangFunction DangFunction;
typedef union _DangExpr DangExpr;
typedef struct _DangStep DangStep;
typedef struct _DangThread DangThread;
typedef struct _DangThreadStackFrame DangThreadStackFrame;
typedef struct _DangImports DangImports;
typedef struct _DangNamespace DangNamespace;
typedef struct _DangNamespaceSymbol DangNamespaceSymbol;
typedef struct _DangVarTable DangVarTable;
typedef struct _DangAnnotations DangAnnotations;
typedef struct _DangUntypedFunction DangUntypedFunction;
typedef struct _DangFunctionFamily DangFunctionFamily;
typedef struct _DangClosureFactory DangClosureFactory;
typedef struct _DangMatchQuery DangMatchQuery;
typedef struct _DangCodePosition DangCodePosition;
typedef struct _DangBuilderVariable DangBuilderVariable;
typedef struct _DangBuilderLabel DangBuilderLabel;
typedef union _DangInsn DangInsn;

typedef unsigned DangLabelId;
#define DANG_LABEL_ID_INVALID   ((DangLabelId)(-1))
typedef unsigned DangVarId;
#define DANG_VAR_ID_INVALID   ((DangVarId)(-1))

#include "dang-util.h"
#include "dang-code-position.h"
#include "dang-value.h"
#include "dang-expr.h"
#include "dang-function-param.h"
#include "dang-signature.h"
#include "dang-function.h"
#include "dang-insn.h"
#include "dang-compile.h"
#include "dang-imports.h"
#include "dang-builder.h"
#include "dang-function-family.h"
#include "dang-compile-context.h"
#include "dang-namespace.h"
#include "dang-enum.h"
#include "dang-struct.h"
#include "dang-union.h"
#include "dang-object.h"
#include "dang-tree.h"
#include "dang-thread.h"
#include "dang-template.h"
#include "dang-token.h"
#include "dang-tokenizer.h"
#include "dang-utf8.h"
#include "dang-var-table.h"
#include "dang-expr-annotations.h"
#include "dang-value-function.h"
#include "dang-untyped-function.h"
#include "dang-closure-factory.h"               /* needed public? */
#include "dang-metafunction.h"
#include "dang-module.h"
#include "dang-parser.h"
#include "dang-run-file.h"

/* addons */
#include "dang-tensor.h"

/* misc */
#include "dang-cleanup.h"
#include "dang-debug.h"


/* vi hackery.
 * by including this line, ^X^L will pick it up:
   DangError *error = NULL;
 */
