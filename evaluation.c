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

/* Define Lispy Value struct */
typedef struct {
  int type;
  long num;
  int err;
} lval;

/* Enum of type constants */
enum { LVAL_NUM, LVAL_ERR };

/* Enum of error subtypes */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

/* Constructor (generator) for number-type lval */
lval lval_num(long x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

/* Constructor (generator) for error-type lval */
lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}

/* Print an lval value */
void lval_print(lval v) {
  switch (v.type) {
    case LVAL_NUM:
      printf("%li", v.num);
      break;
    case LVAL_ERR:
      if (v.err == LERR_DIV_ZERO) { printf("Error: Division by Zero"); }
      if (v.err == LERR_BAD_OP)   { printf("Error: Invalid Operator"); }
      if (v.err == LERR_BAD_NUM)  { printf("Error: Invalid Number"); }
      break;
    default:
      break;
  }
}

/* Print an lval value followed by a newline */
void lval_println(lval v) {
  lval_print(v);
  putchar('\n');
}

/* Read Operator to perform corresponding operations */
lval eval_op(lval x, char* op, lval y) {

  /* If any of x and y is an error, return */
  if (x.type == LVAL_ERR) { return x; }
  if (y.type == LVAL_ERR) { return y; }

  if (strcmp(op, "-") == 0)   { return lval_num(x.num - y.num); }
  if (strcmp(op, "+") == 0)   { return lval_num(x.num + y.num); }   // '+' is converted to int
  if (strcmp(op, "*") == 0)   { return lval_num(x.num * y.num); }
  if (strcmp(op, "/") == 0)   {
    /* Implement safe division */ 
    return y.num == 0
      ? lval_err(LERR_DIV_ZERO)
      : lval_num(x.num / y.num);
  }
  if (strcmp(op, "%") == 0)   { return lval_num(x.num % y.num); }
  if (strcmp(op, "^") == 0)   { return lval_num(pow(x.num, y.num)); }
  if (strcmp(op, "min") == 0) { return lval_num(fmin(x.num, y.num)); }
  if (strcmp(op, "max") == 0) { return lval_num(fmax(x.num, y.num)); }

  /* If no operator matches, return LERR_BAD_OP */
  return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* t) {
  /* Base case */
  if (strstr(t->tag, "number")) {
    /* Safely convert string to number */
    errno = 0;    // external flag for strtol
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE
      ? lval_num(x)
      : lval_err(LERR_BAD_NUM);
    // return atoi(t->contents);
  }

  /* Operator at [1] ([0] is "(") */
  char* op = t->children[1]->contents;

  /* Recursively evaluate first argument at [2] */
  lval x = eval(t->children[2]);

  /* For as many children expr as there remains from [3] */
  /* Since currying / operator overloading spells support for multi-argument */
  int i = 3;
  while (strstr(t->children[i]->tag, "expr")) {
    /* Recursively evaluate second argument */
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }

  return x;
}

int main(int argc, char** argv) {

  /* Define some parsers */
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Operator = mpc_new("operator");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Lispy = mpc_new("lispy");
  
  /* Define the language */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                                   \
     number   : /-?[0-9]+/ ;                                            \
     operator : '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\" ; \
     expr     : <number> | '(' <operator> <expr>+ ')' ;                 \
     lispy    : /^/ <operator> <expr>+ /$/ ;                            \
    ",
    Number, Operator, Expr, Lispy);
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
      lval result = eval(r.output);
      lval_println(result);
      // /* On Success -> Print the AST */
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
  mpc_cleanup(4, Number, Operator, Expr, Lispy);

  return 0;
}

/*
json = { 
  [a-zA-Z]+: ([0-9a-zA-Z] | <json>) | [a-zA-Z]+: ([0-9a-zA-Z] | <json>) (, [a-zA-Z]+: ([0-9a-zA-Z] | <json>))+
} 
*/