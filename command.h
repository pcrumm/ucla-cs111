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
void print_command (command_t, bool);

/* Execute a single command tree. */
void execute_command (command_t);

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
 * Splits expr by the specified token and sets cmd->u.word. Additionally it will
 * increment the line number count based on any encountered newline characters.
 * Since newlines can only be valid between a token and a word (i.e. before the start
 * of any words in this context) or after the end of all words (i.e. after the last
 * word in this context) these locations are the only ones checked. Otherwise it is
 * up to the validation phase to handle improper newlines.
 */
void split_expression_by_token (command_t cmd, char const *expr, char token, int * const p_line_number);

/**
 * Checks for a single redirect token, as specified by redirect_type (which is the
 * redirect token itself) and updates the command. Then the original expression is copied
 * and the redirects are stripped from the copy. This new (mutable) string is then returned.
 * It is the duty of the caller to free orig_expr at the appropriate time.
 */
char* handle_and_strip_single_file_redirect (char const * const orig_expr, command_t cmd, char redirect_type, bool is_subshell_command);

/**
 * Searches for any file redirects and updates the command struct as appropriate.
 * The expression is copied and the redirect tokens and their respective destinations
 * are then removed from the copy, which gets returned. It is the duty of the caller to
 * free expr.
 */
char* handle_and_strip_file_redirects (char const * const expr, command_t cmd, bool is_subshell_command);

/**
 * Frees all memory associated with a command
 */
void free_command (command_t c);

/**
 * Frees all memory associated with a command_stream, including
 * all command trees.
 */
void free_command_stream (command_stream_t cmd_stream);

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
 * be performed outside. It accepts a pointer to a integer used for tracking
 * which line a command is found. Since the function is recursive, it is the
 * caller's duty to provide a non-NULL location for the counter.
 */
command_t recursive_build_command_from_expression (const char * const expr, int * const p_line_number);

/**
 * Builds a command tree from an expression. A public wrapper function
 * for recursive_build_command_from_expression which hosts the location
 * for the line_number counter.
 */
command_t make_command_from_expression (const char * const expr, int line_number);

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
void add_expression_to_stream (const char *expr, command_stream_t stream, int line_number);

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
 * Write an error to the standard output and exit for syntax errors.
 */
void show_syntax_error (int line_number, char *desc);

/**
 * Write an error to the standard output and exit for any errors.
 */
void show_error (int line_number, char *desc, char *desc2);

/**
 * Recursively analyzes an expression and verifies that there is no more than
 * one input redirect < between any set of tokens and that no redirects in any
 * set of tokens appear in this order: > <
 */
bool expression_redirect_order_is_valid (const char *expr, int *line_number);

/**
 * EXECUTION
 */

/**
 * Executes a simple command. If the command cannot be executed, we will
 * display an error and call exit--the program will not return to the caller.
 *
 * Piping rules: if the command being executed has a file redirect, it has higher
 * precedence than piping, therefore, file redirects overwrite any specified
 * file descriptors.
 *
 * If the command has a value of -1 set for fd_read_from the command will default
 * to reading to STDIN. Otherwise it will attempt to read from the specified
 * file descriptor. If a value of false is passed in for pipe_output the command
 * will default to writing to STDOUT. Otherwise it will open up a pipe and specify
 * the READ end of the pipe by setting its fd_writing_to field.
 *
 * It is up to the caller to then close the open file descriptors, as well as issue
 * a system wait on the forked process for it's exit status.
 */
void execute_simple_command (command_t c, bool pipe_output);

/**
 * Searches the current working directory, then the system path, for the
 * specified binary. Returns an empty string if we aren't able to find
 * what we're looking for.
 *
 * If an absolute or relative path is specified (preced by / or ., respectively),
 * we do no searching and simply check if the file exists.
 */
char* get_executable_path (char* bin_name);

/**
 * As stated, determine if a file exists at the specified path.
 */
bool file_exists (char *path);

/**
 * A function that recursively traverses the command tree and executes the commands
 * based on the tokens. Any unpiped commands' output will directly be written to stdout.
 * Any piped commands will open a pipe between the two commands.
 *
 * In the case of nested piping, file descriptors will be closed and forked processes
 * waited on after the outermost pipe has been set up. Thus we avoid deadlock in between
 * waiting on a nested pipe to exit while the outermost pipe is not being read from.
 */
void recursive_execute_command (command_t c, bool pipe_output);

/**
 * Frees up CPU resources as a result of executing a command.
 * Any file descriptors opened by SIMPLE_COMMANDs will be closed. Other commands which
 * are parents of SIMPLE_COMMANDs will have copied their file descriptors for reference,
 * but as they should not be opening them, they will NOT be closed for non-SIMPLE_COMMANDs.
 * Moreover, if the SIMPLE_COMMAND is still executed (denoted by a status of -1 and a
 * positive pid) a wait will be issued on the pid and the status saved and propagated to
 * any parent as appropriate.
 *
 * At the end of the function the status will be properly set, the pid will be -1, and the
 * file descriptors closed and set to -1 as well.
 *
 * Calling this function will effectively block the process until the child command has
 * exited.
 */
void close_command_exec_resources (command_t c);

/**
 * Equivalent to the `exec` keyword in bash: the current process is replaced with a
 * command, which is the first argument to exec. Any specified redirect are applied
 * to the process before it is replaced.
 *
 * This command does not return.
 */
void exec_utility (command_t c);

/**
 * Finds a file for use with input or output redirection. Checks the cwd and as an
 * absolute path.
 */
char* get_redirect_file_path (char *redirect_file);

/**
 * Executes command while parallelizing commands that do not share dependencies
 */
int timetravel (command_stream_t c_stream);

/**
 * Checks whether dep has any dependencies based on indep. For example, if dep reads
 * from a file with the same name as an argument to indep, or to one of indep's I/O
 * files, it is dependent on it
 *
 * A true return value indicates a dependence is present.
 */
bool check_dependence (command_t indep, command_t dep);
