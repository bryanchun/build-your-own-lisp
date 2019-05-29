/** @file variables.c
 *  @brief Lispy implementation source.
 *
 *
 *  @author Bryan Chun (bryanchun)
 */

#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"
#include <math.h>

#ifdef _WIN32  // defined(__unix__) || defined(__APPLE__) || defined(__MACH__) || defined(_WIN64)
#include <string>
/* Declare input buffer */
static char buffer[2048];

char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);

  /* copy from buffer */
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

/* Dummy useless add_history for completeness */
void add_history(char* unused) {}
#else
#include <editline/readline.h>
#endif

#include "variables.h"

/**
 * Constructors and Destructor 
 * 
 */

/* Constructor (generator) for number-type lval */
lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

/* Constructor (generator) for error-type lval */
/* Error as first class citizen, for expression and error propagation */
lval* lval_err(char* fmt, ...) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;

  /* Create and Initialise a 'va_list' */
  va_list va;
  va_start(va, fmt);

  /* Allocate a default maximum (512) bytes of error string space */
  v->err = malloc(ERROR_BUFFER_SIZE);

  /* printf the error */
  vsnprintf(v->err, ERROR_BUFFER_SIZE - 1, fmt, va);

  /* Reallocate error string to required number of bytes */
  v->err = realloc(v->err, strlen(v->err) + 1);

  /* Clean up 'va_list' */
  va_end(va);

  return v;
}

/* Constructor (generator) for symbol-type lval */
lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);   // for accommodating terminating '\0'
  // BUG: v->err gives rise to freeing un-malloced memory
  strcpy(v->sym, s);
  return v;
}

/* Constructor (generator) for sexpr-type lval */
lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_fun(lbuiltin func) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->fun = func;
  return v;
}

lval* lval_term(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_TERM;
  return v;
}

char* ltype_name(int t) {
  /* Convert Enum LVAL types to proper strings */
  switch (t) {
    case LVAL_FUN:    return "Function";
    case LVAL_NUM:    return "Number";
    case LVAL_ERR:    return "Error";
    case LVAL_SYM:    return "Symbol";
    case LVAL_SEXPR:  return "S-Expression";
    case LVAL_QEXPR:  return "Q-Expression";
    default:          return "Unknown";
  }
}

/* Free memory of each subtypes of lval */
void lval_del(lval* v) {
  switch (v->type) {
    case LVAL_NUM:    break;

    /* Free the error and symbol string memories */
    case LVAL_ERR:    free(v->err);   break;
    case LVAL_SYM:    free(v->sym);   break;

    /* For both SEXPR and QEXPR delete all elements inside */
    case LVAL_SEXPR:
    case LVAL_QEXPR:
      for (int i = 0; i < v->count; i++) {
        /* Recursively lval_del memories from all s/q-expressions */
        lval_del(v->cell[i]);
      }
      free(v->cell);
      break;
    
    case LVAL_FUN:    break;
  }
  /* Free itself always */
  free(v);
}

lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

void lenv_del(lenv* e) {
  /* Free syms and lval_del vals recursively */
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
}

lval* lenv_get(lenv* e, lval* k) {

  /* Lookup 'k' in 'syms' */
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      /* Return copy of the 'sym' from 'e' */
      return lval_copy(e->vals[i]);
    }
  }
  return lval_err("unbound symbol '%s'", k->sym);
}

void lenv_put(lenv* e, lval* k, lval* v) {

  /* Lookup if 'k' in 'syms' */
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      /* If 'sym' is found, delete 'e''s copy, write in supplied 'k''s copy */
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      return;
    }
  }

  /* Otherwise allocate new memory for new entry */
  e->count++;
  e->vals = realloc(e->vals, sizeof(lval*) * e->count);
  e->syms = realloc(e->syms, sizeof(char*) * e->count);

  /* Copy contents to newly allocated memory */
  e->vals[e->count - 1] = lval_copy(v);
  e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
  strcpy(e->syms[e->count - 1], k->sym);
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
  lval* k = lval_sym(name);
  lval* f = lval_fun(func);
  lenv_put(e, k, f);
  lval_del(k);  lval_del(f);
}

