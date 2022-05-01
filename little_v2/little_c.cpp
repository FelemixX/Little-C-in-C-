#pragma hdrstop
#pragma argsused
#include <cstdio>	 //printf()
#include <csetjmp>	 //setjmp()
#include <string>	 //strcpy()
#include <windows.h> //CharToOem()
#include <cctype>	 //isdigit()
#include <cstdlib>	 //exit()
#include <conio.h>	 //getch()
//#include <math.h>   //?
//---------------------------------------------------------------------------
#define PROG_SIZE 10000
#define ID_LEN 31
#define NUM_FUNC 100
#define NUM_GLOBAL_VARS 100
#define NUM_PARAMS 31
#define NUM_LOCAL_VARS 200

enum tokens
{
	lcARG,
	lcCHAR,
	lcINT,
	lcIF,
	lcELSE,
	lcFOR,
	lcDO,
	lcWHILE,
	lcSWITCH,
	lcRETURN,
	lcEOL,
	lcFINISHED,
	lcEND
};

enum tok_types
{
	lcDELIMITER,
	lcIDENTIFIER,
	lcNUMBER,
	lcKEYWORD,
	lcTEMP,
	lcSTRING,
	lcBLOCK
};
enum double_ops
{
	LT = 1,
	LE,
	GT,
	GE,
	EQ,
	NE
};

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
struct func_type
{
	char func_name[ID_LEN];
	int ret_type;
	char *loc;
} func_table[NUM_FUNC];

struct commands
{
	char command[20];
	char ltok;
} table[] = {
	"if", lcIF,
	"else", lcELSE,
	"for", lcFOR,
	"do", lcDO,
	"while", lcWHILE,
	"char", lcCHAR,
	"int", lcINT,
	"return", lcRETURN,
	"end", lcEND,
	"", lcEND};

struct var_type
{
	char var_name[32];
	int v_type;
	int value;
} global_vars[NUM_GLOBAL_VARS];

struct var_type local_var_stack[NUM_LOCAL_VARS];
//---------------------------------------------------------------------------
char *p_buf;
jmp_buf e_buf;
char *prog;
int gvar_index;
int func_index;
int call_stack[NUM_FUNC];
int functos;

int ret_value;
int lvartos;
char token[80]; //строковое представление лексемы main print call_puts putback get_token atom eval_exp5 eval_exp4 eval_exp3 eval_exp2 eval_exp1 eval_exp0 eval_exp exec_for find_eob is_var find_var get_params get_args call decl_local decl_global interp_block prescan
char token_type;
char tok;

int load_program(char *p, char *fname);
void prescan(void);
void interp_block(void);
char *find_func(char *name);
void decl_global(void);
void decl_local(void);
void call(void);
void get_args(void);
void get_params(void);
void func_ret(void);
void local_push(struct var_type i);
int func_pop(void);
void func_push(int i);
void assign_var(char *var_name, int value);
int find_var(char *s);
int is_var(char *s);
void exec_if(void);
void exec_while(void);
void exec_do(void);
void find_eob(void);
void exec_for(void);

int get_token(void);
void putback(void);
int iswhite(char c);
void sntx_err(int error);
int isdelim(char c);
int look_up(char *s);
void eval_exp(int *value);
void eval_exp0(int *value);
void eval_exp1(int *value);
void eval_exp2(int *value);
void eval_exp3(int *value);
void eval_exp4(int *value);
void eval_exp5(int *value);
void atom(int *value);
int internal_func(char *s);

char *cto(char *);

int call_getche(void);
int call_putch(void);
int call_puts(void);
int print(void);
int getnum(void);

struct intern_func_type
{
	char *f_name;
	int (*p)();
} intern_func[] = {
	"getche", call_getche,
	"putch", call_putch,
	"puts", call_puts,
	"print", print,
	"getnum", getnum,
	"", 0};

