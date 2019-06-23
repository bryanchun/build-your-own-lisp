/** @file functions.c
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

#include "functions.h"

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

lval* lval_builtin(lbuiltin func) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->builtin = func;
  return v;
}

lval* lval_lambda(lval* formals, lval* body) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;

  /* User_def_fun: set builtin to NULL */
  v->builtin = NULL;
  /* Build new environment */
  v->env = lenv_new();
  /* Set passed formals and body */
  v->formals = formals;
  v->body = body;

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
    
    case LVAL_FUN:
      if (!v->builtin) {
        lenv_del(v->env);
        lval_del(v->formals);
        lval_del(v->body);
      }
      break;
  }
  /* Free itself always */
  free(v);
}

lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
  e->par = NULL;
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
  /* Do not delete parent envs */
}

lval* lenv_get(lenv* e, lval* k) {

  /* Lookup 'k' in 'syms' */
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      /* Return copy of the 'sym' from 'e' */
      return lval_copy(e->vals[i]);
    }
  }

  /* If there is parent scope, flood into enclosing scope */
  /* Otherwise cannot find */
  if (e->par) {
    return lenv_get(e->par, k);
  } else {
    return lval_err("unbound symbol '%s'", k->sym);
  }
}

void lenv_put(lenv* e, lval* k, lval* v) {
  /* Put variable defintion into deepest, local env */

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

void lenv_def(lenv* e, lval* k, lval* v) {
  /* Put variable defintion into shallowest, global env */

  /* Traverse to the parent env where there are no more grand-parents */
  while(e->par) { e = e->par; }
  /* Perform ordinary put in outermost 'e' */
  lenv_put(e, k, v);
}

lenv* lenv_copy(lenv* e) {
  lenv* n = malloc(sizeof(lenv));
  n->par = e->par;
  n->count = e->count;
  n->syms = malloc(sizeof(char*) * n->count);
  n->vals = malloc(sizeof(lval*) * n->count);
  for (int i = 0; i < n->count; i++) {
    n->syms[i] = malloc(strlen(e->syms[i]) + 1);
    strcpy(n->syms[i], e->syms[i]);
    n->vals[i] = lval_copy(e->vals[i]);
  }
  return n;
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
  lval* k = lval_sym(name);
  lval* f = lval_builtin(func);
  lenv_put(e, k, f);
  lval_del(k);  lval_del(f);
}

void lenv_add_builtins(lenv* e) {
  /* Prelude: Builtins to be added on start */

  /* Meta Functions */
  lenv_add_builtin(e, "\\", builtin_lambda);
  // lenv_add_builtin(e, "fun", builtin_fun);
  lenv_add_builtin(e, "def", builtin_def);
  lenv_add_builtin(e, "=", builtin_put);
  lenv_add_builtin(e, "exit", builtin_exit);
  lenv_add_builtin(e, "env", builtin_env);

  /* List Functions */
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);
  lenv_add_builtin(e, "cons", builtin_cons);
  lenv_add_builtin(e, "len", builtin_len);
  lenv_add_builtin(e, "init", builtin_init);

  /* Mathematical Functions */
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);
  lenv_add_builtin(e, "%", builtin_mod);
  lenv_add_builtin(e, "^", builtin_exp);
  lenv_add_builtin(e, "max", builtin_max);
  lenv_add_builtin(e, "min", builtin_min);

  /* Comparison Functions */
  lenv_add_builtin(e, "if", builtin_if);
  lenv_add_builtin(e, "==", builtin_eq);
  lenv_add_builtin(e, "!=", builtin_ne);
  lenv_add_builtin(e, ">",  builtin_gt);
  lenv_add_builtin(e, "<",  builtin_lt);
  lenv_add_builtin(e, ">=", builtin_ge);
  lenv_add_builtin(e, "<=", builtin_le);
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
    case LVAL_FUN:
      if (v->builtin) {
        printf("<builtin>");
      } else {
        putchar('(');
        printf("\\ "); lval_print(v->formals);
        putchar(' '); lval_print(v->body);
        putchar(')');
      }
      break;
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

