/****** Internal Library Functions *******/

/* Add more of your own, here. */

#if defined(_MSC_VER)
#include <conio.h> /* if your compiler does not
					   support this  header file,
					   remove it */
#endif

#include <stdio.h>
#include <stdlib.h>

extern char *source_code_location; /* points to current location in program */
extern char current_token[80];	   /* holds string representation of current_token */
extern char token_type;			   /* contains type of current_token */
extern char current_tok_datatype;  /* holds the internal representation of current_token */

enum token_types
{
	DELIMITER,
	IDENTIFIER,
	NUMBER,
	KEYWORD,
	TEMP,
	STRING,
	BLOCK
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
	NOT_STRING,
	TOO_MANY_LVARS,
	DIV_BY_ZERO
};

int get_next_token(void);
void syntax_error(int error), eval_exp(int *result);
void shift_source_code_location_back(void);

/* Get a character from console. (Use getchar() if
   your compiler does not support       _getche().) */
int call_getche(void)
{
	char ch;
#if defined(_QC)
	ch = (char)getche();
#elif defined(_MSC_VER)
	ch = (char)_getche();
#else
	ch = (char)getchar();
#endif
	while (*source_code_location != ')')
		source_code_location++;
	source_code_location++; /* advance to end of line */
	return ch;
}

/* Put a character to the display. */
int call_putch(void)
{
	int value;

	eval_exp(&value);
	printf("%c", value);
	return value;
}

/* Call puts(). */
int call_puts(void)
{
	get_next_token();
	if (*current_token != '(')
		syntax_error(PAREN_EXPECTED);
	get_next_token();
	if (token_type != STRING)
		syntax_error(QUOTE_EXPECTED);
	puts(current_token);
	get_next_token();
	if (*current_token != ')')
		syntax_error(PAREN_EXPECTED);

	get_next_token();
	if (*current_token != ';')
		syntax_error(SEMI_EXPECTED);
	shift_source_code_location_back();
	return 0;
}

/* A built-in console output function. */
int print(void)
{
	int i;

	get_next_token();
	if (*current_token != '(')
		syntax_error(PAREN_EXPECTED);

	get_next_token();
	if (token_type == STRING)
	{ /* output a string */
		printf("%s ", current_token);
	}
	else
	{ /* output a number */
		shift_source_code_location_back();
		eval_exp(&i);
		printf("%d ", i);
	}

	get_next_token();

	if (*current_token != ')')
		syntax_error(PAREN_EXPECTED);

	get_next_token();
	if (*current_token != ';')
		syntax_error(SEMI_EXPECTED);
	shift_source_code_location_back();
	return 0;
}

/* Read an integer from the keyboard. */
int getnum(void)
{
	char s[80];

	if (fgets(s, sizeof(s), stdin) != NULL)
	{
		while (*source_code_location != ')')
			source_code_location++;
		source_code_location++; /* advance to end of line */
		return atoi(s);
	}
	else
	{
		return 0;
	}
}
