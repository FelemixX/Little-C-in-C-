#include <iostream>
#include <cstdio>
#include <utility>
#include <csetjmp>
#include <cstring>
#include <fstream>
#include "enum.h"

/// TODO параша, на помойку это
#if !defined(_MSC_VER) || _MSC_VER < 1400
#define strcpy_s(dest, count, source) strncpy((dest), (source), (count))
#endif

#define PROG_SIZE 10000
#define ID_LEN 32
#define NUM_GLOBAL_VARS 100
#define NUMBER_FUNCTIONS 100
#define NUM_PARAMS 31
#define NUM_LOCAL_VARS 200

using namespace std;

class LittleC
{
public:
	/// TODO перевести
	char *source_code_location;				/* current location in source code */
	char *program_start_buffer;				/* points to start of program buffer */
	jmp_buf execution_buffer;				/* hold environment for longjmp() */

	char current_token[80];					/* string representation of current_token */
	char token_type;						/* contains type of current_token */
	char current_tok_datatype;				/* internal representation of current_token */

	int global_variable_position;			/* индекс глобальной переменной в таблице global_vars */
	int function_position;					/* index into function table_with_statements */

	int function_last_index_on_call_stack;	/* index to top of function call stack */
	int lvartos;						  	/* index into local variable stack */

	int ret_value;		 					/* function return value */
	int ret_occurring;	 					/* function return is occurring */
	int break_occurring; 					/* loop break is occurring */

	string fileName;						/* Название файла с программой */

	/// TODO надо что-то с чтением без пути (не путю) сделать
	string path = "/home/ahehiohyou/Desktop/works/zen/littlec/";

	int call_stack[NUMBER_FUNCTIONS];

