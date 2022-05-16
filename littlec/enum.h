/**
 * @brief Операторы
 */
enum tokens
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

/**
 * @brief Операторы отношений
 */
enum double_ops
{
	LOWER = 1,  //меньше
	LOWER_OR_EQUAL, //меньше или равно
	GREATER,    //больше
	GREATER_OR_EQUAL,   //больше или равно
	EQUAL,  //эквивалентно
	NOT_EQUAL   //не эквивалентно
};

/**
 * @brief Типы токенов
 */
enum token_types
{
	DELIMITER, //!; , +-< >'/*%^= ()
	VARIABLE,
	NUMBER,
	KEYWORD,
	TEMP,
	STRING,
	/// Блоки кода, которые находятся в обработке. Что то вроде темпа только для других задач
	BLOCK
};

/**
 * @brief Виды ошибок
 */
enum error_msg
{
	/// Синтаксическая ошибка
	SYNTAX,
	/// Несбалансированные скобки ()
	UNBAL_PARENS,
	/// Нет выражения
	NO_EXP,
	/// Ожидались операторы сравнения
	EQUALS_EXPECTED,
	/// Не является переменной
	NOT_VAR,
	/// Ошибка в функции
	PARAM_ERR,
	/// Не поставили точку с запятой ;
	SEMICOLON_EXPECTED,
	/// Слишком много или не хватает операторных скобок {}
	UNBAL_BRACES,
	/// Неправильно описали функцию
	FUNC_UNDEFINED,
	/// Не указали тип данных
	TYPE_EXPECTED,
	NESTED_FUNCTIONS,
	RET_NOCALL,
	/// Потеряли одну из скобок ()
	PAREN_EXPECTED,
	WHILE_EXPECTED,
	QUOTE_EXPECTED,
	NOT_STRING,
	TOO_MANY_LVARS,
	/// На ноль делить нельзя блеать
	DIV_BY_ZERO
};