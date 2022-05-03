/* A Little C interpreter. */

#include <stdio.h>
#include <setjmp.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define NUM_FUNC 100
#define NUM_GLOBAL_VARS 100
#define NUM_LOCAL_VARS 200
#define NUM_BLOCK 100
#define ID_LEN 32
#define FUNC_CALLS 31
#define NUM_PARAMS 31
#define PROG_SIZE 10000
#define LOOP_NEST 31

// Secure function compatibility
#if !defined(_MSC_VER) || _MSC_VER < 1400
#define strcpy_s(dest, count, source) strncpy((dest), (source), (count))
#define fopen_s(pFile, filename, mode) (((*(pFile)) = fopen((filename), (mode))) == NULL)
#endif

enum tok_types
{
	DELIMITER,
	IDENTIFIER,
	NUMBER,
	KEYWORD,
	TEMP,
	STRING,
	BLOCK
};

/* add additional C keyword tokens here */
enum tokens
{
	ARG,
	CHAR,
	INT,
	IF,
	ELSE,
	FOR,
	DO,
	WHILE,
	SWITCH,
	RETURN,
	CONTINUE,
	BREAK,
	EOL,
	FINISHED,
	END
};

/* add additional double operators here (such as ->) */
enum double_ops
{
	LT = 1,
	LE,
	GT,
	GE,
	EQ,
	NE
};

/* These are the constants used to call syntax_error() when
   a syntax error occurs. Add more if you like.
   NOTE: SYNTAX is a generic error message used when
   nothing else seems appropriate.
*/
enum error_msg
{
	SYNTAX,
	UNBAL_PARENS,
	NO_EXP,
	EQUALS_EXPECTED,
	NOT_VAR,
	PARAM_ERR,
	SEMI_EXPECTED,
	UNBAL_BRACES,
	FUNC_UNDEF,
	TYPE_EXPECTED,
	NEST_FUNC,
	RET_NOCALL,
	PAREN_EXPECTED,
	WHILE_EXPECTED,
	QUOTE_EXPECTED,
	NOT_TEMP,
	TOO_MANY_LVARS,
	DIV_BY_ZERO
};

char *source_code_location; /* current location in source code */
char *program_start_buffer; /* points to start of program buffer */
jmp_buf execution_buffer;	/* hold environment for longjmp() */

/* An array of these structures will hold the info
   associated with global variables.
*/
struct var_type
{
	char variable_name[ID_LEN];
	int variable_type;
	int variable_value;
} global_vars[NUM_GLOBAL_VARS];

struct var_type local_var_stack[NUM_LOCAL_VARS];

struct func_type
{
	char func_name[ID_LEN];
	int ret_type;
	char *loc; /* location of entry point in file */
} func_table[NUM_FUNC];

int call_stack[NUM_FUNC];

struct commands
{ /* keyword lookup table_with_statements */
	char command[20];
	char tok;
} table_with_statements[] = {
	/* Commands must be entered lowercase */
	{"if", IF}, /* in this table_with_statements. */
	{"else", ELSE},
	{"for", FOR},
	{"do", DO},
	{"while", WHILE},
	{"char", CHAR},
	{"int", INT},
	{"return", RETURN},
	{"continue", CONTINUE},
	{"break", BREAK},
	{"end", END},
	{"", END} /* mark end of table_with_statements */
};

char current_token[80];
char token_type, current_tok;

int functos;				  /* index to top of function call stack */
int func_index;				  /* index into function table_with_statements */
int global_variable_position; /* индекс глобальной переменной в таблице global_vars */
int lvartos;				  /* index into local variable stack */

int ret_value;		 /* function return value */
int ret_occurring;	 /* function return is occurring */
int break_occurring; /* loop break is occurring */

