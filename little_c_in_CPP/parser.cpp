/* Recursive descent parser for integer expressions
   which may include variables and function calls.
*/

#include <setjmp.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define NUMBER_FUNCTIONS 100
#define NUM_GLOBAL_VARS 100
#define NUM_LOCAL_VARS 200
#define ID_LEN 32
#define FUNC_CALLS 31
#define PROG_SIZE 10000
#define FOR_NEST 31

// Secure function compatibility
#if !defined(_MSC_VER) || _MSC_VER < 1400
#define strcpy_s(dest, count, source) strncpy((dest), (source), (count))
#endif

enum token_types //константы к которым обращаемся при нахождении нужных совпадений в коде / для хранения данных в буфере
{
    DELIMITER,  // !;,+-<>'/*%^= ()
    VARIABLE,
    NUMBER, 
    KEYWORD,
    TEMP,   //буфер для хранения части кода при проверках
    STRING, //строки или то что надо преобразовать в строку
    BLOCK   //блок кода в операторных скобках
};

enum tokens //тут храним операторы
{
    ARG,    //аргумент  
    CHAR,   //тупо для типа данных chfr
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
    EOL,    //end of line 
    FINISHED,
    END
};

enum double_ops
{   /*операторы отношений*/
    LOWER = 1,  //меньше
    LOWER_OR_EQUAL, //меньше или равно
    GREATER,    //больше
    GREATER_OR_EQUAL,   //больше или равно
    EQUAL,  //эквивалентно
    NOT_EQUAL   //не эквивалентно
};

/* These are the constants used to call syntax_error() when
   a syntax error occurs. Add more if you like.
   NOTE: SYNTAX is a generic error message used when
   nothing else seems appropriate.
*/
enum error_msg //коды ошибок
{
    SYNTAX,
    UNBAL_PARENS,
    NO_EXP,
    EQUALS_EXPECTED,
    NOT_VAR,
    PARAM_ERR,
    SEMICOLON_EXPECTED, //потерялась точка с запятой ;
    UNBAL_BRACES,
    FUNC_UNDEFINED, //неверно определена функция
    TYPE_EXPECTED,  //не задан тип переменной или функции
    NESTED_FUNCTIONS,   //функция определена внутри другой функции
    RET_NOCALL, //функция ничего не возвращает
    PAREN_EXPECTED, //где-то не закрыли или не открыли скобки
    WHILE_EXPECTED,
    QUOTE_EXPECTED,
    NOT_TEMP,
    TOO_MANY_LVARS, //Переименовать в TOO_MANY_LOCAL_VARS. Слишком много локальных переменных ?
    DIV_BY_ZERO
};

extern char *source_code_location; /* указатель на исполняемый кусок кода */
extern char *program_start_buffer; /* указатель на начало программы в буфере исполнения  */
extern jmp_buf execution_buffer;   /* указатель на указатель longjmp() */

/* здесь хранится инфа о глобальных переменных
* тип данных, значение, имя
*/
extern struct variable_type
{
    char variable_name[ID_LEN];
    int variable_type;
    int variable_value;
} global_vars[NUM_GLOBAL_VARS];

/*  Хранит тип возвращаемых данных, название функции, местоположение в коде. */
extern struct function_type
{
    char func_name[ID_LEN];
    int ret_type;
    char *loc; /* точка вхождения начала функции в буфере программы */
} func_stack[NUMBER_FUNCTIONS];

/* таблица состояний */
extern struct commands
{
    char command[20];
    char tok;
} table_with_statements[];

/* "Standard library" functions are declared here so
   they can be put into the internal function table_with_statements that
   follows.
 */
int call_getche(void), call_putch(void);    //getchar(), putchar
int call_puts(void), print(void), getnum(void); 

struct intern_func_type
{
    char *f_name;   /* имя функции */
    int (*p)(void); /* указатель на функцию */
} intern_func[] = {
    {"getche", call_getche},
    {"putch", call_putch},
    {"puts", call_puts},
    {"print", print},
    {"getnum", getnum},
    {"", 0} /* null terminate the list */
};

extern char current_token[80];    /* string representation of current_token */
extern char token_type;           /* contains type of current_token */
extern char current_tok_datatype; /* internal representation of current_token */

extern int ret_value; /* function return value */

                      /*Вот эти штуки стоит переименовать подстать тому что они делают*/

void eval_assignment_expression(int *value);    //Присваивание значения переменной
void eval_expression(int *value);   
void eval_exp1(int *value);     //обработка операторов отношений 
void eval_exp2(int *value);     //обработка сложения или вычитания (в том числе инкремент и декремент, как я понял)
void eval_exp3(int *value);     //обработка умножения, деления, целочисленного деления
void eval_exp4(int *value);     //унарные плюс и минус
void eval_exp5(int *value);     //обработка выражений в скобках
void atom(int *value);      //найти значение числа, переменной или функции

