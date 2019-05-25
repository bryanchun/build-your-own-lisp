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

#include "s-expression.h"

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
lval* lval_err(char* e) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(e) + 1);
  strcpy(v->err, e);
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
  }
  /* Free itself always */
  free(v);
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
void lval_print(lval* v);
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
    case LVAL_NUM:    printf("%li", v->num);         break;
    case LVAL_ERR:    printf("Error: %s", v->err);   break;
    case LVAL_SYM:    printf("%s", v->sym);          break;
    case LVAL_SEXPR:  lval_expr_print(v, '(', ')');  break;
    case LVAL_QEXPR:  lval_expr_print(v, '{', '}');  break;
  }
}

/* Print an lval value followed by a newline */
void lval_println(lval* v) {
  lval_print(v);
  putchar('\n');
}

/**
 * Evaluator
 * 
 */

lval* lval_eval(lval* v);
lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);
lval* lval_join(lval* x, lval* y);

lval* builtin(lval* a, char* func);
lval* builtin_op(lval* a, char* op);
lval* builtin_head(lval* a);
lval* builtin_tail(lval* a);
lval* builtin_list(lval* a);
lval* builtin_eval(lval* a);
lval* builtin_join(lval* a);

lval* builtin_cons(lval* a);
lval* builtin_len(lval* a);
lval* builtin_init(lval* a);

lval* lval_expr_sexpr(lval* v) {
  /* Transform of v -> v' */

  /* Evaluate children */
  for (int i = 0; i < v->count; i++) {
    /* Element-wise transformation */
    v->cell[i] = lval_eval(v->cell[i]);
  }

  /* Error checking */
  for (int i = 0; i < v->count; i++) {
    lval* c = v->cell[i];
    if (c->type == LVAL_ERR) { return lval_take(v, i); }
  }

  /* Empty Expression */
  if (v->count == 0)  { return v; }

  /* Single Expression */
  if (v->count == 1)  { return lval_take(v, 0); }

  /* Guard that first element is a Symbol */
  lval* f = lval_pop(v, 0);
  if (f->type != LVAL_SYM) {
    lval_del(f);
    lval_del(v);
    return lval_err("S-expression does not start with Symbol");
  }
  /* Postcondition that v is simple (op num num ...) */

  /* Call builtin with operator */
  lval* result = builtin(v, f->sym);
  lval_del(f);
  return result;
}

