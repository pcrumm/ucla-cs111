// UCLA CS 111 Lab 1 command reading

#include "command.h"
#include "command-internals.h"
#include "alloc.h"

#include <error.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

/* FIXME: You may need to add #include directives, macro definitions,
   static function definitions, etc.  */

struct command_stream
{
  // An array of command pointers that are parsed from the stream.
  command_t *commands;

  // For iteration (obviously), holds the numerical index of the last traversed location.
  int iterator;

  // The number of commands we've placed in our stream
  int stream_size;

  // The number of spaces we've made available to add commands (through malloc). This should match
  // stream_size when we're all said and done.
  int alloc_size;
};

void
free_command (command_t c)
{
  free(c->input);
  free(c->output);

  switch (c->type)
  {
    case AND_COMMAND:
    case SEQUENCE_COMMAND:
    case OR_COMMAND:
    case PIPE_COMMAND:
      free_command(c->u.command[0]);
      free_command(c->u.command[1]);
      break;
    case SIMPLE_COMMAND:
      {
        char** w = c->u.word;
        while (*w)
          free(*w++);

        free(c->u.word);
        break;
      }
    case SUBSHELL_COMMAND:
      free_command(c->u.subshell_command);
      break;
    default: break;
  }

  free(c);
}

bool
is_valid_token (char *expr)
{
  // Alphanumeric and space characters (or NULL pointer) are NOT tokens
  if(isalnum (expr[0]) || isspace (expr[0]) || !expr)
    return false;

  // Checking for multi-char tokens AND_COMMAND_STR and OR_COMMAND_STR
  if( (expr[0] == '&' && expr[1] == '&') || (expr[0] == '|' && expr[1] == '|') )
    return true;

  switch(expr[0])
    {
      case SEQUENCE_COMMAND_CHAR:
      case PIPE_COMMAND_CHAR:
      case SUBSHELL_COMMAND_CHAR_OPEN:
      case SUBSHELL_COMMAND_CHAR_CLOSE:
      case FILE_IN_CHAR:
      case FILE_OUT_CHAR:
        return true;
        break;
      case '\0':
        return false;
        break;
      default:
        break;
    }

  return false;
}

char*
rev_find_token (char *expr, enum command_type type)
{
  char *p = expr;

  // Find the end of the string
  while (*p)
    p++;

  while (expr >= p)
    {
      bool is_valid = is_valid_token (p);
      enum command_type cur_type;

      // If '|' is found, check that the token isnt really '||'
      if(is_valid && *p == PIPE_COMMAND && is_valid_token (p - 1))
          p--;

      cur_type = convert_token_to_command_type (p);

      // Return whenever the current token type matches the desired type
      // Since both '(' and ')' are marked as SUBSHELL_COMMAND we explicitly
      // check for ONLY '(' tokens
      if(cur_type == type)
      {
        if(type == SUBSHELL_COMMAND)
          {
            if(*p == SUBSHELL_COMMAND_CHAR_OPEN)
              return p;
          }
        else
          {
            return p;
          }
      }

      p--;
    }

    return NULL;
}

char*
get_pivot_token (char *expr)
{
  // Search for tokens using lowest precedence first
  char *token;

  // Checking ';'
  token = rev_find_token (expr, SEQUENCE_COMMAND);

  // Checking equal precedence '&&' and '||'
  if(token == NULL)
    {
      char *and_token, *or_token;

      and_token = rev_find_token (expr, AND_COMMAND);
      or_token  = rev_find_token (expr, OR_COMMAND);

      if(and_token != NULL && or_token != NULL)
        token = (and_token > or_token ? or_token : and_token);
      else if(and_token == NULL)
        token = or_token;
      else
        token = and_token;
    }

    // Checking '|'
    if(token == NULL)
      token = rev_find_token (expr, PIPE_COMMAND);

    // @todo check '<' and '>'

    // Checking '()'
    if(token == NULL)
      token = rev_find_token (expr, SUBSHELL_COMMAND);

    return token;
}

char**
split_expression_by_token (char *expr, char token)
{
  char *p = expr;
  size_t size = 1; // At least 1 element exists
  size_t index = 0;

  while (*p)
    {
      if(*p == token)
        size++;

      p++;
    }

  char** array = checked_malloc (sizeof (char*) * size);

  p = strtok (expr, &token);

  while (p)
    {
      char *new_str = checked_malloc (sizeof (char) * strlen (p));
      strcpy (new_str, p);

      array[index++] = new_str;
      p = strtok (NULL, &token);
    }

  array[index] = NULL;

  return array;
}

enum command_type
convert_token_to_command_type (char* token)
{
  if(token[0] == token[1] && token[0] == '&')
    return AND_COMMAND;

  if(token[0] == token[1] && token[0] == '|')
    return OR_COMMAND;

  switch (*token)
  {
    case SEQUENCE_COMMAND_CHAR:
      return SEQUENCE_COMMAND;
    case PIPE_COMMAND_CHAR:
      return PIPE_COMMAND;
    case SUBSHELL_COMMAND_CHAR_OPEN:
    case SUBSHELL_COMMAND_CHAR_CLOSE:
      return SUBSHELL_COMMAND;
    default: break;
  }

  return SIMPLE_COMMAND;
}

command_t
make_command_from_expression (char *expr)
{
  return NULL;
}

/**
 * Reads through a stream and assembles a command_stream by decomposing
 * the stream into individual commands.
 *
 * int (*get_next_byte) (void *) - Function pointer to get_next_byte
 * void *get_next_byte_argument - The stream to iterate through.
 */
command_stream_t
make_command_stream (int (*get_next_byte) (void *),
         void *get_next_byte_argument)
{
  /* FIXME: Replace this with your implementation.  You may need to
     add auxiliary functions and otherwise modify the source code.
     You can also use external functions defined in the GNU C Library.  */
  error (1, 0, "command reading not yet implemented");
  return 0;
}

/**
 * Iterates over a command_stream struct and returns the next command.
 * Sample usage: main.c:56
 */
command_t
read_command_stream (command_stream_t s)
{
  if (s->iterator == s->stream_size)
  {
    s->iterator = 0;
    return NULL;
  }

  return s->commands[s->iterator++];
}