#if defined(_MSC_VER) && _MSC_VER >= 1200
__declspec(noreturn) void syntax_error(int error);
#elif __GNUC__
void syntax_error(int error) __attribute((noreturn));
#else
void syntax_error(int error);
#endif

void shift_source_code_location_back(void); //при ошибке возвращает указатель на место в коде, где все работало нормально
void assign_var(char *var_name, int value);
int is_delimiter(char c), is_whitespace(char c);
int find_var(char *s);
int internal_func(char *s); //обработка вложенных функций
int is_variable(char *s);
char *find_function_in_function_table(char *name), look_up_token_in_table(char *s), get_next_token(void);
void call_function(void);
static void str_replace(char *line, const char *search, const char *replace);

/* Парсер. */
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
    register char temp_tok;

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

/* Process relational operators. */
void eval_exp1(int *value)
{
    int partial_value;
    register char op;
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

/*  Add or subtract two terms. */
void eval_exp2(int *value)
{
    register char op;
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

/* Multiply or divide two factors. */
void eval_exp3(int *value)
{
    register char op;
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

/* Is a unary + or -. */
void eval_exp4(int *value)
{
    register char op;

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

/* Process parenthesized expression. */
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

/* Find value of number, variable, or function. */
void atom(int *value)
{
    int i;

    switch (token_type)
    {
    case VARIABLE:
        i = internal_func(current_token);
        if (i != -1)
        { /* call "standard library" function */
            *value = (*intern_func[i].p)();
        }
        else if (find_function_in_function_table(current_token))
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

/* Display an error_type message. */
void syntax_error(int error_type)
{
    char *program_pointer_location, *temp;
    int line_count = 0;
    register int i;

    static char *errors_human_readable[] = {    //репрезентация ошибок анализатора в понятном для человека виде 
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
        "На ноль делить НЕЛЬЗЯ"};
    printf("\n%s", errors_human_readable[error_type]);

    program_pointer_location = program_start_buffer;

    /*В очередной раз напишу, что почти все эти считывания конца строки можно заменить устарвшим и забытым
    Спецсимволом SUB из бейсика. 
    Насколько мне известно, плюсы его тоже понимают, потому что недалеко ушли от оригинального СИ*/
    while (program_pointer_location != source_code_location && *program_pointer_location != '\0')
    { /* find line number of error_type */
        program_pointer_location++;
        if (*program_pointer_location == '\r')
        {
            line_count++;
            if (program_pointer_location == source_code_location)
            {
                break;
            }
            /* See if this is a Windows or Mac newline */
            program_pointer_location++;
            /* If we are a mac, backtrack */
            if (*program_pointer_location != '\n')
            {
                program_pointer_location--;
            }
        }
        else if (*program_pointer_location == '\n')
        {
            line_count++;
        }
        else if (*program_pointer_location == '\0')
        {
            line_count++;
        }
    }
    printf(" in line %d\n", line_count);

    temp = program_pointer_location--;
    for (i = 0; i < 20 && program_pointer_location > program_start_buffer && *program_pointer_location != '\n' && *program_pointer_location != '\r'; i++, program_pointer_location--)
        ;
    for (i = 0; i < 30 && program_pointer_location <= temp; i++, program_pointer_location++)
        printf("%c", *program_pointer_location);

    longjmp(execution_buffer, 1); /* в данном случае буфер указывает на начало функции. 
                                  Если он есть, значит в начале функция работала. 
                                  Иначе он был бы до начала функции.
                                  Проще говоря, он указывает на последний рабочий кусок кода*/
}

/* Get a current_token. */
char get_next_token(void)
{

    register char *temp_token;

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

/* Return a current_token to input stream. */
// передвигаем указатель на текущую программу на *_токен_* обратно
void shift_source_code_location_back(void)
{
    char *t;

    t = current_token;
    for (; *t; t++)
        source_code_location--;
}

/* ищет текущий токен в специальной таблице с зарезервированными словами
 */
char look_up_token_in_table(char *token_string)
{
    register int i;
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

/* Return index of internal library function or -1 if
   not found.
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

/* Return true if c is a delimiter. */
int is_delimiter(char c)
{
    if (strchr(" !;,+-<>'/*%^=()", c) || c == 9 ||
        c == '\r' || c == '\n' || c == 0)
        return 1;
    return 0;
}

/* Return 1 if c is space or tab. */
int is_whitespace(char c)
{
    if (c == ' ' || c == '\t')
        return 1;
    else
        return 0;
}

/* Модификация на месте находит и заменяет строку.
    Предполагается, что буфер, на который указывает строка, достаточно велик для хранения результирующей строки.*/
static void str_replace(char *line, const char *search, const char *replace)
{
    char *sp;
    while ((sp = strstr(line, search)) != NULL)
    {
        int search_len = (int)strlen(search);
        int replace_len = (int)strlen(replace);
        int tail_len = (int)strlen(sp + search_len);

        memmove(sp + replace_len, sp + search_len, tail_len + 1);
        memcpy(sp, replace, replace_len);
    }
}
