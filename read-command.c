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
  free (c->input);
  free (c->output);

  switch (c->type)
  {
    case AND_COMMAND:
    case SEQUENCE_COMMAND:
    case OR_COMMAND:
    case PIPE_COMMAND:
      free_command (c->u.command[0]);
      free_command (c->u.command[1]);
      break;
    case SIMPLE_COMMAND:
      {
        char** w = c->u.word;
        while (*w)
          free (*w++);

        free (c->u.word);
        break;
      }
    case SUBSHELL_COMMAND:
      free_command (c->u.subshell_command);
      break;
    default: break;
  }

  free (c);
}

bool
is_valid_token (char const *expr)
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

char const *
get_next_valid_token (char const *expr)
{
  if(expr == NULL)
    return NULL;

  expr++;

  while (*expr != '\0')
    {
      if(is_valid_token(expr))
        break;

      expr++;
    }

  return expr;
}

char const *
rev_find_token (char const *expr, const enum command_type type)
{
  char const *p = expr;

  // Find the end of the string
  while (*p)
    p++;

  while (p >= expr)
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

char const *
get_pivot_token (char const *expr)
{
  // Search for tokens using lowest precedence first
  char const *token;

  // Checking ';'
  token = rev_find_token (expr, SEQUENCE_COMMAND);

  // Checking equal precedence '&&' and '||'
  if(token == NULL)
    {
      char const *and_token;
      char const *or_token;

      and_token = rev_find_token (expr, AND_COMMAND);
      or_token  = rev_find_token (expr, OR_COMMAND);

      if(and_token != NULL && or_token != NULL)
        token = (and_token > or_token ? and_token : or_token);
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

void
replace_char (char** p_string, char old_char, char new_char)
{
  char *str = *p_string;

  while (*str != '\0')
    {
      if(*str == old_char)
        *str = new_char;

      str++;
    }
}

char**
split_expression_by_token (char const *expr, char token)
{
  char token_string[] = { token, '\0' };  // strtok expects a NULL delimited string, not just a char
  char *p;                                // Generic pointer to iterate through characters
  char *expr_copy;                        // Copy expr as strtok internally modifies strings so we copy expr
  size_t size = 1;                        // At least 1 element exists
  size_t index = 0;

  expr_copy = checked_malloc (sizeof (char) * (strlen (expr)+1));
  p = strcpy (expr_copy, expr);

  // Replacing all new lines in the expression
  replace_char (&expr_copy, '\n', token);

  // Consecutive tokens will result in allocating slightly larger than necessary memory
  while (*p)
    {
      if(*p == token)
        size++;

      p++;
    }

  char** array = checked_malloc (sizeof (char*) * (size+1));

  p = strtok (expr_copy, token_string);

  while (p)
    {
      char *new_str = checked_malloc (sizeof (char) * (strlen (p)+1));
      strcpy (new_str, p);

      array[index++] = new_str;
      p = strtok (NULL, token_string);
    }

  array[index] = NULL;

  free (expr_copy);
  return array;
}

char*
handle_and_strip_single_file_redirect (char const * const orig_expr, command_t cmd, char redirect_type)
{
  if(orig_expr == NULL)
    return NULL;

  char *expr_copy;            // Local copy of the original expression, used for parsing
  char *cmd_file;             // Permanent string which will be held by the command struct
  char *new_expr;             // Stripped version of orig_expression

  char *file_token;           // Pointer to the location of the redirect_token
  char *file_start;           // Pointer to the start of the redirect name
  char *file_end;             // Pointer to the end of the redirect name, delimited by space, newline, or end of string

  size_t len_expr;            // Length of the original expression
  size_t len_cmd_file;        // Length of the redirect name
  size_t len_before_token;    // Length of the substring up to the redirect token
  size_t len_after_redirect;  // Length of the substring after then end of the redirect name

  len_expr = (strlen (orig_expr)+1);
  expr_copy = checked_malloc (sizeof (char) * len_expr);
  strcpy (expr_copy, orig_expr);

  // If any parameters are invalid, return a copy of the un-edited expression
  if(cmd == NULL || (redirect_type != FILE_IN_CHAR && redirect_type != FILE_OUT_CHAR))
    return expr_copy;

  // Find the end of the string
  file_token = expr_copy;
  while (*file_token != '\0')
    file_token++;

  // Search for redirect token token in reverse. If we hit the start of the string
  // it does not exist. The expression would be invalid otherwise.
  while (*file_token != redirect_type && file_token > expr_copy)
    file_token--;

  // No redirect found
  if(file_token == expr_copy)
    return expr_copy;

  // A redirect was found
  // A redirect will start at least one character after the redirect token, possibly separated by whitespace
  file_start = file_token + 1;
  while (*file_start == ' ' && *file_start != '\0') // Sanity check for the end of string.
    file_start++;

  // Something wonky happened...
  if(*file_start == '\0')
    return expr_copy;

  // The redirect name will be delimited by whitespace, a newline, or the end of a string (NULL)
  file_end = file_start;
  while (*file_end != ' ' && *file_end != NEWLINE_CHAR && *file_end != '\0')
    file_end++;

  // Copy the redirect name for the command struct
  len_cmd_file = (file_end - file_start);
  cmd_file = checked_malloc (sizeof (char) * (len_cmd_file+1)); // Need to leave a space for the NULL byte as well
  memcpy (cmd_file, file_start, len_cmd_file);
  cmd_file[len_cmd_file] = '\0';

  if(redirect_type == FILE_IN_CHAR)
    cmd->input = cmd_file;
  else // FILE_OUT_CHAR
    cmd->output = cmd_file;

  // Now we strip the redirect token and name from the strings

  // file_token should come after expr_copy, they won't be the same char
  len_before_token = (file_token - expr_copy);
  len_after_redirect = strlen (file_end+1) + 1; // Don't forget the NULL byte

  new_expr = checked_malloc (sizeof (char) * (len_before_token + len_after_redirect));
  memcpy (new_expr, expr_copy, len_before_token);
  strcpy (new_expr + len_before_token, file_end);

  free (expr_copy);
  return new_expr;
}

char*
handle_and_strip_file_redirects (const char const *expr, command_t cmd)
{
  char *out_stripped;
  char *in_stripped;

  out_stripped = handle_and_strip_single_file_redirect (expr, cmd, FILE_OUT_CHAR);

  in_stripped = handle_and_strip_single_file_redirect (out_stripped, cmd, FILE_IN_CHAR);
  free (out_stripped);

  return in_stripped;
}

enum command_type
convert_token_to_command_type (char const *token)
{
  if(token == NULL)
    return SIMPLE_COMMAND;

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
make_command_from_expression (const char * const expr)
{
  command_t cmd = checked_malloc (sizeof (struct command));

  char const *token = get_pivot_token (expr);
  char const *left_string_start;  // Pointers to the beginning of the two possible strings
  char const *right_string_start;

  char *left_command;       // Formalized string copies which will be sub-parsed
  char *right_command;

  size_t left_size;
  size_t right_size;

  enum command_type token_type = convert_token_to_command_type (token);
  bool large_token = false; // Used to determine character offsets for tokens like '&&' and '||'

  if(token_type == AND_COMMAND || token_type == OR_COMMAND)
    large_token = true;

  if(token == NULL) // No tokens found, SIMPLE_COMMAND
    {
      left_string_start = expr;
      left_size = strlen(left_string_start) + 1;

      right_string_start = NULL;
      right_size = 0;
    }
  else
    {
      left_string_start = expr;
      left_size = (token - left_string_start) + 1; // Leave space for the null byte

      right_string_start = token + large_token + 1;
      right_size = strlen(right_string_start) + 1;
    }

    cmd->input = NULL;
    cmd->output = NULL;
    cmd->type = token_type;

    switch (cmd->type)
      {
        case SEQUENCE_COMMAND:
        case OR_COMMAND:
        case AND_COMMAND:
        case PIPE_COMMAND:
          {
            left_command = checked_malloc (sizeof (char) * left_size);
            memcpy (left_command, left_string_start, left_size);
            left_command[left_size-1] = '\0';

            right_command = checked_malloc (sizeof (char) * right_size);
            strcpy (right_command, right_string_start);

            cmd->u.command[0] = make_command_from_expression (left_command);
            cmd->u.command[1] = make_command_from_expression (right_command);

            free (left_command);
            free (right_command);
            break;
          }
        case SUBSHELL_COMMAND:
          {
            // @todo handle file redirects
            // @todo handle subparsing properly

            command_t sub_cmd = checked_malloc (sizeof (struct command));
            sub_cmd->input = NULL;
            sub_cmd->output = NULL;
            sub_cmd->type = SIMPLE_COMMAND;

            left_string_start = token + 1;
            char const *subshell_close_token = left_string_start;

            // We assume that the expressions are valid this far into the process (ie all subshells
            // have matching tokens), however, for sanity checks we capture the rest of the string
            // if the closing token is missing.
            while (*subshell_close_token != SUBSHELL_COMMAND_CHAR_CLOSE && subshell_close_token != NULL)
              subshell_close_token++;

            left_size = (subshell_close_token - left_string_start) + 1;

            left_command = checked_malloc (sizeof (char) * left_size);
            memcpy (left_command, left_string_start, left_size);
            left_command[left_size-1] = '\0';

            sub_cmd->u.word = split_expression_by_token (left_command, ' ');

            cmd->u.subshell_command = sub_cmd;
            break;
          }
        case SIMPLE_COMMAND:
          {
            // @todo handle file redirects
            char *stripped_expr = handle_and_strip_file_redirects (expr, cmd);
            cmd->u.word = split_expression_by_token (stripped_expr, ' ');
            free (stripped_expr);
            break;
          }
        default: break;
      }
  return cmd;
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
   command_stream_t expression_stream = checked_malloc (sizeof (struct command_stream));
   expression_stream->iterator = 0;
   expression_stream->stream_size = 0;
   expression_stream->alloc_size = 16;
   expression_stream->commands = checked_malloc (expression_stream->alloc_size * sizeof (command_t));

   char current_char;
   int total_lines_read = 0, current_expr_line_number = 1;

    // Anything between a COMMENT_CHAR and a newline is a comment. We don't write them to our buffer.
   bool in_comment = false, last_char_was_token = true;

   while ((current_char = get_next_byte (get_next_byte_argument)) != EOF)
   {
    // If this is a newline and we're not currently adding to a buffer, move on...
    if (current_char == NEWLINE_CHAR && current_expression_size == 0 && !in_comment)
      current_expr_line_number++;

    // If this is not a newline or a comment, just add it to the buffer...
    else if (current_char != NEWLINE_CHAR && current_char != COMMENT_CHAR && !in_comment)
    {
      last_char_was_token = false;
      expression_buffer = add_char_to_expression (current_char, expression_buffer, &current_expression_size, &expression_buffer_size);
    }

    // Comments
    else if (current_char == COMMENT_CHAR)
    {
      // If we're immediately preceded by a token or a normal word, this is not a comment.
      if (token_ends_at_point (expression_buffer, current_expression_size) || is_valid_word_char ((char) *(expression_buffer + current_expression_size - 1)))
        expression_buffer = add_char_to_expression (current_char, expression_buffer, &current_expression_size, &expression_buffer_size);

      else
        in_comment = true;
    }

    // Deal with newlines
    else if (current_char == NEWLINE_CHAR)
    {
      current_expr_line_number++;

      // Firstly, comments: newlines end comments.
      if (in_comment)
      {
        expression_buffer = add_char_to_expression (current_char, expression_buffer, &current_expression_size, &expression_buffer_size);
        in_comment = false;
      }

      // Next, handle cases where lines end with tokens. The expression continues.
      else if (token_ends_at_point (expression_buffer, current_expression_size))
      {
        last_char_was_token = true;
        expression_buffer = add_char_to_expression (current_char, expression_buffer, &current_expression_size, &expression_buffer_size);
      }

      // We've found the end of an expression!
      else if (!last_char_was_token)
      {
        // Add the terminator...
        expression_buffer = add_char_to_expression ('\0', expression_buffer, &current_expression_size, &expression_buffer_size);

        if (is_valid_expression (expression_buffer, &current_expr_line_number))
          add_expression_to_stream (expression_buffer, expression_stream);

        // Display an error message to stderr and exit if there's an error.
        else
        {
          show_error(total_lines_read + current_expr_line_number, expression_buffer);
        }

        // And reset everything to start again...
        free (expression_buffer);
        expression_buffer_size = 1024;
        current_expression_size = 0;

        expression_buffer = checked_malloc (expression_buffer_size * sizeof (char));
        expression_buffer[0] = '\0'; // Make things like strlen safe on the untouched buffer

        total_lines_read += current_expr_line_number;
        current_expr_line_number = 1;
      }
    }
   }

   // If there was anything extra in our buffer, we need to make sure we add it...
   if (strlen (expression_buffer) > 0)
   {
    if (is_valid_expression (expression_buffer, &current_expr_line_number))
    add_expression_to_stream (expression_buffer, expression_stream);

    // Display an error message to stderr and exit if there's an error.
    else
    {
      show_error(total_lines_read + current_expr_line_number, expression_buffer);
    }
   }

   // Clean up the buffer
   free (expression_buffer);

   return expression_stream;
}

void
show_error(int line_number, char *desc)
{
  fprintf (stderr, "%d: Incorrect syntax: %s", line_number, desc);
  exit (EXIT_FAILURE);
}

char*
add_char_to_expression (char c, char *expr, size_t *expr_utilized, size_t *expr_size)
{
  // Simply, add the next character
  expr[(*expr_utilized)++] = c;

  // If we've hit the size limit, time to add another 1024 bytes.
  if (*expr_utilized == *expr_size)
  {
    *expr_size += 1024;
    expr = checked_realloc (expr, *expr_size);
  }

  return expr;
}

bool
token_ends_at_point (const char *expr, size_t point)
{
  if (point == 0) // Can't start with a token.
    return false;

  const char *single_token_location = expr + (point - 1);
  if (is_valid_token (single_token_location))
  {
    return true;
  }

  const char *double_token_location = expr + (point - 2);
  if (is_valid_token (double_token_location))
  {
    // Figure out if this is a two-character token
    enum command_type type = convert_token_to_command_type (double_token_location);

    switch (type)
    {
      case AND_COMMAND:
      case OR_COMMAND:
        return true;
      default:
        return false;
    }
  }

  // If we got this far, we're not valid.
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
is_valid_expression (const char *expr, int *expr_line_number)
{
  /**
   * To validate everything, we'll need to use multiple passes.
   *
   * Pass 1:
   * No Illegal Characters
   * Alphanumeric and ! % + , - . / : @ ^ _ are allowed
   * We remove all tokens from the expression, and check for any remaining
   * illegal characters.
   *
   * All tokens must be followed by a word. The exception is
   * ( ) which may be preceded by or followed by any token.
   * Two consecutive tokens (even with whitespace) are not allowed.
   *
   * Pass 2:
   * Parenthesis must be matched.
   *
   * @todo we must check that redirects are not in an illegal order, i.e. > <
   */

  char current_char;
  int i;
  bool previous_was_token = false, found_word = false, last_paren_was_close = false, last_paren_was_open = true;

  enum command_type previous_token_type;

  *expr_line_number = 1;

  for (i = 0; (current_char = expr[i]) != '\0' ; ++i)
  {
    // A note: tokens and words do not share any characters.
    if (is_valid_word_char (current_char))
    {
      previous_was_token = false;
      found_word = true;
      continue;
    }

    else if (current_char == NEWLINE_CHAR)
    {
      (*expr_line_number)++;
      continue; // White space does not indicate a change of state for validation.
    }

    else if (current_char == ' ')
    {
      continue;
    }

    else if (!previous_was_token)
    {
      if (is_valid_token (expr + i))
      {
        previous_was_token = true;
        previous_token_type = convert_token_to_command_type (expr + i);

        // Remember that the first real character can't be any symbol but a parenthesis.
        // Note that not having found a word here indicates that we got here via whitespace,
        // which is still invalid.
        if ((i == 0 || !found_word) && current_char != SUBSHELL_COMMAND_CHAR_OPEN)
          return false;

        found_word = true;
        last_paren_was_close = (current_char == SUBSHELL_COMMAND_CHAR_CLOSE);
        last_paren_was_open = (current_char == SUBSHELL_COMMAND_CHAR_OPEN);

        switch (previous_token_type)
        {
          // For two-character commands, we need to skip the next character
          case AND_COMMAND:
          case OR_COMMAND:
            i+= 1;
            continue;
          default:
            continue; // Move along
        }
      }

      else // If there isn't a valid token here, it's not a legal character.
        return false;
    }

    else if (previous_was_token)
    {
      /**
       * If the previous character was a token, and this one is not a valid word
       * character or whitespace (in which case, we wouldn't be here...), it had
       * better be:
       *
       * Any token but an open parenthesis, if the previous token was a close parenthesis.
       * An open parenthesis, if the previous token was also an open parenthesis.
       *
       * Otherwise, this is invalid.
       */

      // If this isn't a valid token, something is wrong
      if (!is_valid_token (expr + i))
        return false;

      enum command_type current_token_type = convert_token_to_command_type (expr + i);

      switch (current_token_type)
        {
          // For two-character commands, we need to skip the next character
          case AND_COMMAND:
          case OR_COMMAND:
            i+= 1;
          default:
            break; // Move along
        }

      // If the previous character was a close parenthesis, ensure this is not an open parenthesis
      if (last_paren_was_close && current_char != SUBSHELL_COMMAND_CHAR_OPEN)
        continue;

      // If the previous token was an open parenthesis, allow only open parenthesis
      else if (last_paren_was_open && current_char == SUBSHELL_COMMAND_CHAR_OPEN)
        continue;

      // Otherwise, this is invalid so generate an error.
      return false;
    }

    // If none of these matched, it's definitely invalid.
    return false;
  }

  // Ending in a token isn't cool, unless it's a pipeline or a close paren
  if (previous_was_token && previous_token_type != PIPE_COMMAND && previous_token_type != SUBSHELL_COMMAND)
    return false;

  /**
   * Onto step 2!
   * Here, we check for parenthesis mismatching.
   * If we make it through here, we should be okay.
   * This could be rolled into the previous loop, but I am keeping it
   * here for clarity.
   */

  size_t num_open_paren = 0, num_close_paren = 0;

  *expr_line_number = 1; // Reset the counter since we're iterating again
  for (i = 0; (current_char = expr[i]) != '\0'; ++i)
  {
    if (current_char == SUBSHELL_COMMAND_CHAR_OPEN)
      num_open_paren++;

    else if (current_char == SUBSHELL_COMMAND_CHAR_CLOSE)
    {
      num_close_paren++;

      // The number of closing parenthesis should never exceed the number of open ones
      if (num_close_paren > num_open_paren)
        return false;
    }

    else if (current_char == NEWLINE_CHAR)
      (*expr_line_number)++;
  }

  // Check for an overall mismatch
  if (num_open_paren != num_close_paren)
    return false;

  return true; // If we reached the end, everything is cool.
}

bool
is_valid_word_char (char c)
{
  if isalnum(c)
    return true;

  // While ugly, this is a little more readable than a regex.
  switch (c)
  {
    case '!':
    case '%':
    case '+':
    case ',':
    case '-':
    case '.':
    case '/':
    case ':':
    case '@':
    case '^':
    case '_':
      return true;
    default:
      return false;
  }
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
