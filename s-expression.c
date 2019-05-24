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
enum { LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_ERR };

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

/* Free memory of each subtypes of lval */
void lval_del(lval* v) {
  switch (v->type) {
    case LVAL_NUM:    break;

    /* Free the error and symbol string memories */
    case LVAL_ERR:    free(v->err);   break;
    case LVAL_SYM:    free(v->sym);   break;

    case LVAL_SEXPR:
      for (int i = 0; i < v->count; i++) {
        /* Recursively lval_del memories from all s-expressions */
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

  /* Otherwise if the following expressions are valid then add to this list */
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0)  { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0)  { continue; }
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
    case LVAL_SYM:    printf("%s", v->sym);           break;
    case LVAL_SEXPR:  lval_expr_print(v, '(', ')');  break;
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
lval* builtin_op(lval* a, char* op);

lval* lval_expr_sexpr(lval* v) {
  /* Transform of v -> v' */

  /* Evaluate children */
  for (int i = 0; i < v->count; i++) {
    /* Element-wise transformation */
    v->cell[i] = lval_eval(v->cell[i]);
  }

  /* Error checking */
  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
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
  lval* result = builtin_op(v, f->sym);
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

lval* builtin_op(lval* a, char* op) {
  /* Apply op on variably long lval a */
  /* Upgrade from 2-argument eval_op */

  /* Ensure all arguments are numbers */
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
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
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Lispy = mpc_new("lispy");
  
  /* Define the language */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                                   \
     number   : /-?[0-9]+/ ;                                            \
     symbol   : '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\" ; \
     sexpr    : '(' <expr>* ')' ;                                       \
     expr     : <number> | <symbol> | <sexpr> ;                         \
     lispy    : /^/ <expr>* /$/ ;                                       \
    ",
    Number, Symbol, Sexpr, Expr, Lispy);
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
  mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);

  return 0;
}

/*
json = { 
  [a-zA-Z]+: ([0-9a-zA-Z] | <json>) | [a-zA-Z]+: ([0-9a-zA-Z] | <json>) (, [a-zA-Z]+: ([0-9a-zA-Z] | <json>))+
} 
*/