int main(int argc, char *argv[])
{ // ;
	if (argc != 2)
	{
		//Применение: littlec <имя_файла>
		printf(cto("Применение: littlec <имя_файла>\n"));
		getch();
		exit(1);
	}
	if ((p_buf = (char *)malloc(PROG_SIZE)) == NULL)
	{
		printf(cto("Выделить память не удалось"));
		getch();
		exit(1);
	}

	if (!load_program(p_buf, argv[1]))
	{
		getch();
		exit(1);
	}
	if (setjmp(e_buf))
	{
		getch();
		exit(1);
	} // инициализация буфера long jump

	gvar_index = 0;

	prog = p_buf;
	prescan();

	lvartos = 0;
	functos = 0;

	prog = find_func("main");

	if (!prog)
	{
		printf(cto("main() не найдена.\n"));
		getch();
		exit(1);
	}

	prog--;
	strcpy(token, "main");
	call();
	getch();
	return 0;
}

int load_program(char *p, char *fname)
{
	FILE *fp;
	int i = 0;

	if ((fp = fopen(fname, "rb")) == NULL)
		return 0;

	i = 0;
	do
	{
		*p = getc(fp);
		p++;
		i++;
	} while (!feof(fp) && i < PROG_SIZE);

	if (*(p - 2) == 0x1a)
		*(p - 2) = '\0';

	else
		*(p - 1) = '\0';
	fclose(fp);
	return 1;
}

void prescan(void)
{
	char *p, *tp;
	char temp[32];
	int datatype;
	int brace = 0;

	p = prog;
	func_index = 0;
	do
	{
		while (brace)
		{
			get_token();
			if (*token == '{')
				brace++;
			if (*token == '}')
				brace--;
		}

		tp = prog;
		get_token();

		if (tok == lcCHAR || tok == lcINT)
		{
			datatype = tok;
			get_token();
			if (token_type == lcIDENTIFIER)
			{
				strcpy(temp, token);
				get_token();
				if (*token != '(')
				{
					prog = tp;
					decl_global();
				}
				else if (*token == '(')
				{
					func_table[func_index].loc = prog;
					func_table[func_index].ret_type = datatype;
					strcpy(func_table[func_index].func_name, temp);
					func_index++;
					while (*prog != ')')
						prog++;
					prog++;
				}
				else
					putback();
			}
		}
		else if (*token == '{')
			brace++;
	} while (tok != lcFINISHED);
	prog = p;
}

void interp_block(void)
{
	int value;
	char block = 0;

	do
	{
		token_type = get_token();

		if (token_type == lcIDENTIFIER)
		{
			putback();
			eval_exp(&value);
			if (*token != ';')
				sntx_err(SEMI_EXPECTED);
		}
		else if (token_type == lcBLOCK)
		{
			if (*token == '{')
				block = 1;
			else
				return;
		}
		else
			switch (tok)
			{
			case lcCHAR:
			case lcINT:
				putback();
				decl_local();
				break;
			case lcRETURN:
				func_ret();
				return;
			case lcIF:
				exec_if();
				break;
			case lcELSE:
				find_eob();

				break;
			case lcWHILE:
				exec_while();
				break;
			case lcDO:
				exec_do();
				break;
			case lcFOR:
				exec_for();
				break;
			case lcEND:
				exit(0);
			}
	} while (tok != lcFINISHED && block);
}

char *find_func(char *name)
{
	int i;

	for (i = 0; i < func_index; i++)
		if (!strcmp(name, func_table[i].func_name))
			return func_table[i].loc;

	return NULL;
}

void decl_global(void)
{
	int vartype;

	get_token();

	vartype = tok;

	do
	{ // обработка списка
		global_vars[gvar_index].v_type = vartype;
		global_vars[gvar_index].value = 0;
		get_token();
		strcpy(global_vars[gvar_index].var_name, token);
		get_token();
		gvar_index++;
	} while (*token == ',');
	if (*token != ';')
		sntx_err(SEMI_EXPECTED);
}

void decl_local(void)
{
	struct var_type i;

	get_token();

	i.v_type = tok;
	i.value = 0;

	do
	{
		get_token();
		strcpy(i.var_name, token);
		local_push(i);
		get_token();
	} while (*token == ',');
	if (*token != ';')
		sntx_err(SEMI_EXPECTED);
}

void call(void)
{
	char *loc, *temp;
	int lvartemp;

	loc = find_func(token);
	if (loc == NULL)
		sntx_err(FUNC_UNDEF);
	else
	{
		lvartemp = lvartos;
		get_args();
		temp = prog;
		func_push(lvartemp);
		prog = loc;
		get_params();
		interp_block();
		prog = temp;
		lvartos = func_pop();
	}
}
void get_args(void)
{
	int value, count, temp[NUM_PARAMS];
	struct var_type i;

	count = 0;
	get_token();
	if (*token != '(')
		sntx_err(PAREN_EXPECTED);

	do
	{
		eval_exp(&value);
		temp[count] = value;
		get_token();
		count++;
	} while (*token == ',');
	count--;

	for (; count >= 0; count--)
	{
		i.value = temp[count];
		i.v_type = lcARG;
		local_push(i);
	}
}

void get_params(void)
{
	struct var_type *p;
	int i;

	i = lvartos - 1;
	do
	{
		get_token();
		p = &local_var_stack[i];
		if (*token != ')')
		{
			if (tok != lcINT && tok != lcCHAR)
				sntx_err(TYPE_EXPECTED);

			p->v_type = token_type;
			get_token();

			strcpy(p->var_name, token);
			get_token();
			i--;
		}
		else
			break;
	} while (*token == ',');
	if (*token != ')')
		sntx_err(PAREN_EXPECTED);
}

void func_ret(void)
{
	int value;

	value = 0;

	eval_exp(&value);

	ret_value = value;
}

void local_push(struct var_type i)
{
	if (lvartos > NUM_LOCAL_VARS)
		sntx_err(TOO_MANY_LVARS);

	local_var_stack[lvartos] = i;
	lvartos++;
}

int func_pop(void)
{
	functos--;
	if (functos < 0)
		sntx_err(RET_NOCALL);
	return call_stack[functos];
}

void func_push(int i)
{
	if (functos > NUM_FUNC)
		sntx_err(NEST_FUNC);
	call_stack[functos] = i;
	functos++;
}

void assign_var(char *var_name, int value)
{
	int i;

	for (i = lvartos - 1; i >= call_stack[functos - 1]; i--)
	{
		if (!strcmp(local_var_stack[i].var_name, var_name))
		{
			local_var_stack[i].value = value;
			return;
		}
	}
	if (i < call_stack[functos - 1])

		for (i = 0; i < NUM_GLOBAL_VARS; i++)
			if (!strcmp(global_vars[i].var_name, var_name))
			{
				global_vars[i].value = value;
				return;
			}
	sntx_err(NOT_VAR);
}

int find_var(char *s)
{
	int i;

	for (i = lvartos - 1; i >= call_stack[functos - 1]; i--)
		if (!strcmp(local_var_stack[i].var_name, token))
			return local_var_stack[i].value;

	for (i = 0; i < NUM_GLOBAL_VARS; i++)
		if (!strcmp(global_vars[i].var_name, s))
			return global_vars[i].value;

	sntx_err(NOT_VAR);
	return -1;
}

int is_var(char *s)
{
	int i;

	for (i = lvartos - 1; i >= call_stack[functos - 1]; i--)
		if (!strcmp(local_var_stack[i].var_name, token))
			return 1;

	for (i = 0; i < NUM_GLOBAL_VARS; i++)
		if (!strcmp(global_vars[i].var_name, s))
			return 1;

	return 0;
}

void exec_if(void)
{
	int cond;

	eval_exp(&cond);

	if (cond)
	{
		interp_block();
	}
	else
	{
		find_eob();
		get_token();

		if (tok != lcELSE)
		{
			putback();
			return;
		}
		interp_block();
	}
}

void exec_while(void)
{
	int cond;
	char *temp;

	putback();
	temp = prog;
	get_token();
	eval_exp(&cond);
	if (cond)
		interp_block();
	else
	{
		find_eob();
		return;
	}
	prog = temp;
}

void exec_do(void)
{
	int cond;
	char *temp;

	putback();
	temp = prog;

	get_token();
	interp_block();
	get_token();
	if (tok != lcWHILE)
		sntx_err(WHILE_EXPECTED);
	eval_exp(&cond);
	if (cond)
		prog = temp;
}

void find_eob(void)
{
	int brace;

	get_token();
	brace = 1;
	do
	{
		get_token();
		if (*token == '{')
			brace++;
		else if (*token == '}')
			brace--;
	} while (brace);
}

void exec_for(void)
{
	int cond;
	char *temp, *temp2;
	int brace;

	get_token();
	eval_exp(&cond);
	if (*token != ';')
		sntx_err(SEMI_EXPECTED);
	prog++;
	temp = prog;
	for (;;)
	{
		eval_exp(&cond);
		if (*token != ';')
			sntx_err(SEMI_EXPECTED);
		prog++;
		temp2 = prog;

		brace = 1;
		while (brace)
		{
			get_token();
			if (*token == '(')
				brace++;
			if (*token == ')')
				brace--;
		}

		if (cond)
			interp_block();
		else
		{
			find_eob();
			return;
		}
		prog = temp2;
		eval_exp(&cond);
		prog = temp;
	}
}

void eval_exp(int *value)
{
	get_token();
	if (!*token)
	{
		sntx_err(NO_EXP);
		return;
	}
	if (*token == ';')
	{
		*value = 0;
		return;
	}
	eval_exp0(value);
	putback();
}

void eval_exp0(int *value)
{
	char temp[ID_LEN];
	int temp_tok;

	if (token_type == lcIDENTIFIER)
	{
		if (is_var(token))
		{
			strcpy(temp, token);
			temp_tok = token_type;
			get_token();
			if (*token == '=')
			{
				get_token();
				eval_exp0(value);
				assign_var(temp, *value);
				return;
			}
			else
			{
				putback();
				strcpy(token, temp);
				token_type = temp_tok;
			}
		}
	}
	eval_exp1(value);
}

void eval_exp1(int *value)
{
	int partial_value;
	char op;
	char relops[7] = {
		LT, LE, GT, GE, EQ, NE, 0};

	eval_exp2(value);
	op = *token;
	if (strchr(relops, op))
	{
		get_token();
		eval_exp2(&partial_value);
		switch (op)
		{
		case LT:
			*value = *value < partial_value;
			break;
		case LE:
			*value = *value <= partial_value;
			break;
		case GT:
			*value = *value > partial_value;
			break;
		case GE:
			*value = *value >= partial_value;
			break;
		case EQ:
			*value = *value == partial_value;
			break;
		case NE:
			*value = *value != partial_value;
			break;
		}
	}
}

void eval_exp2(int *value)
{
	char op;
	int partial_value;

	eval_exp3(value);
	while ((op = *token) == '+' || op == '-')
	{
		get_token();
		eval_exp3(&partial_value);
		switch (op)
		{ // суммирование или вычитание
		case '-':
			*value = *value - partial_value;
			break;
		case '+':
			*value = *value + partial_value;
			break;
		}
	}
}

void eval_exp3(int *value)
{
	char op;
	int partial_value, t;

	eval_exp4(value);
	while ((op = *token) == '*' || op == '/' || op == '%')
	{
		get_token();
		eval_exp4(&partial_value);
		switch (op)
		{ // умножение, деление или деление целых
		case '*':
			*value = *value * partial_value;
			break;
		case '/':
			if (partial_value == 0)
				sntx_err(DIV_BY_ZERO);
			*value = (*value) / partial_value;
			break;
		case '%':
			t = (*value) / partial_value;
			*value = *value - (t * partial_value);
			break;
		}
	}
}

void eval_exp4(int *value)
{
	char op;

	op = '\0';
	if (*token == '+' || *token == '-')
	{
		op = *token;
		get_token();
	}
	eval_exp5(value);
	if (op)
		if (op == '-')
			*value = -(*value);
}

void eval_exp5(int *value)
{
	if ((*token == '('))
	{
		get_token();
		eval_exp0(value);
		if (*token != ')')
			sntx_err(PAREN_EXPECTED);
		get_token();
	}
	else
		atom(value);
}

void atom(int *value)
{
	int i;

	switch (token_type)
	{
	case lcIDENTIFIER:
		i = internal_func(token);
		if (i != -1)
		{ // вызов функции из "стандартной билиотеки"
			*value = (*intern_func[i].p)();
		}
		else if (find_func(token))
		{ // вызов функции,
			// определенной пользователем
			call();
			*value = ret_value;
		}
		else
			*value = find_var(token); // получение значения переменной
		get_token();
		return;
	case lcNUMBER: // числовая константа
		*value = atoi(token);
		get_token();
		return;
	case lcDELIMITER: // это символьная константа?
		if (*token == '\'')
		{
			*value = *prog;
			prog++;
			if (*prog != '\'')
				sntx_err(QUOTE_EXPECTED);
			prog++;
			get_token();
			return;
		}
		if (*token == ')')
			return; // обработка пустого выражения
		else
			sntx_err(SYNTAX); // синтаксическая ошибка
	default:
		sntx_err(SYNTAX); // синтаксическая ошибка
	}
}

void sntx_err(int error)
{
	char *p, *temp;
	int linecount = 1;
	int i;

	static char *e[] = {
		"синтаксическая ошибка",
		"несбалансированные скобки",
		"выражение отсутствует",
		"ожидается знак равенства",
		"не переменная",
		"ошибка в параметре",
		"ожидается точка с запятой",
		"несбалансированные фигурные скобки",
		"функция не определена",
		"ожидается спецификатор типа",
		"слишком много вложенных вызовов функций",
		"оператор return вне функции",
		"ожидаются скобки",
		"ожидается while",
		"ожидается закрывающаяся кавычка",
		"не строка",
		"слишком много локальных переменных",
		"деление на нуль"};
	printf("\n%s", cto(e[error]));
	p = p_buf;
	while (p != prog)
	{ //поиск номера строки с ошибкой
		p++;
		if (*p == '\r')
		{
			linecount++;
		}
	}
	printf(cto(" на линии %d\n"), linecount);

	temp = p;
	for (i = 0; i < 20 && p > p_buf && *p != '\n'; i++, p--)
		;
	for (i = 0; i < 30 && p <= temp; i++, p++)
		printf("%c", *p);

	longjmp(e_buf, 1); // возврат в безопасную точку
}

int get_token(void)
{

	char *temp;

	token_type = 0;
	tok = 0;

	temp = token;
	*temp = '\0';

	// пропуск пробелов, символов табуляции и пустой строки
	while (iswhite(*prog) && *prog)
		++prog;

	if (*prog == '\r')
	{
		++prog;
		++prog;
		// пропуск пробелов
		while (iswhite(*prog) && *prog)
			++prog;
	}

	if (*prog == '\0')
	{ // конец файла
		*token = '\0';
		tok = lcFINISHED;
		return (token_type = lcDELIMITER);
	}

	if (strchr("{}", *prog))
	{ // ограничение блока
		*temp = *prog;
		temp++;
		*temp = '\0';
		prog++;
		return (token_type = lcBLOCK);
	}

	// поиск комментариев
	if (*prog == '/')
		if (*(prog + 1) == '*')
		{ // это комментарий
			prog += 2;
			do
			{ // найти конец комментария
				while (*prog != '*')
					prog++;
				prog++;
			} while (*prog != '/');
			prog++;
		}

	if (strchr("!<>=", *prog))
	{ // возможно, это
		// оператор сравнения
		switch (*prog)
		{
		case '=':
			if (*(prog + 1) == '=')
			{
				prog++;
				prog++;
				*temp = EQ;
				temp++;
				*temp = EQ;
				temp++;
				*temp = '\0';
			}
			break;
		case '!':
			if (*(prog + 1) == '=')
			{
				prog++;
				prog++;
				*temp = NE;
				temp++;
				*temp = NE;
				temp++;
				*temp = '\0';
			}
			break;
		case '<':
			if (*(prog + 1) == '=')
			{
				prog++;
				prog++;
				*temp = LE;
				temp++;
				*temp = LE;
			}
			else
			{
				prog++;
				*temp = LT;
			}
			temp++;
			*temp = '\0';
			break;
		case '>':
			if (*(prog + 1) == '=')
			{
				prog++;
				prog++;
				*temp = GE;
				temp++;
				*temp = GE;
			}
			else
			{
				prog++;
				*temp = GT;
			}
			temp++;
			*temp = '\0';
			break;
		}
		if (*token)
			return (token_type = lcDELIMITER);
	}

	if (strchr("+-*^/%=;(),'", *prog))
	{ // разделитель
		*temp = *prog;
		prog++; // продвижение на следующую позицию
		temp++;
		*temp = '\0';
		return (token_type = lcDELIMITER);
	}

	if (*prog == '"')
	{ // строка в кавычках
		prog++;
		while (*prog != '"' && *prog != '\r')
			*temp++ = *prog++;
		if (*prog == '\r')
			sntx_err(SYNTAX);
		prog++;
		*temp = '\0';
		return (token_type = lcSTRING);
	}

	if (isdigit(*prog))
	{ // число
		while (!isdelim(*prog))
			*temp++ = *prog++;
		*temp = '\0';
		return (token_type = lcNUMBER);
	}

	if (isalpha(*prog))
	{ // переменная или оператор
		while (!isdelim(*prog))
			*temp++ = *prog++;
		token_type = lcTEMP;
	}

	*temp = '\0';

	//эта строка является оператором или переменной?
	if (token_type == lcTEMP)
	{
		tok = look_up(token); // преобразовать во внутренее представление
		if (tok)
			token_type = lcKEYWORD; // это зарезервированное слово
		else
			token_type = lcIDENTIFIER;
	}
	return token_type;
}