lval* lval_eval_sexpr(lenv* e, lval* v) {
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
      /* 0-ary function support? */
      && v->cell[0]->type != LVAL_FUN
      )
        { return lval_take(v, 0); }

  /* Guard that first element is a Function */
  lval* f = lval_pop(v, 0);
  if (f->type != LVAL_FUN) {
    lval* err = lval_err(
      "S-expression does not start with Function. "
      "Got %s, Expected %s.",
      ltype_name(f->type), ltype_name(LVAL_FUN));
    lval_del(f);  lval_del(v);
    return err;
  }
  /* Postcondition that v is simple (op num num ...) */

  /* If so call function to get result */
  lval* result = lval_call(e, f, v);
  lval_del(f);
  return result;
}

lval* lval_eval(lenv* e, lval* v) {
  /* Evaluate S-Expressions by recursively calling */
  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
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
    case LVAL_NUM:  x->num = v->num;          break;
    case LVAL_FUN:
      if (v->builtin) {
        x->builtin = v->builtin;
      } else {
        x->builtin = NULL;
        x->env = lenv_copy(v->env);
        x->formals = lval_copy(v->formals);
        x->body = lval_copy(v->body);
      }
      break;

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

int lval_eq(lval* x, lval* y) {
  /* Type equality */
  if (x->type != y->type) { return 0; }

  /* Match type */
  switch (x->type) {
    case LVAL_NUM: return (x->num == y->num);
    case LVAL_ERR: return (strcmp(x->err, y->err) == 0);
    case LVAL_SYM: return (strcmp(x->err, y->err) == 0);

    case LVAL_FUN:
      if (x->builtin || y->builtin) {
        /* Builtin Functino reference comparison */
        return (x->builtin == y->builtin);
      } else {
        /* Match user-defined function formals and body */
        return lval_eq(x->formals, y->formals)
          && lval_eq(x->body, y->body);
      }

    case LVAL_QEXPR:
    case LVAL_SEXPR:
      /* Elementwise equal */
      if (x->count != y->count) { return 0; }
      for (int i = 0; i < x->count; i++) {
        if (!lval_eq(x->cell[i], y->cell[i])) { return 0; }
      }
      return 1;
      break;
  }
  return 0;
}

lval* lval_call(lenv* e, lval* f, lval* a) {

  /* Case builtin functions: direct application */
  if (f->builtin) { return f->builtin(e, a); }
  /* Making builtin functions not possible to be partially applied */

  /* Count arguments and match */
  int given = a->count;
  int total = f->formals->count;

  /* While there remains arguments being supplied */
  /* If under-supplied, this resembles Partial Application */
  while (a->count) {
    /* If no more formals to be applied to */
    if (f->formals->count == 0) {
      lval_del(a);
      return lval_err("Function passed too many arguments. "
                      "Got %i, Expected %i.", given, total);
    }

    /* Incrementally grab formal and argument pair */
    lval* sym = lval_pop(f->formals, 0);

    /* Variably-long Arguments: 'x & xs' */
    /* If 'sym' is variable argument operator '&' */
    if (strcmp(sym->sym, "&") == 0) {
      /* Ensure '&' is followed by one more symbol in 'formals' */
      if (f->formals->count != 1) {
        lval_del(a);
        return lval_err("Function format invalid. "
          "Symbol '&' not followed by single symbol.");
      }

      /* Bind that list of formals to the tail of arguments */
      lval* nsym = lval_pop(f->formals, 0);
      lenv_put(f->env, nsym, builtin_list(e, a));
      lval_del(sym);  lval_del(nsym);

      /* First pass in loop, can immediately break */
      break;
    }

    /* Incrementally grab formal and argument pair */
    lval* val = lval_pop(a, 0);

    /* Bind the pair in 'f''s env, then get deleted */
    lenv_put(f->env, sym, val);
    lval_del(sym);  lval_del(val);
  }
  /* Argument list is completely bound */
  lval_del(a);

  /* Variably-long Formals are not satuated with variably-long Arguments */
  /* Handles this case only: (\ {x & w} {...}) x' -> {...}[x = x', w = {}] */
  /* (\ {x y & w} {...}) x' -> \ {y & w} {...[x = x']} */
  /* (\ {x y z} {...} x') -> \ {y z} {...[x = x']} */
  if (f->formals->count > 0 &&
    strcmp(f->formals->cell[0]->sym, "&") == 0) {
    
    /* Guard that '& xs' is not followed */
    if (f->formals->count != 2) {
      return lval_err("Function format invalid. "
        "Symbol '&' not followed by single symbol");
    }
    /* Post condition:  */

    /* Pop and delete redundant '&' */
    lval_del(lval_pop(f->formals, 0));

    /* Pop next symbol and create an empty list */
    lval* sym = lval_pop(f->formals, 0);
    lval* val = lval_qexpr();

    /* Bind this pair in env and delete */
    lenv_put(f->env, sym, val);
    lval_del(sym);  lval_del(val);
  }

  /* If formals are all bound, do evaluate */
  if (f->formals->count == 0) {
    /* Set the parent env, the largest scope for evaluation, as 'e', so as to define most variables */
    f->env->par = e;
    return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
  } else {
    /* Otherwise return partially applied function */
    return lval_copy(f);
  }

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

/* Static type checker for function arguments */
#define LASSERT_TYPE(func, args, index, expect) \
  LASSERT(args, args->cell[index]->type == expect, \
    "Function '%s' passed incorrect type for argument %i. " \
    "Got %s, Expected %s.", \
    func, index, ltype_name(args->cell[index]->type), ltype_name(expect))

#define LASSERT_NUM(func, args, num) \
  LASSERT(args, args->count == num, \
    "Function '%s' passed incorrect number of arguments. " \
    "Got %i, Expected %i.", \
    func, args->count, num)

#define LNONEMPTY(args, err) \
  LASSERT(args, args->count > 0, err)
#define LARGN(args, n, err) \
  LASSERT(args, args->count == n, err)


lval* builtin_head(lenv* e, lval* a) {
  /* Guards required conditions and return error if contradicts */
  LASSERT_NUM("head", a, 1);
  LASSERT_TYPE("head", a, 0, LVAL_QEXPR);

  /* Extract singleton/head */
  lval* v = lval_take(a, 0);

  /* Extract head by removing all elements after head */
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }

  return v;
}

lval* builtin_tail(lenv* e, lval* a) {
  /* Guards required conditions and return error if contradicts */
  LASSERT_NUM("tail", a, 1);
  LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);

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
  LASSERT_NUM("eval", a, 1);
  LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);
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
    LASSERT_TYPE("join", a, i, LVAL_QEXPR);
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
  LASSERT_NUM("cons", a, 2);

  lval* x = a->cell[0];
  lval* y = a->cell[1];
  LASSERT(a, x->type == LVAL_NUM
        || x->type == LVAL_SEXPR
        || x->type == LVAL_QEXPR,
        "Function 'cons' passed incorrect type in the first argument! "
        "Got %s, Expected %s", x->type, "Number/S-Expression/Q-Expression");
  LASSERT_TYPE("cons", a, 1, LVAL_QEXPR);

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
  LASSERT_NUM("len", a, 1);
  LASSERT_TYPE("len", a, 0, LVAL_QEXPR);
  lval* x = a->cell[0];

  return lval_num(x->count);
}

