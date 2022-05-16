/**
 * @brief Типы токенов
 */
enum token_types
{
	DELIMITER,
	VARIABLE,
	NUMBER,
	KEYWORD,
	TEMP,
	STRING,
	BLOCK
};
/**
 * @brief Ключевые слова
 */
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
/**
 * @brief Операторы отношений
 */
enum double_ops
{
	LOWER = 1,
	LOWER_OR_EQUAL,
	GREATER,
	GREATER_OR_EQUAL,
	EQUAL,
	NOT_EQUAL
};
/**
 * @brief Виды ошибок
 */
enum error_msg
{
	SYNTAX,
	UNBAL_PARENS,
	NO_EXP,
	EQUALS_EXPECTED,
	NOT_VAR,
	PARAM_ERR,
	SEMICOLON_EXPECTED,
	UNBAL_BRACES,
	FUNC_UNDEFINED,
	TYPE_EXPECTED,
	NESTED_FUNCTIONS,
	RET_NOCALL,
	PAREN_EXPECTED,
	WHILE_EXPECTED,
	QUOTE_EXPECTED,
	NOT_TEMP,
	TOO_MANY_LVARS,
	DIV_BY_ZERO
};