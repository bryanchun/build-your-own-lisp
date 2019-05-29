/** @file variables.h
 *  @brief Lispy implementation declaration header.
 *
 *
 *  @author Bryan Chun (bryanchun)
 */



/**
 * Abstract Data Type for Expression
 * 
 */

/* Forward Declarations */
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* Lispy Value */
/* Enum of type constants */
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, 
       LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR, LVAL_TERM };

/* Error String Buffer Maximum Size */
const int ERROR_BUFFER_SIZE = 512;

/* Define lbuiltin new function type */
typedef lval* (*lbuiltin)(lenv*, lval*);

/* Define Lispy Value struct */
struct lval {
  /* Type is Enum */
  int type;
  /* Numerical value (if any) */
  long num;
  /* Error and Symbol are Strings */
  char* err;
  /* Symbol is redefined from functions to variable bindings */
  char* sym;
  /* Function as first class citizen, type function pointer */
  lbuiltin fun;

  /* count and cell as pointer to recursively-defined lval pointers, interpreted as lists */
  /* the use of pointers is to allow variable length expressions */
  /* cell resembles cons cell */
  int count;
  lval** cell;
};

/* Define Lipsy Environment to record name bindings */
struct lenv {
  int count;
  char** syms;
  lval** vals;
};



/**
 * lval Constructors and Destructor 
 * 
 */

lval* lval_num(long x);
lval* lval_err(char* fmt, ...);
lval* lval_sym(char* s);
lval* lval_sexpr(void);
lval* lval_qexpr(void);
lval* lval_fun(lbuiltin func);
char* ltype_name(int t);
void lval_del(lval* v);

/**
 * lenv Constructors and Destructor and Manipulators
 * 
 */
lenv* lenv_new(void);
void lenv_del(lenv* e);

lval* lenv_get(lenv* e, lval* k);
void lenv_put(lenv* e, lval* k, lval* v);

void lenv_add_builtin(lenv* e, char* name, lbuiltin func);
void lenv_add_builtins(lenv* e);

/**
 * Parser / Reader
 * 
 */

lval* lval_read_num(mpc_ast_t* t);
lval* lval_read(mpc_ast_t* t) ;


/**
 * Printers
 * 
 */

void lval_print(lval* v);
void lval_expr_print(lval* v, char open, char close);
void lval_println(lval* v);


/**
 * Evaluator / Manipulator
 * 
 */

lval* lval_add(lval* v, lval* x);

lval* lval_eval(lenv* e, lval* v);
lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);
lval* lval_join(lval* x, lval* y);

lval* lval_copy(lval* v);

/**
 * Builtins
 * 
 */

lval* builtin(lval* a, char* func);
lval* builtin_op(lenv* e, lval* a, char* op);
lval* builtin_add(lenv* e, lval* a);
lval* builtin_sub(lenv* e, lval* a);
lval* builtin_mul(lenv* e, lval* a);
lval* builtin_div(lenv* e, lval* a);
lval* builtin_mod(lenv* e, lval* a);
lval* builtin_exp(lenv* e, lval* a);
lval* builtin_max(lenv* e, lval* a);
lval* builtin_min(lenv* e, lval* a);

lval* builtin_head(lenv* e, lval* a);
lval* builtin_tail(lenv* e, lval* a);
lval* builtin_list(lenv* e, lval* a);
lval* builtin_eval(lenv* e, lval* a);
lval* builtin_join(lenv* e, lval* a);

lval* lval_expr_sexpr(lenv* e, lval* v);

lval* builtin_cons(lenv* e, lval* a);
lval* builtin_len(lenv* e, lval* a);
lval* builtin_init(lenv* e, lval* a);

lval* builtin_def(lenv* e, lval* a);
lval* builtin_exit(lenv* e, lval* a);
lval* builtin_env(lenv* e, lval* a);