void lenv_add_builtins(lenv* e) {
  /* Prelude: Builtins to be added on start */

  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);
  lenv_add_builtin(e, "%", builtin_mod);
  lenv_add_builtin(e, "^", builtin_exp);
  lenv_add_builtin(e, "max", builtin_max);
  lenv_add_builtin(e, "min", builtin_min);
  
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);
  lenv_add_builtin(e, "cons", builtin_cons);
  lenv_add_builtin(e, "len", builtin_len);
  lenv_add_builtin(e, "init", builtin_init);

  lenv_add_builtin(e, "def", builtin_def);
  lenv_add_builtin(e, "exit", builtin_exit);
  lenv_add_builtin(e, "env", builtin_env);
}

/**
 * Parser / Reader
 * 
 */

lval* lval_read_num(mpc_ast_t* t) {
  /* Safely convert string to number */
  errno = 0;    // external flag for strtol
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE
    ? lval_num(x)
    : lval_err("invalid number");
}

lval* lval_add(lval* v, lval* x) {
  /* Effect: Preserve 'v' and 'x' without deallocation */
  v->count++;
  /* Increment memory on-demand by realloc */
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count - 1] = x;
  return v;
}

lval* lval_read(mpc_ast_t* t) {
  /* Base case for Number and Symbol */
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  /* If root (>) or sexpr then create empty list for later accumulation */
  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
  if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
  if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }

  /* Otherwise if the following expressions are valid then add to this list */
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0)  { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0)  { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0)  { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0)  { continue; }
    if (strcmp(t->children[i]->tag, "regex")  == 0)  { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

/**
 * Printers
 * 
 */

/* Print an s-expression */
/* Done recursively when lval_print calls lval_expr_print again */
void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    /* Print each lval contained within */
    lval_print(v->cell[i]);

    /* Print seperating space only for non-last lvals */
    if (i != (v->count - 1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

/* Print an lval value */
void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_NUM:    printf("%li", v->num);            break;
    case LVAL_ERR:    printf("Error: %s", v->err);      break;
    case LVAL_SYM:    printf("%s", v->sym);             break;
    case LVAL_SEXPR:  lval_expr_print(v, '(', ')');     break;
    case LVAL_QEXPR:  lval_expr_print(v, '{', '}');     break;
    case LVAL_FUN:    printf("<function>");             break;
    case LVAL_TERM:   printf("<termination>");          break;
  }
}

/* Print an lval value followed by a newline */
void lval_println(lval* v) {
  lval_print(v);
  putchar('\n');
}

/**
 * Evaluator / Manipulator
 * 
 */

lval* lval_expr_sexpr(lenv* e, lval* v) {
  /* Transform of e*v -> v' */

  /* Evaluate children */
  for (int i = 0; i < v->count; i++) {
    /* Element-wise transformation */
    v->cell[i] = lval_eval(e, v->cell[i]);
  }

  /* Error checking */
  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }

  /* Empty Expression */
  if (v->count == 0)  { return v; }
  /* Single Expression */
  if (v->count == 1 
      /* Unitary function support? */
      && v->cell[0]->type != LVAL_FUN
      )
        { return lval_take(v, 0); }

  /* Guard that first element is a Function */
  lval* f = lval_pop(v, 0);
  if (f->type != LVAL_FUN) {
    lval_del(f);
    lval_del(v);
    return lval_err("S-expression does not start with Function. Got %s.", f->sym);
  }
  /* Postcondition that v is simple (op num num ...) */

  /* If so call function to get result */
  lval* result = f->fun(e, v);
  lval_del(f);
  return result;
}

lval* lval_eval(lenv* e, lval* v) {
  /* Evaluate S-Expressions by recursively calling */
  if (v->type == LVAL_SEXPR) { return lval_expr_sexpr(e, v); }
  if (v->type == LVAL_SYM) {
    /* Symbols become an expression to be evaluated by the environment */
    lval* x = lenv_get(e, v);
    lval_del(v);
    return x;
  }
  /* Otherwise all other subtypes (Number, Error, Function) are unchanged, return identity */
  return v;
}

lval* lval_pop(lval* v, int i) {
  /* Effect: Preserve 'v' and 'v->cell[i]' without deallocation  */
  /* Get element at i'th index */
  lval* x = v->cell[i];

  /* Shift memory layout to left to overwrite i'th element */
  /* At a, use memory starting at b, for c long */
  memmove(&v->cell[i], &v->cell[i+1],
    sizeof(lval*) * (v->count-i-1));  

  /* Decrement count record */
  v->count--;

  /* Reallocate memory for cell */
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}

lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

lval* lval_join(lval* x, lval* y) {
  /* Join between two lists of cells */
  /* For each cell in 'y', add it to 'x' */
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  /* Delete 'y' upon consumption and return 'x' */
  lval_del(y);
  return x;
}

lval* lval_copy(lval* v) {
  /* Allocate new memory */
  lval* x = malloc(sizeof(lval));

  /* Copy attributes */
  x->type = v->type;

  switch (v->type) {

    /* Copy Numbers (literals) and Functions (pointers) directly */
    case LVAL_NUM:  x->num = v->num;  break;
    case LVAL_FUN:  x->fun = v->fun;  break;

    /* Copy Errors and Symbols (Strings) by allocating the right amount of memory */
    /* for err and sym */
    case LVAL_ERR:
      x->err = malloc(strlen(v->err) + 1);
      strcpy(x->err, v->err);
      break;
    case LVAL_SYM:
      x->sym = malloc(strlen(v->sym) + 1);
      strcpy(x->sym, v->sym);
      break;

    /* Copy Sexpr and Qexpr (Lists) by copying each sub-expression recursively */
    case LVAL_SEXPR:
    case LVAL_QEXPR:
      x->count = v->count;
      x->cell = malloc(sizeof(lval*) * x->count);
      for (int i = 0; i < x->count; i++) {
        x->cell[i] = lval_copy(v->cell[i]);
      }
      break;
  }

  return x;
}

/**
 * Builtins
 *  
 * Functions considered primitives
 * As of Chapter 11, these become prelude-defined names in the environment
 * Instead of primitive symbols
 */

lval* builtin_op(lenv* e, lval* a, char* op) {
  /* Apply op on variably long lval a */
  /* Upgrade from 2-argument eval_op */

  /* Ensure all arguments are numbers */
  for (int i = 0; i < a->count; i++) {
    lval* c = a->cell[i];
    if (c->type != LVAL_NUM) {
      /* Abort evaluation by shortcircuiting to deallocating memory */
      lval_del(a);
      return lval_err("Cannot operate on non-number!");
    }
  }

  /* Pop first argument */
  lval* x = lval_pop(a, 0);

  /* Unitary negation */
  if ((strcmp(op, "-") == 0) && (a->count == 0)) {
    /* If op is minus and after two pops there remain no more elements, i.e. a 2-element cell */
    x->num = -x->num;
  }

  /* (+ 9) */
  /* (- 1) will skip this block */

  /* While there is still elements on the list */
  while (a->count > 0) {

    /* Pop the next element */
    lval* y = lval_pop(a, 0);

    if (strcmp(op, "-") == 0)   { x->num -= y->num; }
    if (strcmp(op, "+") == 0)   { x->num += y->num; }   // '+' is converted to int
    if (strcmp(op, "*") == 0)   { x->num *= y->num; }
    if (strcmp(op, "/") == 0)   {
      /* Implement safe division */
      if (y->num == 0) {
        lval_del(x); lval_del(y);
        x = lval_err("Division By Zero!");              // Error as value
        break;
      }
      x->num /= y->num;
    }
    if (strcmp(op, "%") == 0)   { x->num %= y->num; }
    if (strcmp(op, "^") == 0)   { x->num = pow(x->num, y->num); }
    if (strcmp(op, "min") == 0) { x->num = fmin(x->num, y->num); }
    if (strcmp(op, "max") == 0) { x->num = fmax(x->num, y->num); }

    lval_del(y);
  }

  /* Deallocate the container */
  lval_del(a);
  /* Return the accumulator */
  return x;
}

lval* builtin_add(lenv* e, lval* a) {
  return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
  return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
  return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
  return builtin_op(e, a, "/");
}

lval* builtin_mod(lenv* e, lval* a) {
  return builtin_op(e, a, "%");
}

lval* builtin_exp(lenv* e, lval* a) {
  return builtin_op(e, a, "^");
}

lval* builtin_max(lenv* e, lval* a) {
  return builtin_op(e, a, "max");
}

lval* builtin_min(lenv* e, lval* a) {
  return builtin_op(e, a, "min");
}

