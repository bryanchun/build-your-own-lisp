/**
 * Abstract Data Type for Expression
 * 
 */

/* Define Lispy Value struct */
typedef struct {
  int type;
  long num;
  /* Error and Symbol are Strings */
  char* err;
  char* sym;
  /* count and cell as pointer to recursively-defined lval pointers, interpreted as lists */
  /* the use of pointers is to allow variable length expressions */
  /* cell resembles cons cell */
  // TODO warning: incompatible pointer types assigning to 'struct lval *' from 'lval *' [-Wincompatible-pointer-types]
  // TODO note passing argument to parameter 'v' here (lval_del, lval_print)
  int count;
  struct lval** cell;
} lval;

/* Enum of type constants */
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };