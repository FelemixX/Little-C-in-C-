/****** Internal Library Functions *******/

/* Add more of your own, here. */

#if defined(_MSC_VER)
#include <conio.h> /* if your compiler does not
					   support this  header file,
					   remove it */
#endif

#include <stdio.h>
#include <stdlib.h>

extern char *source_code_location; /* указатель на расположение в исходнике программы */
extern char current_token[80];	   /* токен в строковом формате */
extern char token_type;			   /* хранит тип данных токена */
extern char current_tok_datatype;  /* тип данных токена для внутренних переменных */

enum token_types
{
	DELIMITER, //!; , +-< >'/*%^= ()
	VARIABLE,
	NUMBER,
	KEYWORD,
	TEMP,
	STRING,
	BLOCK	//блоки кода, которые находятся в обработке. Что то вроде темпа только для других задач
};

/* Константы, к которым обращается функция,
когда интерпретатор встречает ошибку в коде */
enum error_msg
{
	SYNTAX, //синтаксическая ошибка
	UNBAL_PARENS,	//несбалансированные скобки ()
	NO_EXP,	//нет выражения
	EQUALS_EXPECTED,	//ожидались операторы сравнения
	NOT_VAR,	//не является переменной	
	PARAM_ERR,	//ошибка в функции
	SEMICOLON_EXPECTED,	//не поставили точку с запятой ;
	UNBAL_BRACES,	//слишком много или не хватает операторных скобок {}
	FUNC_UNDEFINED,	//неправильно описали функцию
	TYPE_EXPECTED,	//не указали тип данных
	NESTED_FUNCTIONS,
	RET_NOCALL,	
	PAREN_EXPECTED,	//потеряли одну из скобок ()
	WHILE_EXPECTED,
	QUOTE_EXPECTED,
	NOT_STRING,
	TOO_MANY_LVARS,
	DIV_BY_ZERO	//на ноль делить нельзя блеать
};

int get_next_token(void);
void syntax_error(int error), eval_expression(int *result);
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
	source_code_location++; /* продолжаем работать, пока не достигнем конца строки */
	return ch;
}

/* Put a character to the display. */
int call_putch(void)
{
	int value;

	eval_expression(&value);
	printf("%c", value);
	return value;
}

/* Call puts(). */
int call_puts(void)
{
	/* Если при вызове puts() у нас нет открытия скобок функции и внутри нет никакой строки, 
	а так же после функции не стоит ;
	Проверяем синтаксис на подобные ошибки
	Спрашивается, нахрена тогда было до этого делать другой анализатор кода, 
	если в итоге был сделан этот костыль */
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
		syntax_error(SEMICOLON_EXPECTED);
	shift_source_code_location_back();
	return 0;
}

/* Самописный аналог printf() */
int print(void)
{
	int i;

	get_next_token();
	if (*current_token != '(')
		syntax_error(PAREN_EXPECTED);

	get_next_token();
	if (token_type == STRING)
	{ /* выводим строку */
		printf("%s ", current_token);
	}
	else
	{ /* выводим число */
		shift_source_code_location_back();
		eval_expression(&i);
		printf("%d ", i);
	}

	get_next_token();

	if (*current_token != ')')
		syntax_error(PAREN_EXPECTED);

	get_next_token();
	if (*current_token != ';')
		syntax_error(SEMICOLON_EXPECTED);
	shift_source_code_location_back();
	return 0;
}

/* Считываем ЦЕЛЫЕ числа из строки в сосноли. */
int getnum(void)
{
	char s[80];

	if (fgets(s, sizeof(s), stdin) != NULL)
	{
		while (*source_code_location != ')')
			source_code_location++;
		source_code_location++; /* читаем до конца строки */
		return atoi(s);
	}
	else
	{
		return 0;
	}
}