/* Marco copy-and-paste into applied code, hence will short-circuit return the corresponding function */
#define LASSERT(args, cond, fmt, ...) \
  if (!(cond)) { \
    lval* err = lval_err(fmt, ##__VA_ARGS__); \
    lval_del(args); \
    return err; \
  }

#define LNONEMPTY(args, err) \
  LASSERT(args, args->count > 0, err)
#define LARGN(args, n, err) \
  LASSERT(args, args->count == n, err)


lval* builtin_head(lenv* e, lval* a) {
  /* Guards required conditions and return error if contradicts */
  LASSERT(a, a->count == 1, "Function 'head' passed too many arguments! Got %i, Expected %i.", a->count, 1);
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'head' passed incorrect type! Got %s, Expected %s", a->cell[0]->type, "Q-Expression");
  LASSERT(a, a->cell[0]->count != 0, "Function 'head' passed empty Qexpr! Got %i, Expected %i.", 0, 1);    // unsafe 'head'

  /* Extract singleton/head */
  lval* v = lval_take(a, 0);

  /* Extract head by removing all elements after head */
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }

  return v;
}

lval* builtin_tail(lenv* e, lval* a) {
  /* Guards required conditions and return error if contradicts */
  LASSERT(a, a->count == 1, "Function 'tail' passed too many arguments! Got %i, Expected %i.", a->count, 1);
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'tail' passed incorrect type! Got %s, Expected %s", a->cell[0]->type, "Q-Expression");
  LASSERT(a, a->cell[0]->count != 0, "Function 'tail' passed empty Qexpr! Got %i, Expected %i.", 0, 1);    // unsafe 'tail'

  /* Extract singleton/head */
  lval* v = lval_take(a, 0);

  /* Chop off the head */
  lval_del(lval_pop(v, 0));

  return v;
}

lval* builtin_list(lenv* e, lval* a) {
  /* Changing type flag Sexpr to Qexpr suffices */
  a->type = LVAL_QEXPR;
  return a;
}

lval* builtin_eval(lenv* e, lval* a) {
  /* Guards required conditions and return error if contradicts */
  LASSERT(a, a->count == 1, "Function 'eval' passed too many arguments! Got %i, Expected %i.", a->count, 1);
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'eval' passed incorrect type! Got %s, Expected %s", a->cell[0]->type, "Q-Expression");
  /* Evaluates empty list */

  lval* x = lval_take(a, 0);
  /* Convert to list */
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

lval* builtin_join(lenv* e, lval* a) {
  /* Flattens Qexpr of Qexprs */
  /* Guards required all cells in 'a' are Qexpr */
  for (int i = 0; i < a->count; i++) {
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR, "Function 'join' passed incorrect type! Got %s, Expected %s", a->cell[i]->type, "Q-Expression");
  }

  /* Get head for accumulator */
  lval* x = lval_pop(a, 0);

  while (a->count) {
    /* Pass to 'lval_join' to also handle inner lists and delete each element */
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

lval* builtin_cons(lenv* e, lval* a) {
  /* Guard types */
  LASSERT(a, a->count == 2, "Function 'cons' passed too few or many arguments! Got %i, Expected %i.", a->count, 2);
  lval* x = a->cell[0];
  lval* y = a->cell[1];
  LASSERT(a, x->type == LVAL_NUM
        || x->type == LVAL_SEXPR
        || x->type == LVAL_QEXPR, "Function 'cons' passed incorrect type in the first argument! Got %s, Expected %s", x->type, "Number/S-Expression/Q-Expression");
  LASSERT(a, y->type == LVAL_QEXPR, "Function 'cons' passed incorrect type in the second argument! Got %s, Expected %s", y->type, "Q-Expression");

  /* Create a new list and append 'x' */
  lval* xs = lval_qexpr();
  xs = lval_add(xs, x);

  /* Join new singleton list with list 'y' */
  xs = lval_join(xs, y);
  
  return xs;
}

lval* builtin_len(lenv* e, lval* a) {
  /* Guard types */
  /* accepts single Qexpr list */
  /* Pure function */
  LASSERT(a, a->count == 1, "Function 'len' passed too many arguments! Got %i, Expected %i.", a->count, 1);
  lval* x = a->cell[0];
  LASSERT(a, x->type == LVAL_QEXPR, "Function 'len' passed incorrect type! Got %s, Expected %s", a->cell[0]->type, "Q-Expression");

  return lval_num(x->count);
}

lval* builtin_init(lenv* e, lval* a) {
  /* Guard types */
  /* accepts single Qexpr list */
  LASSERT(a, a->count == 1, "Function 'init' passed too many arguments! Got %i, Expected %i.", a->count, 1);
  lval* x = a->cell[0];
  LASSERT(a, x->type == LVAL_QEXPR, "Function 'init' passed incorrect type! Got %s, Expected %s", x->type, "Q-Expression");

  /* Extract pointer to last element */
  /* Shift and reallocate cell */
  /* Delete returned the last-element pointer*/
  lval_del(lval_pop(x, x->count - 1));
  return x;
}

lval* builtin_def(lenv* e, lval* a) {
  /* Guard first cell of 'a' as List (Qexpr) */
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'def' passed incorrect type! Got %s, Expected %s", a->cell[0]->type, "Q-Expression");

  /* Get the list */
  lval* syms = a->cell[0];

  /* Guard first cell of 'syms' is arg List (Qexpr) of Symbols */
  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, syms->cell[i]->type == LVAL_SYM, "Function 'def' cannot define non-symbol");
  }

  /* Guard that number of syms matches number of following values */
  LASSERT(a, syms->count == a->count - 1, "Function 'def' cannot define incorrect number of values to symbols. Got %i, Expected %i.", a->count - 1, syms->count);

  /* All clear then assign copies (done by 'lenv_put') of values to symbols */
  for (int i = 0; i < syms->count; i++) {
    lenv_put(e, syms->cell[i], a->cell[i+1]);
  }

  /* Delete 'a' and return unit */
  lval_del(a);
  return lval_sexpr();
}

