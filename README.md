# build-your-own-lisp
Building a subset of Lisp written in C to learn both languages - tutorial at http://www.buildyourownlisp.com

### Dependencies

- `cc` compiler for Clang (change to `gcc` in `clisp.sh` at will)
- `mpc` ([Micro Parser Combinator](https://github.com/orangeduck/mpc)) library, included in source already
- Other standard C libraries: `stdio.h`, `stdlib.h`, `math.h`

### How to use

To run the interactive console, pull this repository and then in your terminal, run:

```
# Each new revision of the 'clisp' language has its own file
# the latest being 'variables.c'
./clisp.sh variables
```

### Features

- [x] S-Expression (evaluatable)
- [x] Q-Expression (can be written as code inside data)
- [x] Environment for containing variables defined and retrieving them
- [x] Functions (builtin)
  - [x] Arithmetic (`+`, `-`, `*`, `/`, `%`, `^`, `max`, `min`)
  - [x] List-processing (`list`, `head`, `tail`, `eval`, `join`, `cons`, `len`, `init`)
  - [x] Definition (`def`) for numerical variables so far, supporting tuple assignment (e.g. `def {a b c} 1 2 3`)
  - [x] Exit (`exit ()`)
  - [x] All defined variables (`env ()`)
- [x] Rich error reports and error-as-expression