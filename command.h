// UCLA CS 111 Lab 1 command interface

#include <stdbool.h>

#define SEQUENCE_COMMAND_CHAR       ';'
#define PIPE_COMMAND_CHAR           '|'
#define SUBSHELL_COMMAND_CHAR_OPEN  '('
#define SUBSHELL_COMMAND_CHAR_CLOSE ')'
#define FILE_IN_CHAR                '<'
#define FILE_OUT_CHAR               '>'

#define AND_COMMAND_STR             "&&"
#define OR_COMMAND_STR              "||"

typedef struct command *command_t;
typedef struct command_stream *command_stream_t;

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
bool is_valid_token (char *expr);

/**
 * Returns a pointer to the next valid token. If no token is
 * found, a pointer to the end of the string is returned.
 */
char* get_next_valid_token (char *expr);

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
enum command_type convert_token_to_command_type (char* token);

/**
 * Recursively analyses expression and builds the nested command_t structs.
 * It assumes that input expressions are valid, thus validation checks should
 * be performed outside.
 */
command_t make_command_from_expression (char *expr);