lval* builtin_exit(lenv* e, lval* a) {
  lval_del(a);
  return lval_term();
}

lval* builtin_env(lenv* e, lval* a) {
  /* Prints out all defined values in 'e' */
  for (int i = 0; i < e->count; i++) {
    printf("%s \t", e->syms[i]);
    lval_print(e->vals[i]);
    putchar('\n');
  }
  return lval_sexpr();
}

/**
 * Main loop
 * 
 */

int main(int argc, char** argv) {

  /* Define some parsers */
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr = mpc_new("sexpr");
  mpc_parser_t* Qexpr = mpc_new("qexpr");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Lispy = mpc_new("lispy");
  
  /* Define the language */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                                   \
     number   : /-?[0-9]+/ ;                                            \
     symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;                      \
     sexpr    : '(' <expr>* ')' ;                                       \
     qexpr    : '{' <expr>* '}' ;                                       \
     expr     : <number> | <symbol> | <sexpr> | <qexpr>;                \
     lispy    : /^/ <expr>* /$/ ;                                       \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    // TODO
    // Double: /-?0\.[0-9]+/ | /-?[1-9]+\.[0-9]+/
    // unitary negate
    // clisp> (+ 2 3)
    // number clisp> 2
    // union vs struct

  /* Print Lisp information */
  puts("Lispy version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  lenv* e = lenv_new();
  lenv_add_builtins(e);

  /* In a loop */
  int is_running = 1;
  while (is_running) {

    /* Output our prompt */
    // fputs("lispy> ", stdout);
    /* Read a line of user input of maximum size 2048 */
    // fgets(input, 2048, stdin);
    char* input = readline("clisp> ");
    /* Done in one go */

    /* Add input to history */
    add_history(input);

    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      /* On Success -> Evaluate the AST */
      lval* x = lval_eval(e, lval_read(r.output));         // Composition
      lval_println(x);

      if (x->type == LVAL_TERM) {
        /* Signals termination by user */
        is_running = 0;
      }

      lval_del(x);
      // mpc_ast_print(r.output);
      mpc_ast_delete(r.output);
    } else {
      /* On Error -> Print the error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    /* Echo back to user */
    // printf("No you're a %s\n", input);

    /* Free retrieved input at dynamic memory */
    free(input);
  }
  lenv_del(e);

  /* Undefine and delete allocated parsers */
  /* aka clean up on exit */
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  return 0;
}

/*
json = { 
  [a-zA-Z]+: ([0-9a-zA-Z] | <json>) | [a-zA-Z]+: ([0-9a-zA-Z] | <json>) (, [a-zA-Z]+: ([0-9a-zA-Z] | <json>))+
} 
*/