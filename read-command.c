// UCLA CS 111 Lab 1 command reading

#include "command.h"
#include "command-internals.h"
#include "alloc.h"

#include <error.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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
make_command_from_expression (const char *expr)
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
  /**
   * A general overview of how we will proceed:
   *
   * First, we iterate over our stream. We will consider each line
   * read in to be an expression, which make_command_from_expression
   * will convert into usable form.
   *
   * The exception here is if a newline immediately follows a token:
   * in this case, we do not consider this the end of the command.
   *
   * Finally, we call our validation function, which will execute
   * the proper error condition if necessary. Once that's done, we can
   * make_command_from_expression and add it to our command stream, which
   * we return.
   */

   // Some numerical values for buffer management
   size_t expression_buffer_size = 1024;
   size_t current_expression_size = 0;

   // Our buffer that we'll read into.
   char *expression_buffer = checked_malloc (expression_buffer_size * sizeof (char));

   // Setup our command stream
   command_stream_t expression_stream = malloc (sizeof (struct command_stream));
   expression_stream->iterator = 0;
   expression_stream->stream_size = 0;
   expression_stream->alloc_size = 16;
   expression_stream->commands = malloc (expression_stream->alloc_size * sizeof (command_t));

   char current_char;

    // Anything between a % and a newline is a comment. We don't write them to our buffer.
   bool in_comment = false;

   while ((current_char = get_next_byte (get_next_byte_argument)) != EOF)
   {
    // If this is not a newline or a comment, just add it to the buffer...
    if (current_char != '\n' && current_char != '#' && !in_comment)
      add_char_to_expression (current_char, expression_buffer, &current_expression_size, &expression_buffer_size);

    // Comments
    else if (current_char == '#')
    {
      // If we're immediately preceded by a token, this is not a comment.
      if (token_ends_at_point (expression_buffer, current_expression_size))
        add_char_to_expression (current_char, expression_buffer, &current_expression_size, &expression_buffer_size);

      else
        in_comment = true;
    }

    // Deal with newlines
    else if (current_char == '\n')
    {
      // Firstly, comments: newlines end comments.
      if (in_comment)
      {
        add_char_to_expression (current_char, expression_buffer, &current_expression_size, &expression_buffer_size);
        in_comment = false;
      }

      // Next, handle cases where lines end with tokens. The expression continues.
      else if (token_ends_at_point (expression_buffer, current_expression_size))
        add_char_to_expression (current_char, expression_buffer, &current_expression_size, &expression_buffer_size);

      // We've found the end of an expression!
      else if (is_valid_expression (expression_buffer))
      {
        add_expression_to_stream (expression_buffer, expression_stream);

        // And reset everything to start again...
        free (expression_buffer);
        expression_buffer_size = 1024;
        current_expression_size = 0;
        
        expression_buffer = checked_malloc (expression_buffer_size * sizeof (char));
      }
    }
   }

   return expression_stream;
}

char*
add_char_to_expression (char c, char *expr, size_t *expr_utilized, size_t *expr_size)
{
  // Simply, add the next character
  expr[*expr_utilized++] = c;

  // If we've hit the size limit, time to add another 1024 bytes.
  if (*expr_utilized == *expr_size)
  {
    *expr_size += 1024;
    expr = checked_realloc (expr, *expr_size);
  }

  return expr;
}

bool
token_ends_at_point(const char *expr, size_t point)
{
  if (point == 0) // Can't start with a token.
    return false;

  // We want to know if point (or point and point-1) are a valid token.
  char *single_token = checked_malloc (sizeof (char) * 1);
  memcpy (single_token, expr + point, 1);

  if (is_valid_token (single_token))
  {
    free (single_token);
    return true;
  }

  // Let's see if there's a valid double token
  char *double_token = checked_realloc (single_token, sizeof (char) * 2);
  memcpy (single_token, expr + (point - 1), 2);

  if (is_valid_token (double_token))
  {
    // Figure out if this is a two-character token
    enum command_type type = convert_token_to_command_type (double_token);

    free (double_token); // We will return from here so make sure we clean up

    switch (type)
    {
      case AND_COMMAND:
      case OR_COMMAND:
        return true;
      default:
        return false;
    }
  }

  // If we got this far, we're not valid. Clean up and get out.
  free (double_token);
  return false;
}

void
add_expression_to_stream (const char *expr, command_stream_t stream)
{
  stream->commands[stream->stream_size++] = make_command_from_expression (expr);

  // Resize if we need to
  if (stream->stream_size == stream->alloc_size)
  {
    stream->alloc_size += 16;
    stream->commands = checked_realloc (stream->commands, stream->alloc_size * sizeof (command_t));
  }
}

bool
is_valid_expression (char *expr)
{
  return true; // @todo actually make this do something
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
