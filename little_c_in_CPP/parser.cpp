/* Recursive descent parser for integer expressions
   which may include variables and function calls.
*/

#include <setjmp.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define NUM_FUNC 100
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
    DELIMITER,
    IDENTIFIER,
    NUMBER,
    KEYWORD,
    TEMP,
    STRING,
    BLOCK
};

enum tokens //тут храним операторы
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

enum double_ops
{
    LOWER = 1,
    LOWER_OR_EQUAL,
    GREATER,
    GREATER_OR_EQUAL,
    EQUAL,
    NOT_EQUAL
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

extern char *source_code_location; /* current location in source code */
extern char *program_start_buffer; /* points to start of program buffer */
extern jmp_buf execution_buffer;   /* hold environment for longjmp() */

/* An array of these structures will hold the info
   associated with global variables.
*/
extern struct variable_type
{
    char variable_name[ID_LEN];
    int variable_type;
    int variable_value;
} global_vars[NUM_GLOBAL_VARS];

/*  This is the function call stack. */
extern struct function_type
{
    char func_name[ID_LEN];
    int ret_type;
    char *loc; /* location of function entry point in file */
} func_stack[NUM_FUNC];

/* Keyword table_with_statements */
extern struct commands
{
    char command[20];
    char tok;
} table_with_statements[];

/* "Standard library" functions are declared here so
   they can be put into the internal function table_with_statements that
   follows.
 */
int call_getche(void), call_putch(void);
int call_puts(void), print(void), getnum(void);

struct intern_func_type
{
    char *f_name;   /* function name */
    int (*p)(void); /* pointer to the function */
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

void eval_exp0(int *value);
void eval_exp(int *value);
void eval_exp1(int *value);
void eval_exp2(int *value);
void eval_exp3(int *value);
void eval_exp4(int *value);
void eval_exp5(int *value);
void atom(int *value);
#if defined(_MSC_VER) && _MSC_VER >= 1200
__declspec(noreturn) void syntax_error(int error);
#elif __GNUC__
void syntax_error(int error) __attribute((noreturn));
#else
void syntax_error(int error);
#endif

void shift_source_code_location_back(void);
void assign_var(char *var_name, int value);
int is_delimiter(char c), is_whitespace(char c);
int find_var(char *s);
int internal_func(char *s);
int is_var(char *s);
char *find_function_in_function_table(char *name), look_up_token_in_table(char *s), get_next_token(void);
void call(void);
static void str_replace(char *line, const char *search, const char *replace);

/* Entry point into parser. */
void eval_exp(int *value)
{
    get_next_token();
    if (!*current_token)
    {
        syntax_error(NO_EXP);
        return;
    }
    if (*current_token == ';')
    {
        *value = 0; /* empty expression */
        return;
    }
    eval_exp0(value);
    shift_source_code_location_back(); /* return last current_token read to input stream */
}

/* Process an assignment expression */
void eval_exp0(int *value)
{
    char temp[ID_LEN]; /* holds name of var receiving
                          the assignment */
    register char temp_tok;

    if (token_type == IDENTIFIER)
    {
        if (is_var(current_token))
        { /* if a var, see if assignment */
            strcpy_s(temp, ID_LEN, current_token);
            temp_tok = token_type;
            get_next_token();
            if (*current_token == '=')
            { /* is an assignment */
                get_next_token();
                eval_exp0(value);         /* get value to assign */
                assign_var(temp, *value); /* assign the value */
                return;
            }
            else
            {                                      /* not an assignment */
                shift_source_code_location_back(); /* restore original current_token */
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
        LOWER, LOWER_OR_EQUAL, GREATER, GREATER_OR_EQUAL, EQUAL, NOT_EQUAL, 0};

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
        eval_exp0(value); /* get subexpression */
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
    case IDENTIFIER:
        i = internal_func(current_token);
        if (i != -1)
        { /* call "standard library" function */
            *value = (*intern_func[i].p)();
        }
        else if (find_function_in_function_table(current_token))
        { /* call user-defined function */
            call();
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

/* Display an error message. */
void syntax_error(int error)
{
    char *p, *temp;
    int linecount = 0;
    register int i;

    static char *e[] = {
        "syntax error",
        "unbalanced parentheses",
        "no expression present",
        "equals sign expected",
        "not a variable",
        "parameter error",
        "semicolon expected",
        "unbalanced braces",
        "function undefined",
        "type specifier expected",
        "too many nested function calls",
        "return without call",
        "parentheses expected",
        "while expected",
        "closing quote expected",
        "not a string",
        "too many local variables",
        "division by zero"};
    printf("\n%s", e[error]);
    p = program_start_buffer;
    while (p != source_code_location && *p != '\0')
    { /* find line number of error */
        p++;
        if (*p == '\r')
        {
            linecount++;
            if (p == source_code_location)
            {
                break;
            }
            /* See if this is a Windows or Mac newline */
            p++;
            /* If we are a mac, backtrack */
            if (*p != '\n')
            {
                p--;
            }
        }
        else if (*p == '\n')
        {
            linecount++;
        }
        else if (*p == '\0')
        {
            linecount++;
        }
    }
    printf(" in line %d\n", linecount);

    temp = p--;
    for (i = 0; i < 20 && p > program_start_buffer && *p != '\n' && *p != '\r'; i++, p--)
        ;
    for (i = 0; i < 30 && p <= temp; i++, p++)
        printf("%c", *p);

    longjmp(execution_buffer, 1); /* return to safe point */
}

/* Get a current_token. */
char get_next_token(void)
{

    register char *temp_token;

    token_type = 0;
    current_tok_datatype = 0;

    temp_token = current_token;
    *temp_token = '\0';

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
        { /* найти конец комментария */
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
    { /* quoted string */
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
            token_type = IDENTIFIER;
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