lval* lval_eval(lval* v) {
  /* Evaluate S-Expressions by recursively calling */
  if (v->type == LVAL_SEXPR) { return lval_expr_sexpr(v); }
  /* Otherwise all other subtypes (Number, Symbol, Error) are unchanged, return identity */
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

/* Functions considered primitives */

lval* builtin_op(lval* a, char* op) {
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
  if ((strcmp(op, "-") == 0) && a->count == 0) {
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

/* Marco copy-and-paste into applied code, hence will short-circuit return the corresponding function */
#define LASSERT(args, cond, err) \
  if (!(cond)) { lval_del(args); return lval_err(err); }
#define LNONEMPTY(args, err) \
  LASSERT(args, args->count > 0, err)
#define LARGN(args, n, err) \
  LASSERT(args, args->count == n, err)

lval* builtin_head(lval* a) {
  /* Guards required conditions and return error if contradicts */
  LASSERT(a, a->count == 1, "Function 'head' passed too many arguments!");
  lval* c = a->cell[0];
  LASSERT(a, c->type == LVAL_QEXPR, "Function 'head' passed incorrect, non-Qexpr type!");
  LASSERT(a, c->count != 0, "Function 'head' passed empty Qexpr!");    // unsafe 'head'

  /* Extract singleton/head */
  lval* v = lval_take(a, 0);

  /* Extract head by removing all elements after head */
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }

  return v;
}

lval* builtin_tail(lval* a) {
  /* Guards required conditions and return error if contradicts */
  LASSERT(a, a->count == 1, "Function 'tail' passed too many arguments!");
  lval* c = a->cell[0];
  LASSERT(a, c->type == LVAL_QEXPR, "Function 'tail' passed incorrect, non-Qexpr type!");
  LASSERT(a, c->count != 0, "Function 'tail' passed empty Qexpr!");    // unsafe 'tail'

  /* Extract singleton/head */
  lval* v = lval_take(a, 0);

  /* Chop off the head */
  lval_del(lval_pop(v, 0));

  return v;
}

lval* builtin_list(lval* a) {
  /* Changing type flag Sexpr to Qexpr suffices */
  a->type = LVAL_QEXPR;
  return a;
}

lval* builtin_eval(lval* a) {
  /* Guards required conditions and return error if contradicts */
  LASSERT(a, a->count == 1, "Function 'eval' passed too many arguments!");
  lval* c = a->cell[0];
  LASSERT(a, c->type == LVAL_QEXPR, "Function 'eval' passed incorrect, non-Qexpr type!");
  /* Evaluates empty list */

  lval* x = lval_take(a, 0);
  /* Convert to list */
  x->type = LVAL_SEXPR;
  return lval_eval(x);
}

lval* builtin_join(lval* a) {
  /* Flattens Qexpr of Qexprs */
  /* Guards required all cells in 'a' are Qexpr */
  for (int i = 0; i < a->count; i++) {
    lval* c = a->cell[i];
    LASSERT(a, c->type == LVAL_QEXPR, "Function 'join' passed incorrect, non-Qexpr type!");
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

lval* builtin_cons(lval* a) {
  /* Guard types */
  LASSERT(a, a->count == 2, "Function 'cons' passed too few or many arguments!");
  lval* x = a->cell[0];
  lval* y = a->cell[1];
  LASSERT(a, x->type == LVAL_NUM
        || x->type == LVAL_SEXPR
        || x->type == LVAL_QEXPR, "Function 'cons' passed incorrect value in first argument!");
  LASSERT(a, y->type == LVAL_QEXPR, "Function 'cons' passed incorrect, non-Qexpr type in second argument!");

  /* Create a new list and append 'x' */
  lval* xs = lval_qexpr();
  xs = lval_add(xs, x);

  /* Join new singleton list with list 'y' */
  xs = lval_join(xs, y);
  
  return xs;
}

lval* builtin_len(lval* a) {
  /* Guard types */
  /* accepts single Qexpr list */
  /* Pure function */
  LASSERT(a, a->count == 1, "Function 'len' passed too many arguments!");
  lval* x = a->cell[0];
  LASSERT(a, x->type == LVAL_QEXPR, "Function 'len' passed incorrect, non-Qexpr type!");

  return lval_num(x->count);
}

lval* builtin_init(lval* a) {
  /* Guard types */
  /* accepts single Qexpr list */
  LASSERT(a, a->count == 1, "Function 'init' passed too many arguments!");
  lval* x = a->cell[0];
  LASSERT(a, x->type == LVAL_QEXPR, "Function 'init' passed incorrect, non-Qexpr type!");

  /* Extract pointer to last element */
  /* Shift and reallocate cell */
  /* Delete returned the last-element pointer*/
  lval_del(lval_pop(x, x->count - 1));
  return x;
}

/* Builtins lookup */

lval* builtin(lval* a, char* func) {
  if (strcmp("list", func) == 0) { return builtin_list(a); }
  if (strcmp("head", func) == 0) { return builtin_head(a); }
  if (strcmp("tail", func) == 0) { return builtin_tail(a); }
  if (strcmp("join", func) == 0) { return builtin_join(a); }
  if (strcmp("eval", func) == 0) { return builtin_eval(a); }
  if (strstr("+-*/%^", func)
      || strcmp("min", func) == 0
      || strcmp("max", func) == 0) { return builtin_op(a, func); }
  if (strcmp("cons", func) == 0) { return builtin_cons(a); }
  if (strcmp("len", func) == 0) { return builtin_len(a); }
  if (strcmp("init", func) == 0) { return builtin_init(a); }
  return lval_err("Unknown function!");
}

/**
 * Main loop
 * 
 */
// TODO Refactor to Declarations header import

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
     symbol   : '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\"   \
              |  \"list\" | \"head\" | \"tail\" | \"join\" | \"eval\"   \
              |  \"cons\" | \"len\" | \"init\" ;                        \
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

  /* In a never-ending loop */
  while (1) {

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
      lval* x = lval_eval(lval_read(r.output));         // Composition
      lval_println(x);
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