void print(void), pre_scan(void);
void declare_global(void), call(void), shift_source_code_location_back(void);
void decl_local(void), local_push(struct var_type i);
void eval_exp(int *value), syntax_error(int error);
void exec_if(void), find_eob(void), exec_for(void);
void get_params(void), get_args(void);
void exec_while(void), func_push(int i), exec_do(void);
void assign_var(char *var_name, int value);
int load_program(char *p, char *fname), find_var(char *s);
void interp_block(void), func_ret(void);
int func_pop(void), is_var(char *s);
char *find_func(char *name), get_next_token(void);

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Usage: littlec <filename>\n");
		exit(1);
	}

	/* выделить память под программу
	 * PROG_SIZE - размер программы*/
	if ((program_start_buffer = (char *)malloc(PROG_SIZE)) == NULL)
	{
		printf("Allocation Failure"); //если программа пустая
		exit(1);
	}

	/* загрузить программу для выполнения */
	char *file_name = argv[1];
	if (!load_program(program_start_buffer, file_name))
		exit(1);
	if (setjmp(execution_buffer))
		exit(1); /* инициализация буфера longjump */

	global_variable_position = 0; /* инициализация индекса глобальных переменных */

	/* установка указателя на начало буфера программы  */
	source_code_location = program_start_buffer;
	pre_scan(); /* определение адресов всех функций
				  и глобальных переменных
				  короче говоря, предварительный проход компилятора */

	lvartos = 0;		 /* инициализация индекса стека локальных переменных */
	functos = 0;		 /* инициализация индекса стека вызова CALL */
	break_occurring = 0; /* initialize the break occurring flag */

	/* вызываем функцию main
	 * она всегда вызывается первой*/
	source_code_location = find_func("main"); /* ищем начало программы */

	if (!source_code_location)
	{ /* main написан с ошибкой или отсутствует */
		printf("main() not found.\n");
		exit(1);
	}

	source_code_location--; /* возвращаемся к открывающей ( */
	strcpy_s(current_token, 80, "main");
	call(); /* вызываем main и интерпретируем */

	return 0;
}

/* Interpret a single statement or block of code. When
   interp_block() returns from its initial call, the final
   brace (or a return) in main() has been encountered.
*/
void interp_block(void)
{
	int value;
	char block = 0;

	do
	{
		token_type = get_next_token();

		/* If interpreting single statement, return on
		   first semicolon.
		*/

		/* see what kind of current_token is up */
		if (token_type == IDENTIFIER)
		{
			/* Not a keyword, so process expression. */
			shift_source_code_location_back(); /* restore current_token to input stream for
						  further processing by eval_exp() */
			eval_exp(&value);				   /* process the expression */
			if (*current_token != ';')
				syntax_error(SEMI_EXPECTED);
		}
		else if (token_type == BLOCK)
		{							   /* if block delimiter */
			if (*current_token == '{') /* is a block */
				block = 1;			   /* interpreting block, not statement */
			else
				return; /* is a }, so return */
		}
		else /* is keyword */
			switch (current_tok)
			{
			case CHAR:
			case INT: /* declare local variables */
				shift_source_code_location_back();
				decl_local();
				break;
			case RETURN: /* return from function call */
				func_ret();
				ret_occurring = 1;
				return;
			case CONTINUE: /* continue loop execution */
				return;
			case BREAK: /* break loop execution */
				break_occurring = 1;
				return;
			case IF: /* process an if statement */
				exec_if();
				if (ret_occurring > 0 || break_occurring > 0)
				{
					return;
				}
				break;
			case ELSE:		/* process an else statement */
				find_eob(); /* find end of else block and continue execution */
				break;
			case WHILE: /* process a while loop */
				exec_while();
				if (ret_occurring > 0)
				{
					return;
				}
				break;
			case DO: /* process a do-while loop */
				exec_do();
				if (ret_occurring > 0)
				{
					return;
				}
				break;
			case FOR: /* process a for loop */
				exec_for();
				if (ret_occurring > 0)
				{
					return;
				}
				break;
			case END:
				exit(0);
			}
	} while (current_tok != FINISHED && block);
}

/* Загрузить программу */
int load_program(char *p, char *fname)
{
	FILE *fp;
	int i;

	if (fopen_s(&fp, fname, "rb") != 0 || fp == NULL)
		return 0;

	i = 0;
	do
	{
		*p = (char)getc(fp);
		p++;
		i++;
	} while (!feof(fp) && i < PROG_SIZE);

	if (*(p - 2) == 0x1a) // рудимент из бейсика. Ставится в конце исполняемого файла
		*(p - 2) = '\0';  /* конец строки завершает программу */
	else
		*(p - 1) = '\0';
	fclose(fp);
	return 1;
}