	/// TODO что это ебать
	struct commands
	{ /* keyword lookup table_with_statements */
		char command[20];
		char tok;
	} table_with_statements[12] = {
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

	/// Хранит тип возвращаемых данных, название функции, местоположение в коде
	struct function_type
	{
		char func_name[ID_LEN];
		int ret_type;
		char *loc; /* location of entry point in file */
	} function_table[NUMBER_FUNCTIONS];

	/// An array of these structures will hold the info associated with global variables. TODO что это
	struct variable_type
	{
		char variable_name[ID_LEN];
		int variable_type;
		int variable_value;
	} global_vars[NUM_GLOBAL_VARS];

	struct variable_type local_var_stack[NUM_LOCAL_VARS];

	/// Конструктор
	/// мейэби анюзд..........	 пХАХАХПАХПХХАХАХ В ГОЛОС
	[[maybe_unused]] explicit LittleC(string _fileName) : fileName(std::move(_fileName)) {}

	int execute()
	{
		/// Если названия файла нет - выход
		if (fileName.empty())
		{
			cout << "Пустое имя файла" << endl;
			exit(1);
		}

		/// Выделить память под программу PROG_SIZE - размер программы, не получилось - выход
		if ((program_start_buffer = (char *)malloc(PROG_SIZE)) == nullptr)
		{
			cout << "Сбой распределения памяти" << endl;
			exit(1);
		}

		/// Загрузить программу для выполнения
		if (!load_program(program_start_buffer, fileName))
		{
			cout << "Не удалось считать код" << endl;
			exit(1);
		}
		/// TODO что это?
		if (setjmp(execution_buffer))
		{
			cout << "Какая-то хуйня с setjmp" << endl;
			exit(1); /* инициализация буфера longjump */
		}

		/// Инициализация индекса глобальных переменных
		global_variable_position = 0;
		/// Установка указателя на начало буфера программы
		source_code_location = program_start_buffer;

		/// Определение адресов всех функций и глобальных переменных
		prescan_source_code();

		/// Инициализация индекса стека локальных переменных
		lvartos = 0;
		/// Инициализация индекса стека вызова CALL
		function_last_index_on_call_stack = 0;
		/// initialize the break occurring flag
		break_occurring = 0;

		/// Вызываем функцию main она всегда вызывается первой
		source_code_location = find_function_in_function_table("main");
		/// main написан с ошибкой или отсутствует
		if (!source_code_location)
		{
			cout << "\"main\" не найдено или написано с ошибкой" << endl;
			exit(1);
		}

		/// Возвращаемся к открывающей (
		source_code_location--;
		strcpy_s(current_token, 80, "main");
		/// Вызываем main и интерпретируем
		call_function();

		return 0;
	}

	/**
	 * Загрузить программу в память
	 * @param p
	 * @param fname
	 * @return
	 */
	int load_program(char *p, const string& fname)
	{
		FILE *fp;

		path += fname;

		fp = fopen(path.c_str(), "rb");;
		int i = 0;

		while (!feof(fp) && i < PROG_SIZE)
		{
			*p = (char)getc(fp);
			p++;
			i++;
		}

		/// Рудимент из бейсика. Ставится в конце исполняемого файла
		if (*(p - 2) == 0x1a)
			*(p - 2) = '\0';  /* конец строки завершает программу */
		else
			*(p - 1) = '\0';
		fclose(fp);
		return 1;
	}
	/**
	 * Найти адреса всех функций и запомнить глобальные переменные
	 */
	void prescan_source_code()
	{
		/// *p - указатель на указатель ? (На source_code_location). temp_source_code_location тоже указатель на source_code_location???
		char *initial_source_code_location, *temp_source_code_location;
		char temp_token[ID_LEN + 1];
		int datatype;
		/// Если is_brace_open = 0, о текущая позиция указателя программы находится в не какой-либо функции
		int is_brace_open = 0;

		initial_source_code_location = source_code_location;
		function_position = 0;
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
			if (current_tok_datatype == CHAR || current_tok_datatype == INT)
			{
				datatype = current_tok_datatype; /* сохраняем тип данных */
				get_next_token();
				if (token_type == VARIABLE)
				{
					//
					strcpy_s(temp_token, ID_LEN + 1, current_token);
					get_next_token();
					if (*current_token != '(')
					{													  /* должно быть глобальной переменной */
						source_code_location = temp_source_code_location; /* вернуться в начало объявления */
						declare_global_variables();
					}
					else if (*current_token == '(')
					{ /* должно быть функцией */
						function_table[function_position].loc = source_code_location;
						function_table[function_position].ret_type = datatype;
						strcpy_s(function_table[function_position].func_name, ID_LEN, temp_token);
						function_position++;
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
		} while (current_tok_datatype != FINISHED);
		source_code_location = initial_source_code_location;
	}
	/**
	 * Передвигаем указатель на текущую программу на *_токен_* обратно
	 *
	 * Return a current_token to input stream.
	 */
	void shift_source_code_location_back()
	{
		char *t;

		t = current_token;
		for (; *t; t++)
			source_code_location--;
	}
	/**
	 * Объявление глобальной переменной в ИНТЕРПРЕТИРУЕМОЙ программе
	 *
	 * Данные хранятся в списке global vars
	 */
	void declare_global_variables()
	{
		int variable_type;

		get_next_token(); /* получаем тип данных */

		variable_type = current_tok_datatype; /* запоминаем тип данных */

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
			syntax_error(SEMICOLON_EXPECTED);
	}
	/**
	 * @brief Печать ошибки
	 *
	 * Выводит сообщение об ошибке, основываясь на полученном типе ошибки
	 * @param error_type
	 */
	static void syntax_error(int error_type)
	{
		string errors_human_readable[]
				{
						"Синтаксическая ошибка",
						"Слишком много или мало скобок",
						"Нет выражения",
						"Не хватает знаков равно",
						"Не является переменной",
						"Параметрическая ошибка",
						"Не хватает точки с запятой",
						"Слишком много или мало операторных скобок",
						"Функция не определена",
						"Нужно указать тип данных",
						"Слишком много обращений к вложенным функциям",
						"Функция возвращает значения без обращения к ней",
						"Не хватает скобки",
						"Нет оператора для цикла while",
						"Не хватает закрывающих кавычек",
						"Не является строкой",
						"Слишком много локальных переменных",
						"На ноль делить НЕЛЬЗЯ"
				};

		/// Репрезентация ошибок анализатора в понятном для человека виде

		cout << "\n" << errors_human_readable[error_type];
	}
	/**
	 * Получить текущий символ
	 * @return
	 */
	char get_next_token()
	{
		char *temp_token;

		token_type = 0;
		current_tok_datatype = 0;

		temp_token = current_token;
		*temp_token = '\0';
		//обработка конца строки для мака и для винды
		//на самом деле это все спокойно можно заменить на SUB из Бейсика,
		//потому что он до сих пор поддерживается в новых ЯП как спецсимвол
		/* skip over white space */
		while (is_whitespace(*source_code_location) && *source_code_location)
			++source_code_location;

		/* Handle Windows and Mac newlines */
		if (*source_code_location == '\r')
		{
			++source_code_location;
			/* Only skip \n if it exists (if it doesn't, we are running on mac) */
			if (*source_code_location == '\n')
			{
				++source_code_location;
			}
			/* skip over white space */
			while (is_whitespace(*source_code_location) && *source_code_location)
				++source_code_location;
		}

		/* Handle Unix newlines */
		if (*source_code_location == '\n')
		{
			++source_code_location;
			/* skip over white space */
			while (is_whitespace(*source_code_location) && *source_code_location)
				++source_code_location;
		}

		if (*source_code_location == '\0')
		{ /* end of file */
			*current_token = '\0';
			current_tok_datatype = FINISHED;
			return (token_type = DELIMITER);
		}

		// https://www.cplusplus.com/reference/cstring/strchr/
		// strchr - Returns a pointer to the first occurrence of character in the C string str
		/*
		этот кусок находит блоки кода типа
		int function(.....)
		{
			все что написано тут, оно находит и помечает как токен BLOCK
		}
		*/
		if (strchr("{}", *source_code_location))
		{ /* block delimiters */
			*temp_token = *source_code_location;
			temp_token++;
			*temp_token = '\0';
			source_code_location++;
			return (token_type = BLOCK);
		}

		/* поиск комментариев */
		if (*source_code_location == '/')
			if (*(source_code_location + 1) == '*')
			{ /* найти конец комментария  */
				source_code_location += 2;
				do
				{
					while (*source_code_location != '*' && *source_code_location != '\0')
						source_code_location++;
					if (*source_code_location == '\0')
					{
						source_code_location--;
						break;
					}
					source_code_location++;
				} while (*source_code_location != '/');
				source_code_location++;
			}

		/* look for C++ style comments */
		if (*source_code_location == '/')
			if (*(source_code_location + 1) == '/')
			{ /* is a comment */
				source_code_location += 2;
				/* find end of line */
				while (*source_code_location != '\r' && *source_code_location != '\n' && *source_code_location != '\0')
					source_code_location++;
				if (*source_code_location == '\r' && *(source_code_location + 1) == '\n')
				{
					source_code_location++;
				}
			}

		/* look for the end of file after a comment */
		if (*source_code_location == '\0')
		{ /* end of file */
			*current_token = '\0';
			current_tok_datatype = FINISHED;
			return (token_type = DELIMITER);
		}

		if (strchr("!<>=", *source_code_location))
		{ /* is or might be
        a relational operator */
			switch (*source_code_location)
			{
				case '=':
					if (*(source_code_location + 1) == '=')
					{
						source_code_location++;
						source_code_location++;
						*temp_token = EQUAL;
						temp_token++;
						*temp_token = EQUAL;
						temp_token++;
						*temp_token = '\0';
					}
					break;
				case '!':
					if (*(source_code_location + 1) == '=')
					{
						source_code_location++;
						source_code_location++;
						*temp_token = NOT_EQUAL;
						temp_token++;
						*temp_token = NOT_EQUAL;
						temp_token++;
						*temp_token = '\0';
					}
					break;
				case '<':
					if (*(source_code_location + 1) == '=')
					{
						source_code_location++;
						source_code_location++;
						*temp_token = LOWER_OR_EQUAL;
						temp_token++;
						*temp_token = LOWER_OR_EQUAL;
					}
					else
					{
						source_code_location++;
						*temp_token = LOWER;
					}
					temp_token++;
					*temp_token = '\0';
					break;
				case '>':
					if (*(source_code_location + 1) == '=')
					{
						source_code_location++;
						source_code_location++;
						*temp_token = GREATER_OR_EQUAL;
						temp_token++;
						*temp_token = GREATER_OR_EQUAL;
					}
					else
					{
						source_code_location++;
						*temp_token = GREATER;
					}
					temp_token++;
					*temp_token = '\0';
					break;
			}
			if (*current_token)
				return (token_type = DELIMITER);
		}

		if (strchr("+-*^/%=;(),'", *source_code_location))
		{ /* delimiter */
			*temp_token = *source_code_location;
			source_code_location++; /* advance to next position */
			temp_token++;
			*temp_token = '\0';
			return (token_type = DELIMITER);
		}

		if (*source_code_location == '"')
		{ /* Обработка строки в кавычках */
			source_code_location++;
			while ((*source_code_location != '"' &&
					*source_code_location != '\r' &&
					*source_code_location != '\n' &&
					*source_code_location != '\0') ||
				   (*source_code_location == '"' &&
					*(source_code_location - 1) == '\\'))
				*temp_token++ = *source_code_location++;

			if (*source_code_location == '\r' || *source_code_location == '\n' || *source_code_location == '\0')
				syntax_error(SYNTAX);
			source_code_location++;
			*temp_token = '\0';
			str_replace(current_token, "\\a", "\a");
			str_replace(current_token, "\\b", "\b");
			str_replace(current_token, "\\f", "\f");
			str_replace(current_token, "\\n", "\n");
			str_replace(current_token, "\\r", "\r");
			str_replace(current_token, "\\t", "\t");
			str_replace(current_token, "\\v", "\v");
			str_replace(current_token, "\\\\", "\\");
			str_replace(current_token, "\\\'", "\'");
			str_replace(current_token, "\\\"", "\"");
			return (token_type = STRING);
		}

		// передаем на вход начало строки, где мы находимся и проверям не число ли это.
		// если число, то считываем его до тех пор пока не встретим разделитель
		if (isdigit((int)*source_code_location))
		{ /* number */
			while (!is_delimiter(*source_code_location))
				*temp_token++ = *source_code_location++;
			*temp_token = '\0';
			return (token_type = NUMBER);
		}

		// передаем на вход начало строки, где мы находимся и проверям не буква ли это.
		// если буква, то считываем его до тех пор пока не встретим разделитель
		// так мы сможем выделить команду или название переменной.
		if (isalpha((int)*source_code_location))
		{ /* var or command */
			while (!is_delimiter(*source_code_location))
				*temp_token++ = *source_code_location++;
			token_type = TEMP;
		}

		*temp_token = '\0';

		/* see if a string is a command or a variable */
		if (token_type == TEMP)
		{
			current_tok_datatype = look_up_token_in_table(current_token); /* convert to internal rep */
			// если тип не 0, то есть токен является специальным словом
			// иначе же это переменная
			if (current_tok_datatype)
				token_type = KEYWORD; /* is a keyword */
			else
				token_type = VARIABLE;
		}
		return token_type;
	}
	/**
	 * Ищет текущий токен в специальной таблице с зарезервированными словами
	 * @param token_string
	 * @return
	 */
	char look_up_token_in_table(char *token_string)
	{
		int i;
		char *pointer_to_token_string;

		/* переводим токен в нижний регистр */
		pointer_to_token_string = token_string;
		while (*pointer_to_token_string)
		{
			*pointer_to_token_string = (char)tolower(*pointer_to_token_string);
			pointer_to_token_string++;
		}

		/* проверяем есть ли данный токен в таблице специальных зарезервированных слов. */
		for (i = 0; *table_with_statements[i].command; i++)
		{
			if (!strcmp(table_with_statements[i].command, token_string))
				return table_with_statements[i].tok;
		}
		return 0; /* unknown command */
	}
	/**
	 * Return 1 if c is space or tab.
	 * @param c
	 * @return
	 */
	static int is_whitespace(char c)
	{
		if (c == ' ' || c == '\t')
			return 1;
		else
			return 0;
	}
	/**
	 * @brief Разделитель
	 *
	 * Проверяет является ли символ разделителем
	 *
	 * @param Символ
	 *
	 * @return 1 - разделитель
	 * 0 - не разделитель
	 */
	static int is_delimiter(char c)
	{
		if (strchr(" !;,+-<>'/*%^=()", c) || c == 9 ||
			c == '\r' || c == '\n' || c == 0)
			return 1;
		return 0;
	}
	/**
	 * Модификация на месте находит и заменяет строку
	 *
	 * Предполагается, что буфер, на который указывает строка, достаточно велик для хранения результирующей строки
	 * @param line
	 * @param search
	 * @param replace
	 */
	static void str_replace(char *line, const char *search, const char *replace)
	{
		char *sp;
		while ((sp = strstr(line, search)) != nullptr)
		{
			int search_len = (int)strlen(search);
			int replace_len = (int)strlen(replace);
			int tail_len = (int)strlen(sp + search_len);

			memmove(sp + replace_len, sp + search_len, tail_len + 1);
			memcpy(sp, replace, replace_len);
		}
	}
	/**
	 * Return the entry point of the specified function
	 * @param name
	 * @return NULL if not found
	 */
	char *find_function_in_function_table(char *name)
	{
		int function_pos;

		for (function_pos = 0; function_pos < function_position; function_pos++)
			if (!strcmp(name, function_table[function_pos].func_name))
				return function_table[function_pos].loc;

		return nullptr;
	}


	/// TODO код ниже вынести(?) в отдельный наследуемый класс интерпретатор
	/// TODO вытащить последний ретерн и вернуть тут!
	/* Call a function. */
	void call_function()
	{
		char *function_location, *temp_source_code_location;
		int lvartemp;

		function_location = find_function_in_function_table(current_token); /* find entry point of function */
		if (function_location == nullptr)
			syntax_error(FUNC_UNDEFINED); /* function not defined */
		else
		{
			lvartemp = lvartos;								  /* save local var stack index */
			get_function_arguments();						  /* get function arguments */
			temp_source_code_location = source_code_location; /* save return location */
			function_push_variables_on_call_stack(lvartemp);  /* save local var stack index */
			source_code_location = function_location;		  /* reset prog to start of function */
			ret_occurring = 0;								  /* P the return occurring variable */
			get_function_parameters();						  /* load the function's parameters with the values of the arguments */
			interpret_block();								  /* interpret the function */
			ret_occurring = 0;								  /* Clear the return occurring variable */
			source_code_location = temp_source_code_location; /* reset the program initial_source_code_location */
			lvartos = func_pop();							  /* reset the local var stack */
		}
	}
	/**
	 * Push the arguments to a function onto the local variable stack
	 */
	void get_function_arguments()
	{
		int value, count, temp[NUM_PARAMS];
		struct variable_type i;

		count = 0;
		get_next_token();
		if (*current_token != '(')
			syntax_error(PAREN_EXPECTED);

		/* process a comma-separated list of values */
		while (*current_token == ',')
		{
			eval_expression(&value);
			temp[count] = value; /* save temporarily */
			get_next_token();
			count++;
		}
		count--;
		/* now, push on local_var_stack in reverse order */
		for (; count >= 0; count--)
		{
			i.variable_value = temp[count];
			i.variable_type = ARG;
			local_push(i);
		}
	}
	/* Парсер хуярсер. */
	void eval_expression(int *value)
	{
		get_next_token();
		if (!*current_token)
		{
			syntax_error(NO_EXP);   //no expression
			return;
		}
		if (*current_token == ';')
		{
			*value = 0; /* empty expression */
			return;
		}
		eval_assignment_expression(value);
		shift_source_code_location_back(); /* return last current_token read to input stream */
	}
	/* Process an assignment expression */
	void eval_assignment_expression(int *value)
	{
		char temp[ID_LEN]; /* holds name of var receiving
                          the assignment */
		char temp_tok;

		if (token_type == VARIABLE)
		{
			if (is_variable(current_token))
			{ /* Если встретили переменную, то проверяем, присваивается ли ей какое-либо значение */
				strcpy_s(temp, ID_LEN, current_token);
				temp_tok = token_type;
				get_next_token();
				if (*current_token == '=')
				{ /* если присваивается */
					get_next_token();
					eval_assignment_expression(value); /* то смотрим, что надо присвоить */
					assign_var(temp, *value);          /* присваиваем */
					return;
				}
				else
				{                                      /* если не присваевается */
					shift_source_code_location_back(); /* то забываем про temp и копируем изначальное значение токена из temp_tok */
					strcpy_s(current_token, 80, temp);
					token_type = temp_tok;
				}
			}
		}
		eval_exp1(value);
	}
	/**
	 * Determine if an identifier is a variable
	 * @param s
	 * @return 1 if variable is found; 0 otherwise
	 */
	int is_variable(char *s)
	{
		int i;

		/* first, see if it's a local variable */
		for (i = lvartos - 1; i >= call_stack[function_last_index_on_call_stack - 1]; i--)
			if (!strcmp(local_var_stack[i].variable_name, current_token))
				return 1;

		/* otherwise, try global vars */
		for (i = 0; i < NUM_GLOBAL_VARS; i++)
			if (!strcmp(global_vars[i].variable_name, s))
				return 1;

		return 0;
	}
	/**
	 * Assign a value to a variable
	 * @param var_name
	 * @param value
	 */
	void assign_var(char *var_name, int value)
	{
		int i;

		/* first, see if it's a local variable */
		for (i = lvartos - 1; i >= call_stack[function_last_index_on_call_stack - 1]; i--)
		{
			if (!strcmp(local_var_stack[i].variable_name, var_name))
			{
				local_var_stack[i].variable_value = value;
				return;
			}
		}
		if (i < call_stack[function_last_index_on_call_stack - 1])
			/* if not local, try global var table_with_statements */
			for (i = 0; i < NUM_GLOBAL_VARS; i++)
				if (!strcmp(global_vars[i].variable_name, var_name))
				{
					global_vars[i].variable_value = value;
					return;
				}
		syntax_error(NOT_VAR); /* variable not found */
	}
	/**
	 * Process relational operators
	 * @param value
	 */
	void eval_exp1(int *value)
	{
		int partial_value;
		char op;
		char relops[7] = {
				LOWER,
				LOWER_OR_EQUAL,
				GREATER,
				GREATER_OR_EQUAL,
				EQUAL,
				NOT_EQUAL,
				0};

		eval_exp2(value);
		op = *current_token;
		if (strchr(relops, op))
		{
			get_next_token();
			eval_exp2(&partial_value);
			switch (op)
			{ /* perform the relational operation */
				case LOWER:
					*value = *value < partial_value;
					break;
				case LOWER_OR_EQUAL:
					*value = *value <= partial_value;
					break;
				case GREATER:
					*value = *value > partial_value;
					break;
				case GREATER_OR_EQUAL:
					*value = *value >= partial_value;
					break;
				case EQUAL:
					*value = *value == partial_value;
					break;
				case NOT_EQUAL:
					*value = *value != partial_value;
					break;
			}
		}
	}
	/**
	 * Add or subtract two terms
	 * @param value
	 */
	void eval_exp2(int *value)
	{
		char op;
		int partial_value;

		eval_exp3(value);
		while ((op = *current_token) == '+' || op == '-')
		{
			get_next_token();
			eval_exp3(&partial_value);
			switch (op)
			{ /* add or subtract */
				case '-':
					*value = *value - partial_value;
					break;
				case '+':
					*value = *value + partial_value;
					break;
			}
		}
	}
	/**
	 * Multiply or divide two factors
	 * @param value
	 */
	void eval_exp3(int *value)
	{
		char op;
		int partial_value, t;

		eval_exp4(value);
		while ((op = *current_token) == '*' || op == '/' || op == '%')
		{
			get_next_token();
			eval_exp4(&partial_value);
			switch (op)
			{ /* mul, div, or modulus */
				case '*':
					*value = *value * partial_value;
					break;
				case '/':
					if (partial_value == 0)
						syntax_error(DIV_BY_ZERO);
					*value = (*value) / partial_value;
					break;
				case '%':
					t = (*value) / partial_value;
					*value = *value - (t * partial_value);
					break;
			}
		}
	}
	/**
	 * Is a unary + or -
	 * @param value
	 */
	void eval_exp4(int *value)
	{
		char op;

		op = '\0';
		if (*current_token == '+' || *current_token == '-')
		{
			op = *current_token;
			get_next_token();
		}
		eval_exp5(value);
		if (op)
			if (op == '-')
				*value = -(*value);
	}
	/**
	 * Process parenthesized expression
	 * @param value
	 */
	void eval_exp5(int *value)
	{
		if (*current_token == '(')
		{
			get_next_token();
			eval_assignment_expression(value); /* get subexpression */
			if (*current_token != ')')
				syntax_error(PAREN_EXPECTED);
			get_next_token();
		}
		else
			atom(value);
	}
	/**
	 * Find value of number, variable, or function
	 * @param value
	 */
	void atom(int *value)
	{
		int i;

		switch (token_type)
		{
			case VARIABLE:
				/*
				i = internal_func(current_token);
				if (i != -1)
				{
					*value = (*intern_func[i].p)();
				}
				else*/
				if (find_function_in_function_table(current_token))
				{ /* call user-defined function */
					call_function();
					*value = ret_value;
				}
				else
					*value = find_var(current_token); /* get var's value */
				get_next_token();
				return;
			case NUMBER: /* is numeric constant */
				*value = atoi(current_token);
				get_next_token();
				return;
			case DELIMITER: /* see if character constant */
				if (*current_token == '\'')
				{
					*value = *source_code_location;
					source_code_location++;
					if (*source_code_location != '\'')
						syntax_error(QUOTE_EXPECTED);
					source_code_location++;
					get_next_token();
					return;
				}
				if (*current_token == ')')
					return; /* process empty expression */
				else
					syntax_error(SYNTAX); /* syntax error */
			default:
				syntax_error(SYNTAX); /* syntax error */
		}
	}
	/**
	 * Return index of internal library function or -1 if not found
	 * @param s
	 * @return
	 */
	int internal_func(char *s)
	{
		int i;

		for (i = 0; intern_func[i].f_name[0]; i++)
		{
			if (!strcmp(intern_func[i].f_name, s))
				return i;
		}
		return -1;
	}
	/**
	 * Find the value of a variable
	 * @param s
	 * @return
	 */
	int find_var(char *s)
	{
		int i;

		/* first, see if it's a local variable */
		for (i = lvartos - 1; i >= call_stack[function_last_index_on_call_stack - 1]; i--)
			if (!strcmp(local_var_stack[i].variable_name, current_token))
				return local_var_stack[i].variable_value;

		/* otherwise, try global vars */
		for (i = 0; i < NUM_GLOBAL_VARS; i++)
			if (!strcmp(global_vars[i].variable_name, s))
				return global_vars[i].variable_value;

		syntax_error(NOT_VAR); /* variable not found */
		return -1;
	}
	/**
	 * Push a local variable
	 * @param i
	 */
	void local_push(struct variable_type i)
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
	/**
	 * Push index of local variable stack.
	 * добавляет локальные переменные функции в стек
	 */
	void function_push_variables_on_call_stack(int i)
	{
		if (function_last_index_on_call_stack >= NUMBER_FUNCTIONS)
		{
			syntax_error(NESTED_FUNCTIONS);
		}
		else
		{
			call_stack[function_last_index_on_call_stack] = i;
			function_last_index_on_call_stack++;
		}
	}
	/**
	 * Get function parameters.
	 */
	void get_function_parameters()
	{
		struct variable_type *variable_type_pointer;
		int position;

		position = lvartos - 1;
		do
		{ /* process comma-separated list of parameters */
			get_next_token();
			variable_type_pointer = &local_var_stack[position];
			if (*current_token != ')')
			{
				if (current_tok_datatype != INT && current_tok_datatype != CHAR)
					syntax_error(TYPE_EXPECTED);

				variable_type_pointer->variable_type = token_type;
				get_next_token();

				/* link parameter name with argument already on
				   local var stack */
				strcpy_s(variable_type_pointer->variable_name, ID_LEN, current_token);
				get_next_token();
				position--;
			}
			else
				break;
		} while (*current_token == ',');
		if (*current_token != ')')
			syntax_error(PAREN_EXPECTED);
	}
	/**
	 * Interpret a single statement or block of code.
	 * When interpret_block() returns from its initial call,
	 * the final brace (or a return) in main() has been encountered.
	 */
	void interpret_block()
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
			if (token_type == VARIABLE)
			{
				/* Not a keyword, so process expression. */
				shift_source_code_location_back(); /* restore current_token to input stream for
						  further processing by eval_exp() */
				eval_expression(&value);		   /* process the expression */
				if (*current_token != ';')
					syntax_error(SEMICOLON_EXPECTED);
			}
			else if (token_type == BLOCK)
			{							   /* if block delimiter */
				if (*current_token == '{') /* is a block */
					block = 1;			   /* interpreting block, not statement */
				else
					return; /* is a }, so return */
			}
			else /* is keyword */
				switch (current_tok_datatype)
				{
					case CHAR:
					case INT: /* declare local variables */
						shift_source_code_location_back();
						declare_local_variables();
						break;
					case RETURN: /* return from function call */
						function_return();
						ret_occurring = 1;
						return;
					case CONTINUE: /* continue loop execution */
						return;
					case BREAK: /* break loop execution */
						break_occurring = 1;
						return;
					case IF: /* process an if statement */
						execute_if_statement();
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
		} while (current_tok_datatype != FINISHED && block);
	}
	/**
	 * Declare a local variable
	 */
	void declare_local_variables()
	{
		struct variable_type i;

		get_next_token(); /* get type */

		i.variable_type = current_tok_datatype;
		i.variable_value = 0; /* init to 0 */

		do
		{					  /* process comma-separated list */
			get_next_token(); /* get var name */
			strcpy_s(i.variable_name, ID_LEN, current_token);
			local_push(i);
			get_next_token();
		} while (*current_token == ',');
		if (*current_token != ';')
			syntax_error(SEMICOLON_EXPECTED);
	}
	/**
	 * Return from a function
	 */
	void function_return()
	{
		int value;

		value = 0;
		/* get return value, if any */
		eval_expression(&value);

		ret_value = value;
	}
	/* Execute an if statement. */
	void execute_if_statement()
	{
		int condition;

		eval_expression(&condition); /* get if expression */

		if (condition)
		{ /* is true so process target of IF */
			interpret_block();
		}
		else
		{				/* otherwise skip around IF block and
					process the ELSE, if present */
			find_eob(); /* find start of next line */
			get_next_token();

			if (current_tok_datatype != ELSE)
			{
				shift_source_code_location_back(); /* restore current_token if
						  no ELSE is present */
				return;
			}
			interpret_block();
		}
	}
	/* Find the end of a block. */
	void find_eob()
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
	/* Execute a while loop. */
	void exec_while()
	{
		int cond;
		char *temp;

		break_occurring = 0; /* clear the break flag */
		shift_source_code_location_back();
		temp = source_code_location; /* save location of top of while loop */
		get_next_token();
		eval_expression(&cond); /* check the conditional expression */
		if (cond)
		{
			interpret_block(); /* if true, interpret */
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
	void exec_do()
	{
		int cond;
		char *temp;

		shift_source_code_location_back();
		temp = source_code_location; /* save location of top of do loop */
		break_occurring = 0;		 /* clear the break flag */

		get_next_token();  /* get start of loop */
		interpret_block(); /* interpret loop */
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
		if (current_tok_datatype != WHILE)
			syntax_error(WHILE_EXPECTED);
		eval_expression(&cond); /* check the loop condition */
		if (cond)
			source_code_location = temp; /* if true loop; otherwise,
					   continue on */
	}
	/* Execute a for loop. */
	void exec_for()
	{
		int cond;
		char *temp, *temp2;
		int brace;

		break_occurring = 0; /* clear the break flag */
		get_next_token();
		eval_expression(&cond); /* initialization expression */
		if (*current_token != ';')
			syntax_error(SEMICOLON_EXPECTED);
		source_code_location++; /* get past the ; */
		temp = source_code_location;
		for (;;)
		{
			eval_expression(&cond); /* check the condition */
			if (*current_token != ';')
				syntax_error(SEMICOLON_EXPECTED);
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
				interpret_block(); /* if true, interpret */
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
			eval_expression(&cond);		 /* do the increment */
			source_code_location = temp; /* loop back to top */
		}
	}
	/* Pop index into local variable stack. */
	int func_pop(void)
	{
		int index = 0;
		function_last_index_on_call_stack--;
		if (function_last_index_on_call_stack < 0)
		{
			syntax_error(RET_NOCALL);
		}
		else if (function_last_index_on_call_stack >= NUMBER_FUNCTIONS)
		{
			syntax_error(NESTED_FUNCTIONS);
		}
		else
		{
			index = call_stack[function_last_index_on_call_stack];
		}

		return index;
	}



	/// TODO вынести все стандартные функции в наследуемый класс
	/// TODO Разобраться с определением массив с ссылками на стандартные функции
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
	/* Аналог printf() */
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


	struct intern_func_type
	{
		char *f_name;   /* имя функции */
		int (*p)(); /* указатель на функцию */
	} intern_func[6];
};

class Parser
{
public:

};

int main()
{
	LittleC program("test.c");
	/*
	 * Чек-лист
	 * - Загрузка в память
	 * - Предварительный проход
	 * - Проверка на main
	 * - Исполнение функций
	 */
	program.execute();

	return 0;
}