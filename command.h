// UCLA CS 111 Lab 1 command interface

#include <stdbool.h>
#include <stdio.h> // For size_t

#define SEQUENCE_COMMAND_CHAR       ';'
#define PIPE_COMMAND_CHAR           '|'
#define SUBSHELL_COMMAND_CHAR_OPEN  '('
#define SUBSHELL_COMMAND_CHAR_CLOSE ')'
#define FILE_IN_CHAR                '<'
#define FILE_OUT_CHAR               '>'
#define COMMENT_CHAR                '#'
#define NEWLINE_CHAR                '\n'

#define AND_COMMAND_STR             "&&"
#define OR_COMMAND_STR              "||"

typedef struct command *command_t;
typedef struct command_stream *command_stream_t;

enum command_type;

/* Create a command stream from GETBYTE and ARG.  A reader of
   the command stream will invoke GETBYTE (ARG) to get the next byte.
   GETBYTE will return the next input byte, or a negative number
   (setting errno) on failure.  */
command_stream_t make_command_stream (int (*getbyte) (void *), void *arg);

/* Read a command from STREAM; return it, or NULL on EOF.  If there is
   an error, report the error and exit instead of returning.  */
command_t read_command_stream (command_stream_t stream);

/* Print a command to stdout, for debugging.  */
void print_command (command_t);

/* Execute a command.  Use "time travel" if the flag is set.  */
void execute_command (command_t, bool);

/* Return the exit status of a command, which must have previously
   been executed.  Wait for the command, if it is not already finished.  */
int command_status (command_t);

/**
 * Checks whether the character pointed to by expr is a valid token. In the case
 * of multichar tokens such as "&&", the next character is checked as well.
 */
bool is_valid_token (char const *expr);

/**
 * Returns a pointer to the next valid token. If no token is
 * found, a pointer to the end of the string is returned.
 */
char const * get_next_valid_token (char const *expr);

/**
 * Searches the expression in reverse for the specified token.
 * A NULL return value indicates token was not found.
 */
char const * rev_find_token (char const *expr, const enum command_type type);

/**
 * Finds and returns a pointer to the right-most lowest precedence operator found
 * (since same precedence tokens are left-associative).
 *
 * Precedence implemented in decending order is:
 * subshell -> pipe -> and/or -> sequence
 *
 * Redirect precedence would come after subshells and before pipes. This function
 * does not take file redirects into account, as they are better handled when
 * building SIMPLE_COMMANDs and SUBSHELL_COMMANDs.
 *
 * A null return value indicates no tokens found (ie SIMPLE_COMMAND).
 */
char const * get_pivot_token (char const *expr);

/**
 * Replaces one character with another within a string
 */
void replace_char (char** p_string, char old_char, char new_char);

/**
 * Splits expr by the specified token and returns an array of char*
 */
char** split_expression_by_token (char const *expr, char token);

/**
 * Checks for a single redirect token, as specified by redirect_type (which is the
 * redirect token itself) and updates the command. Then the original expression is copied
 * and the redirects are stripped from the copy. This new (mutable) string is then returned.
 * It is the duty of the caller to free orig_expr at the appropriate time.
 */
char* handle_and_strip_single_file_redirect (char const * const orig_expr, command_t cmd, char redirect_type);

/**
 * Searches for any file redirects and updates the command struct as appropriate.
 * The expression is copied and the redirect tokens and their respective destinations
 * are then removed from the copy, which gets returned. It is the duty of the caller to
 * free expr.
 */
char* handle_and_strip_file_redirects (char const * const expr, command_t cmd);

/**
 * Frees all memory associated with a command
 */
void free_command (command_t c);

/**
 * Converts any char token equivalent of the command_type enum
 * into the enum type. If no token is found SIMPLE_COMMAND is
 * returned. FILE_IN_CHAR and FILE_OUT_CHAR ('<' and '>', respectively)
 * are NOT currently handled and SIMPLE_COMMAND will be returned.
 */
enum command_type convert_token_to_command_type (char const *token);

/**
 * Recursively analyses expression and builds the nested command_t structs.
 * It assumes that input expressions are valid, thus validation checks should
 * be performed outside.
 */
command_t make_command_from_expression (const char * const expr);

/**
 * Analyzes a given expression string. Returns true if valid, false if not.
 * If the expression is invalid, we'll also set expr_line_number, which is the
 * line number /within the expression/ that the error occurred on.
 */
bool is_valid_expression (const char *expr, int *expr_line_number);

/**
 * Converts the specified expression to a command through make_command_from_expression
 * and adds it to the specified command stream.
 */
void add_expression_to_stream (const char *expr, command_stream_t stream);

/**
 * Add the given string to the expression buffer and resize the buffer if necessary.
 */
char* add_char_to_expression (char c, char *expr, size_t *expr_utilized, size_t *expr_size);

/**
 * Check if the expression consists of a token at the specified point.
 */
bool token_ends_at_point (const char *expr, size_t point);

/**
 * Checks if a character is within the word character set:
 * Alphanum, ! % + , - . / : @ ^ _
 */
bool is_valid_word_char (char c);

/**
 * Write an error to the standard output and exit.
 */
void show_error(int line_number, char *desc);