/* Найти адреса всех функций
   и запомнить глобальные переменные. */
void pre_scan(void) // Предварительный проход компилятора
{
	char *initial_source_code_location, *temp_source_code_location; //*p - указатель на указатель ? (На source_code_location). temp_source_code_location тоже указатель на source_code_location???
	char temp_token[ID_LEN + 1];
	int datatype;
	int is_brace_open = 0; /* Если is_brace_open = 0, о текущая
					  позиция оказателя программы находится
					  в не какой-либо функции */

	initial_source_code_location = source_code_location;
	func_index = 0;
	do
	{
		while (is_brace_open)
		{ /* обхода кода функции внутри фигурных скобок */
			get_next_token();
			if (*current_token == '{') //когда встречаем открывающую скобку, увеличиваем is_brace_open на один
				is_brace_open++;
			if (*current_token == '}')
				is_brace_open--; //когда встречаем закрывающую уменьшаем на один
		}

		temp_source_code_location = source_code_location; /* запоминаем текущую позицию */
		get_next_token();
		/* тип глобальной переменной или возвращаемого значения функции */
		if (current_tok == CHAR || current_tok == INT)
		{
			datatype = current_tok; /* сохранить тип данных */
			get_next_token();
			if (token_type == IDENTIFIER)
			{
				//
				strcpy_s(temp_token, ID_LEN + 1, current_token);
				get_next_token();
				if (*current_token != '(')
				{													  /* должно быть глобальной переменной */
					source_code_location = temp_source_code_location; /* вернуться в начало объявления */
					declare_global();
				}
				else if (*current_token == '(')
				{ /* должно быть функцией */
					func_table[func_index].loc = source_code_location;
					func_table[func_index].ret_type = datatype;
					strcpy_s(func_table[func_index].func_name, ID_LEN, temp_token);
					func_index++;
					while (*source_code_location != ')')
						source_code_location++;
					source_code_location++;
					/* сейчас source_code_location указывает на открывающуюся
					   фигурную скобку функции */
				}
				else
					shift_source_code_location_back();
			}
		}
		else if (*current_token == '{')
			is_brace_open++;
	} while (current_tok != FINISHED);
	source_code_location = initial_source_code_location;
}

/* Return the entry point of the specified function.
   Return NULL if not found.
*/
char *find_func(char *name)
{
	register int i;

	for (i = 0; i < func_index; i++)
		if (!strcmp(name, func_table[i].func_name))
			return func_table[i].loc;

	return NULL;
}

/* Объявление глобальной переменной
 * Данные хранятся в списке global vars */
void declare_global(void)
{
	int variable_type;

	get_next_token(); /* получаем тип данных */

	variable_type = current_tok; /* запоминаем тип данных */

	do
	{ /* обработка списка с разделителями запятыми */
		global_vars[global_variable_position].variable_type = variable_type;
		global_vars[global_variable_position].variable_value = 0; /* инициализируем нулем */
		get_next_token();										  /* определяем имя */
		strcpy_s(global_vars[global_variable_position].variable_name, ID_LEN, current_token);
		get_next_token();
		global_variable_position++;
	} while (*current_token == ',');
	if (*current_token != ';')
		syntax_error(SEMI_EXPECTED);
}

/* Declare a local variable. */
void decl_local(void)
{
	struct var_type i;

	get_next_token(); /* get type */

	i.variable_type = current_tok;
	i.variable_value = 0; /* init to 0 */

	do
	{					  /* process comma-separated list */
		get_next_token(); /* get var name */
		strcpy_s(i.variable_name, ID_LEN, current_token);
		local_push(i);
		get_next_token();
	} while (*current_token == ',');
	if (*current_token != ';')
		syntax_error(SEMI_EXPECTED);
}

