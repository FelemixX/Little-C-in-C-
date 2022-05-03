const NUM_FUNC = 100;
const NUM_GLOBAL_VARS = 100;
const NUM_LOCAL_VARS = 200;
const NUM_BLOCK = 100;
const ID_LEN = 32;
const FUNC_CALLS = 31;
const NUM_PARAMS = 31;
const PROG_SIZE = 10000;
const LOOP_NEST = 31;

// Secure function compatibility
// #if !defined(_MSC_VER) || _MSC_VER < 1400
// #define strcpy_s(dest, count, source) strncpy((dest), (source), (count))
// #define fopen_s(pFile, filename, mode) (((*(pFile)) = fopen((filename), (mode))) == NULL)
// #endif

const DELIMITER = 0;
const IDENTIFIER = 1;
const NUMBER = 2;
const KEYWORD = 3;
const TEMP = 4;
const STRING = 5;
const BLOCK = 6;

/* add additional C keyword tokens here */
const ARG = 0;
const CHAR = 1;
const INT = 2;
const IF = 3;
const ELSE = 4;
const FOR = 5;
const DO = 6;
const WHILE = 7;
const SWITCH = 8;
const RETURN = 9;
const CONTINUE = 10;
const BREAK = 11;
const EOL = 12;
const FINISHED = 13;
const END = 14;

/* add additional double operators here (such as ->) */
const LT = 1;
const LE = 2;
const GT = 3;
const GE = 4;
const EQ = 5;
const NE = 6;

/* These are the constants used to call sntx_err() when
   a syntax error occurs. Add more if you like.
   NOTE: SYNTAX is a generic error message used when
   nothing else seems appropriate.
*/
const SYNTAX = 0;
const UNBAL_PARENS = 1;
const NO_EXP = 2;
const EQUALS_EXPECTED = 3;
const NOT_VAR = 4;
const PARAM_ERR = 5;
const SEMI_EXPECTED = 6;
const UNBAL_BRACES = 7;
const FUNC_UNDEF = 8;
const TYPE_EXPECTED = 9;
const NEST_FUNC = 10;
const RET_NOCALL = 11;
const PAREN_EXPECTED = 12;
const WHILE_EXPECTED = 13;
const QUOTE_EXPECTED = 14;
const NOT_TEMP = 15;
const TOO_MANY_LVARS = 16;
const DIV_BY_ZERO = 17;

let prog; /* current location in source code */
let p_buf; /* points to start of program buffer */
let e_buf; /* hold environment for longjmp() */

/* An array of these structures will hold the info
   associated with global variables.
*/
// struct var_type
// {
// 	char var_name[ID_LEN];
// 	int v_type;
// 	int value;
// } global_vars[NUM_GLOBAL_VARS];

// struct var_type local_var_stack[NUM_LOCAL_VARS];

// struct func_type
// {
// 	char func_name[ID_LEN];
// 	int ret_type;
// 	char *loc; /* location of entry point in file */
// } func_table[NUM_FUNC];

// int call_stack[NUM_FUNC];

const commands = {
  /* Commands must be entered lowercase */
  if: IF /* in this table. */,
  else: ELSE,
  for: FOR,
  do: DO,
  while: WHILE,
  char: CHAR,
  int: INT,
  return: RETURN,
  continue: CONTINUE,
  break: BREAK,
  end: END,
  "": END /* mark end of table */,
};

let token;
let token_type, tok;

let functos; /* index to top of function call stack */
let func_index; /* index into function table */
let gvar_index; /* index into global variable table */
let lvartos; /* index into local variable stack */

let ret_value; /* function return value */
let ret_occurring; /* function return is occurring */
let break_occurring; /* loop break is occurring */

const printf = (data) => {
  console.log(data);
};
const exit = (status) => {
  process.exit(status);
};
function main() {
  const argc = process.argv.length;
  const argv = process.argv;
  if (argc != 2) {
    printf("Usage: littlec <filename>\n");
    exit(1);
  }

  /* allocate memory for the program */
  // if ((p_buf = (char *)malloc(PROG_SIZE)) == NULL)
  // {
  // 	printf("Allocation Failure");
  // 	exit(1);
  // }

  /* load the program to execute */
  if (!load_program(p_buf, argv[1])) exit(1);
  // сохраняет в состояние стека в переменную (зачем?)
  // if (setjmp(e_buf))
  // 	exit(1); /* initialize long jump buffer */

  gvar_index = 0; /* initialize global variable index */

  /* set program pointer to start of program buffer */
  prog = p_buf;
  prescan(); /* find the location of all functions
				  and global variables in the program */

  lvartos = 0; /* initialize local variable stack index */
  functos = 0; /* initialize the CALL stack index */
  break_occurring = 0; /* initialize the break occurring flag */

  /* setup call to main() */
  prog = find_func("main"); /* find program starting point */

  if (!prog) {
    /* incorrect or missing main() function in program */
    printf("main() not found.\n");
    exit(1);
  }

  prog--; /* back up to opening ( */
  strcpy_s(token, 80, "main");
  call(); /* call main() to start interpreting */

  return 0;
}

function load_program(fname) {}