void putback(void)
{
	char *t;

	t = token;
	for (; *t; t++)
		prog--;
}

int look_up(char *s)
{
	int i;
	char *p;

	// преобразование в нижний регистр
	p = s;
	while (*p)
	{
		*p = tolower(*p);
		p++;
	}

	// есть ли лексемы в таблице?
	for (i = 0; *table[i].command; i++)
	{
		if (!strcmp(table[i].command, s))
			return table[i].ltok;
	}
	return 0; // незнакомый оператор
}

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

int isdelim(char c)
{
	if (strchr(" !;,+-<>'/*%^=()", c) || c == 9 ||
		c == '\r' || c == 0)
		return 1;
	return 0;
}

int iswhite(char c)
{
	if (c == ' ' || c == '\t')
		return 1;
	else
		return 0;
}

int call_getche()
{
	char ch;
	ch = getchar();
	while (*prog != ')')
		prog++;
	prog++; // продвижение к концу строки
	return ch;
}

int call_putch()
{
	int value;

	eval_exp(&value);
	printf("%c", value);
	return value;
}

int call_puts(void)
{
	get_token();
	if (*token != '(')
		sntx_err(PAREN_EXPECTED);
	get_token();
	if (token_type != lcSTRING)
		sntx_err(QUOTE_EXPECTED);
	puts(token);
	get_token();
	if (*token != ')')
		sntx_err(PAREN_EXPECTED);

	get_token();
	if (*token != ';')
		sntx_err(SEMI_EXPECTED);
	putback();
	return 0;
}

int getnum(void)
{
	char s[80];

	gets(s);
	while (*prog != ')')
		prog++;
	prog++; // продвижение к концу строки
	return atoi(s);
}

int print(void)
{
	int i;

	get_token();
	if (*token != '(')
		sntx_err(PAREN_EXPECTED);

	get_token();
	if (token_type == lcSTRING)
	{ // вывод строки
		// char *p;// = cto(token);
		//    OemToAnsi(token,p);
		printf("%s ", token);
	}
	else
	{ // вывод числа
		putback();
		eval_exp(&i);
		printf("%d ", i);
	}

	get_token();

	if (*token != ')')
		sntx_err(PAREN_EXPECTED);

	get_token();
	if (*token != ';')
		sntx_err(SEMI_EXPECTED);
	putback();
	return 0;
}

char *cto(char *s)
{
	char *c;
	CharToOem(s, c);
	return c;
}