/* Call a function. */
void call(void)
{
	char *loc, *temp;
	int lvartemp;

	loc = find_func(current_token); /* find entry point of function */
	if (loc == NULL)
		syntax_error(FUNC_UNDEF); /* function not defined */
	else
	{
		lvartemp = lvartos;			 /* save local var stack index */
		get_args();					 /* get function arguments */
		temp = source_code_location; /* save return location */
		func_push(lvartemp);		 /* save local var stack index */
		source_code_location = loc;	 /* reset prog to start of function */
		ret_occurring = 0;			 /* P the return occurring variable */
		get_params();				 /* load the function's parameters with the values of the arguments */
		interp_block();				 /* interpret the function */
		ret_occurring = 0;			 /* Clear the return occurring variable */
		source_code_location = temp; /* reset the program initial_source_code_location */
		lvartos = func_pop();		 /* reset the local var stack */
	}
}

/* Push the arguments to a function onto the local
   variable stack. */
void get_args(void)
{
	int value, count, temp[NUM_PARAMS];
	struct var_type i;

	count = 0;
	get_next_token();
	if (*current_token != '(')
		syntax_error(PAREN_EXPECTED);

	/* process a comma-separated list of values */
	do
	{
		eval_exp(&value);
		temp[count] = value; /* save temporarily */
		get_next_token();
		count++;
	} while (*current_token == ',');
	count--;
	/* now, push on local_var_stack in reverse order */
	for (; count >= 0; count--)
	{
		i.variable_value = temp[count];
		i.variable_type = ARG;
		local_push(i);
	}
}

/* Get function parameters. */
void get_params(void)
{
	struct var_type *p;
	int i;

	i = lvartos - 1;
	do
	{ /* process comma-separated list of parameters */
		get_next_token();
		p = &local_var_stack[i];
		if (*current_token != ')')
		{
			if (current_tok != INT && current_tok != CHAR)
				syntax_error(TYPE_EXPECTED);

			p->variable_type = token_type;
			get_next_token();

			/* link parameter name with argument already on
			   local var stack */
			strcpy_s(p->variable_name, ID_LEN, current_token);
			get_next_token();
			i--;
		}
		else
			break;
	} while (*current_token == ',');
	if (*current_token != ')')
		syntax_error(PAREN_EXPECTED);
}

/* Return from a function. */
void func_ret(void)
{
	int value;

	value = 0;
	/* get return value, if any */
	eval_exp(&value);

	ret_value = value;
}

/* Push a local variable. */
void local_push(struct var_type i)
{
	if (lvartos >= NUM_LOCAL_VARS)
	{
		syntax_error(TOO_MANY_LVARS);
	}
	else
	{
		local_var_stack[lvartos] = i;
		lvartos++;
	}
}

/* Pop index into local variable stack. */
int func_pop(void)
{
	int index = 0;
	functos--;
	if (functos < 0)
	{
		syntax_error(RET_NOCALL);
	}
	else if (functos >= NUM_FUNC)
	{
		syntax_error(NEST_FUNC);
	}
	else
	{
		index = call_stack[functos];
	}

	return index;
}

/* Push index of local variable stack. */
void func_push(int i)
{
	if (functos >= NUM_FUNC)
	{
		syntax_error(NEST_FUNC);
	}
	else
	{
		call_stack[functos] = i;
		functos++;
	}
}

/* Assign a value to a variable. */
void assign_var(char *var_name, int value)
{
	register int i;

	/* first, see if it's a local variable */
	for (i = lvartos - 1; i >= call_stack[functos - 1]; i--)
	{
		if (!strcmp(local_var_stack[i].variable_name, var_name))
		{
			local_var_stack[i].variable_value = value;
			return;
		}
	}
	if (i < call_stack[functos - 1])
		/* if not local, try global var table_with_statements */
		for (i = 0; i < NUM_GLOBAL_VARS; i++)
			if (!strcmp(global_vars[i].variable_name, var_name))
			{
				global_vars[i].variable_value = value;
				return;
			}
	syntax_error(NOT_VAR); /* variable not found */
}

/* Find the value of a variable. */
int find_var(char *s)
{
	register int i;

	/* first, see if it's a local variable */
	for (i = lvartos - 1; i >= call_stack[functos - 1]; i--)
		if (!strcmp(local_var_stack[i].variable_name, current_token))
			return local_var_stack[i].variable_value;

	/* otherwise, try global vars */
	for (i = 0; i < NUM_GLOBAL_VARS; i++)
		if (!strcmp(global_vars[i].variable_name, s))
			return global_vars[i].variable_value;

	syntax_error(NOT_VAR); /* variable not found */
	return -1;
}