lval* builtin_init(lenv* e, lval* a) {
  /* Guard types */
  /* accepts single Qexpr list */
  LASSERT_NUM("init", a, 1);
  LASSERT_TYPE("init", a, 0, LVAL_QEXPR);
  lval* x = a->cell[0];

  /* Extract pointer to last element */
  /* Shift and reallocate cell */
  /* Delete returned the last-element pointer*/
  lval_del(lval_pop(x, x->count - 1));
  return x;
}

lval* builtin_lambda(lenv* e, lval* a) {
  LASSERT_NUM("\\", a, 2);
  LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
  LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

  for (int i = 0; i < a->cell[0]->count; i++) {
    LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
      "Cannot define non-symbol. Got %s, Expected %s.",
      ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
  }

  lval* formals = lval_pop(a, 0);
  lval* body = lval_pop(a, 0);
  lval_del(a);

  return lval_lambda(formals, body);
}

/* lval* builtin_fun(lenv* e, lval* a) {
  lval* lambda = lval_qexpr();

  lval* formals = lval_qexpr();
  
  lval* body = lval_qexpr();
  lval* inner_body = 
  lval* inner_def = builtin_def(e, )

  lval_add(lambda, formals);
  lval_add(lambda, body);
  return builtin_def(e, lval_lambda(e, lambda));
} */

lval* builtin_def(lenv* e, lval* a) {
  return builtin_var(e, a, "def");
}

