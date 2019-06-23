/** @file functions.h
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

  /* Basic */
  long num;         /* Numerical value (if any) */
  char* err;        /* Error and Symbol are Strings */
  char* sym;        /* Symbol is redefined from functions to variable bindings */

  /* Function */
  lbuiltin builtin; /* Function as first class citizen, type function pointer */
                    /* if builtin != null then builtin_fun else user_def_fun */
  lenv* env;        /* Environment of bound arguments exclusively for this function */
  lval* formals;    /* Q-Expr of argument list */
  lval* body;       /* Q-Expr of function body */

  /* Expression */
  int count;        /* count and cell as pointer to recursively-defined lval pointers, interpreted as lists 
                        the use of pointers is to allow variable length expressions */
  lval** cell;      /* cell resembles cons cell */
};

/* Define Lipsy Environment to record name bindings */
struct lenv {
  lenv* par;
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
lval* lval_builtin(lbuiltin func);
lval* lval_lambda(lval* formals, lval* body);
lval* lval_term(void);

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
void lenv_def(lenv* e, lval* k, lval* v);
lenv* lenv_copy(lenv* e);

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

lval* lval_eval_sexpr(lenv* e, lval* v);
lval* lval_eval(lenv* e, lval* v);
lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);
lval* lval_join(lval* x, lval* y);

lval* lval_copy(lval* v);
int lval_eq(lval* x, lval* y);

lval* lval_call(lenv* e, lval* f, lval* a);

/**
 * Builtins
 * 
 * the primitives
 */

lval* builtin_lambda(lenv* e, lval* a);
lval* builtin_fun(lenv* e, lval* a);
lval* builtin_def(lenv* e, lval* a);
lval* builtin_put(lenv* e, lval* a);
lval* builtin_var(lenv* e, lval* a, char* func);
lval* builtin_exit(lenv* e, lval* a);
lval* builtin_env(lenv* e, lval* a);

lval* builtin_head(lenv* e, lval* a);
lval* builtin_tail(lenv* e, lval* a);
lval* builtin_list(lenv* e, lval* a);
lval* builtin_eval(lenv* e, lval* a);
lval* builtin_join(lenv* e, lval* a);
lval* builtin_cons(lenv* e, lval* a);
lval* builtin_len(lenv* e, lval* a);
lval* builtin_init(lenv* e, lval* a);

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

lval* builtin_if(lenv* e, lval* a);
lval* builtin_ord(lenv* e, lval* a, char* op);
lval* builtin_gt(lenv* e, lval* a);
lval* builtin_lt(lenv* e, lval* a);
lval* builtin_ge(lenv* e, lval* a);
lval* builtin_le(lenv* e, lval* a);
lval* builtin_cmp(lenv* e, lval* a, char* op);
lval* builtin_eq(lenv* e, lval* a);
lval* builtin_ne(lenv* e, lval* a);
// TODO: ||, &&, !
// TODO: true, false builtin const