/* Determine if an identifier is a variable. Return
   1 if variable is found; 0 otherwise.
*/
int is_var(char *s)
{
	register int i;

	/* first, see if it's a local variable */
	for (i = lvartos - 1; i >= call_stack[functos - 1]; i--)
		if (!strcmp(local_var_stack[i].variable_name, current_token))
			return 1;

	/* otherwise, try global vars */
	for (i = 0; i < NUM_GLOBAL_VARS; i++)
		if (!strcmp(global_vars[i].variable_name, s))
			return 1;

	return 0;
}

/* Execute an if statement. */
void exec_if(void)
{
	int cond;

	eval_exp(&cond); /* get if expression */

	if (cond)
	{ /* is true so process target of IF */
		interp_block();
	}
	else
	{				/* otherwise skip around IF block and
					process the ELSE, if present */
		find_eob(); /* find start of next line */
		get_next_token();

		if (current_tok != ELSE)
		{
			shift_source_code_location_back(); /* restore current_token if
						  no ELSE is present */
			return;
		}
		interp_block();
	}
}

/* Execute a while loop. */
void exec_while(void)
{
	int cond;
	char *temp;

	break_occurring = 0; /* clear the break flag */
	shift_source_code_location_back();
	temp = source_code_location; /* save location of top of while loop */
	get_next_token();
	eval_exp(&cond); /* check the conditional expression */
	if (cond)
	{
		interp_block(); /* if true, interpret */
		if (break_occurring > 0)
		{
			break_occurring = 0;
			return;
		}
	}
	else
	{ /* otherwise, skip around loop */
		find_eob();
		return;
	}
	source_code_location = temp; /* loop back to top */
}

/* Execute a do loop. */
void exec_do(void)
{
	int cond;
	char *temp;

	shift_source_code_location_back();
	temp = source_code_location; /* save location of top of do loop */
	break_occurring = 0;		 /* clear the break flag */

	get_next_token(); /* get start of loop */
	interp_block();	  /* interpret loop */
	if (ret_occurring > 0)
	{
		return;
	}
	else if (break_occurring > 0)
	{
		break_occurring = 0;
		return;
	}
	get_next_token();
	if (current_tok != WHILE)
		syntax_error(WHILE_EXPECTED);
	eval_exp(&cond); /* check the loop condition */
	if (cond)
		source_code_location = temp; /* if true loop; otherwise,
					   continue on */
}

/* Find the end of a block. */
void find_eob(void)
{
	int brace;

	get_next_token();
	brace = 1;
	do
	{
		get_next_token();
		if (*current_token == '{')
			brace++;
		else if (*current_token == '}')
			brace--;
	} while (brace);
}

/* Execute a for loop. */
void exec_for(void)
{
	int cond;
	char *temp, *temp2;
	int brace;

	break_occurring = 0; /* clear the break flag */
	get_next_token();
	eval_exp(&cond); /* initialization expression */
	if (*current_token != ';')
		syntax_error(SEMI_EXPECTED);
	source_code_location++; /* get past the ; */
	temp = source_code_location;
	for (;;)
	{
		eval_exp(&cond); /* check the condition */
		if (*current_token != ';')
			syntax_error(SEMI_EXPECTED);
		source_code_location++; /* get past the ; */
		temp2 = source_code_location;

		/* find the start of the for block */
		brace = 1;
		while (brace)
		{
			get_next_token();
			if (*current_token == '(')
				brace++;
			if (*current_token == ')')
				brace--;
		}

		if (cond)
		{
			interp_block(); /* if true, interpret */
			if (ret_occurring > 0)
			{
				return;
			}
			else if (break_occurring > 0)
			{
				break_occurring = 0;
				return;
			}
		}
		else
		{ /* otherwise, skip around loop */
			find_eob();
			return;
		}
		source_code_location = temp2;
		eval_exp(&cond);			 /* do the increment */
		source_code_location = temp; /* loop back to top */
	}
}