lval* builtin_put(lenv* e, lval* a) {
  return builtin_var(e, a, "=");
}

lval* builtin_var(lenv* e, lval* a, char* func) {

  /* Guard first cell of 'a' as List (Qexpr) */
  LASSERT_TYPE(func, a, 0, LVAL_QEXPR);

  /* Get the list */
  lval* syms = a->cell[0];

  /* Guard the first cell 'syms' is arg List (Qexpr) of Symbols */
  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
      "Function '%s' cannot define non-symbol. "
      "Got %s, Expected %s.", func,
      ltype_name(syms->cell[i]->type),
      ltype_name(LVAL_SYM));
  }

  /* Guard that number of syms matches number of following values */
  LASSERT(a, syms->count == a->count - 1, 
    "Function '%s' cannot define incorrect number of values to symbols. "
    "Got %i, Expected %i.", func, a->count - 1, syms->count);

  /* All clear then assign copies (done by 'lenv_put') of values to symbols */
  for (int i = 0; i < syms->count; i++) {
    if (strcmp(func, "def") == 0) {
      /* i'th sym corresponds to (i+1)'th cell, 1st cell is 'sym' */
      lenv_def(e, syms->cell[i], a->cell[i+1]);
    }
    if (strcmp(func, "=") == 0) {
      lenv_put(e, syms->cell[i], a->cell[i+1]);
    }
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

lval* builtin_if(lenv* e, lval* a) {
  LASSERT_NUM("if", a, 3);
  LASSERT_TYPE("if", a, 0, LVAL_NUM);
  LASSERT_TYPE("if", a, 1, LVAL_QEXPR);
  LASSERT_TYPE("if", a, 2, LVAL_QEXPR);

  /* Mark yes and no branch expressions as executable SExpr */
  a->cell[1]->type = LVAL_SEXPR;
  a->cell[2]->type = LVAL_SEXPR;

  /* Only execute relevant branch */
  lval* x;
  /* Condition shall be SExpr that quickly evaluates to LVAL_NUM  , cannot be QExpr */
  if (a->cell[0]->num) {
    x = lval_eval(e, lval_pop(a, 1));
  } else {
    x = lval_eval(e, lval_pop(a, 2));
  }

  lval_del(a);
  return x;
}

lval* builtin_gt(lenv* e, lval* a) {
  return builtin_ord(e, a, ">");
}

lval* builtin_lt(lenv* e, lval* a) {
  return builtin_ord(e, a, "<");
}

lval* builtin_ge(lenv* e, lval* a) {
  return builtin_ord(e, a, ">=");
}

lval* builtin_le(lenv* e, lval* a) {
  return builtin_ord(e, a, "<=");
}


lval* builtin_ord(lenv* e, lval* a, char* op) {
  /* Representing 0 == False, otherwise True */
  /* Supports Number ordering for now */

  LASSERT_NUM(op, a, 2);
  LASSERT_TYPE(op, a, 0, LVAL_NUM);
  LASSERT_TYPE(op, a, 1, LVAL_NUM);

  int r;
  if (strcmp(op, ">") == 0) {
    r = (a->cell[0]->num > a->cell[1]->num);
  }
  if (strcmp(op, "<") == 0) {
    r = (a->cell[0]->num < a->cell[1]->num);
  }
  if (strcmp(op, ">=") == 0) {
    r = (a->cell[0]->num >= a->cell[1]->num);
  }
  if (strcmp(op, "<=") == 0) {
    r = (a->cell[0]->num <= a->cell[1]->num);
  }
  lval_del(a);
  return lval_num(r);
}

lval* builtin_cmp(lenv* e, lval* a, char* op) {
  LASSERT_NUM(op, a, 2);
  int r;
  if (strcmp(op, "==") == 0) {
    r = lval_eq(a->cell[0], a->cell[1]);
  }
  if (strcmp(op, "!=") == 0) {
    r = !lval_eq(a->cell[0], a->cell[1]);
  }
  lval_del(a);
  return lval_num(r);
}

lval* builtin_eq(lenv* e, lval* a) {
  return builtin_cmp(e, a, "==");
}

lval* builtin_ne(lenv* e, lval* a) {
  return builtin_cmp(e, a, "!=");
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
     symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&^]+/ ;                      \
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