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

void
free_command_stream (command_stream_t cmd_stream)
{
  int i;
  for(i = 0; i < cmd_stream->stream_size; i++)
    free_command (cmd_stream->commands[i]);

  free (cmd_stream->commands);
  free (cmd_stream);
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
  int subshell_count = 0;

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

      // Unless we are looking for subshells, ignore anything inside of them
      if(type != SUBSHELL_COMMAND && *p == SUBSHELL_COMMAND_CHAR_CLOSE)
        {
          subshell_count++;

          while (subshell_count > 0 && p > expr)
            {
              p--;

              if(*p == SUBSHELL_COMMAND_CHAR_CLOSE)
                subshell_count++;
              else if(*p == SUBSHELL_COMMAND_CHAR_OPEN)
                subshell_count--;
            }

            // Move past the outermost '(' token
            p--;
        }

      cur_type = convert_token_to_command_type (p);

      // Return whenever the current token type matches the desired type
      // Since both '(' and ')' are marked as SUBSHELL_COMMAND we explicitly
      // check for ONLY '(' tokens
      if(cur_type == type)
      {
        if(type == SUBSHELL_COMMAND)
          {
            // We want to make sure the right level of subshell nesting is found
            if(*p == SUBSHELL_COMMAND_CHAR_CLOSE)
              subshell_count++;
            else if(*p == SUBSHELL_COMMAND_CHAR_OPEN)
              subshell_count--;

            if(subshell_count == 0)
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
handle_and_strip_single_file_redirect (char const * const orig_expr, command_t cmd, char redirect_type, bool is_subshell_command)
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
  // In the case of subshells, we only care about redirects outside of the shell.
  // Internal redirects will be handled when that subshell is being parsed
  if(is_subshell_command)
    {
      while (*file_token != redirect_type && *file_token != SUBSHELL_COMMAND_CHAR_CLOSE && file_token > expr_copy)
        file_token--;
    }
  else
    {
      while (*file_token != redirect_type && file_token > expr_copy)
        file_token--;
    }

  // No redirect found
  if(file_token == expr_copy || (is_subshell_command && *file_token == SUBSHELL_COMMAND_CHAR_CLOSE) )
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
handle_and_strip_file_redirects (const char const *expr, command_t cmd, bool is_subshell_command)
{
  char *out_stripped;
  char *in_stripped;

  out_stripped = handle_and_strip_single_file_redirect (expr, cmd, FILE_OUT_CHAR, is_subshell_command);

  in_stripped = handle_and_strip_single_file_redirect (out_stripped, cmd, FILE_IN_CHAR, is_subshell_command);
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
            char const *subshell_close_token;           // Pointer to the outermost ')' token
            char const *token_after_subshell_redirects; // Pointer to the next delimiting token after shell

            char *redirect_stripped_subshell;           // String returned from function handling redirects
            char *subshell_with_redirects;              // Copied string of subshell and characters up to the next token
                                                        // ie subshell along with any potential redirects applied to it
            size_t len_subshell_with_redirects;         // The size of subshell_with_redirects

            left_string_start = token + 1;

            subshell_close_token = (char const *)left_string_start;
            int subshell_count = 1;

            while (subshell_count > 0 && *subshell_close_token != '\0')
              {
                subshell_close_token++;

                if(*subshell_close_token == SUBSHELL_COMMAND_CHAR_OPEN)
                  subshell_count++;
                else if(*subshell_close_token == SUBSHELL_COMMAND_CHAR_CLOSE)
                  subshell_count--;
              }

            // Find the next token that is NOT a file redirect
            token_after_subshell_redirects = subshell_close_token;

            do
              {
                token_after_subshell_redirects = get_next_valid_token (token_after_subshell_redirects);
              }
            while (token_after_subshell_redirects != NULL &&
                  *token_after_subshell_redirects != '\0' &&
                    ( *token_after_subshell_redirects == FILE_IN_CHAR ||
                      *token_after_subshell_redirects == FILE_OUT_CHAR   )
                  );

            // Copy the subshell and any potential redirects into a new string
            len_subshell_with_redirects = token_after_subshell_redirects - left_string_start + 1;       // Pick up the outermost '(' as well
            subshell_with_redirects = checked_malloc (sizeof (char) * (len_subshell_with_redirects+1)); // Leave a space for NULL
            memcpy (subshell_with_redirects, left_string_start-1, len_subshell_with_redirects);         // One char left of left_string_start gets the outermost '(' char
            subshell_with_redirects[len_subshell_with_redirects] = '\0';

            // Handle redirects
            redirect_stripped_subshell = handle_and_strip_file_redirects (subshell_with_redirects, cmd, true);

            // Free memory we don't need
            free (redirect_stripped_subshell);
            free (subshell_with_redirects);
            redirect_stripped_subshell = NULL;
            subshell_with_redirects = NULL;

            // Copy the string within the subshell and subparse it
            left_size = (subshell_close_token - left_string_start);
            left_command = checked_malloc (sizeof (char) * (left_size+1)); // Leave space for NULL byte
            memcpy (left_command, left_string_start, left_size);
            left_command[left_size] = '\0';

            cmd->u.subshell_command = make_command_from_expression (left_command);
            free (left_command);
            break;
          }
        case SIMPLE_COMMAND:
          {
            char *stripped_expr = handle_and_strip_file_redirects (expr, cmd, false);
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
   int total_lines_read = 0, current_expr_line_number = 1, open_paren_count = 0, close_paren_count = 0;

    // Anything between a COMMENT_CHAR and a newline is a comment. We don't write them to our buffer.
   bool in_comment = false, last_char_was_token = true, last_token_was_close_paren = false, last_was_word = false, seen_word = false;

   while ((current_char = get_next_byte (get_next_byte_argument)) != EOF)
   {
    // If this is a newline and we're not currently adding to a buffer, move on...
    if (current_char == NEWLINE_CHAR && current_expression_size == 0 && !in_comment)
      current_expr_line_number++;

    // If this is not a newline or a comment, just add it to the buffer...
    else if (current_char != NEWLINE_CHAR && current_char != COMMENT_CHAR && !in_comment && (current_char != SEQUENCE_COMMAND_CHAR || open_paren_count != close_paren_count ))
    {
      last_char_was_token = false;

      if (is_valid_word_char (current_char))
        seen_word = true;

      last_was_word = (current_char != ' ');

      expression_buffer = add_char_to_expression (current_char, expression_buffer, &current_expression_size, &expression_buffer_size);

      // Increment our parenthesis counters
      switch (current_char)
      {
        case SUBSHELL_COMMAND_CHAR_OPEN:
          open_paren_count++;
          last_char_was_token = false;
          last_was_word = false;
          continue;
        case SUBSHELL_COMMAND_CHAR_CLOSE:
          close_paren_count++;
          last_token_was_close_paren = true;
          last_char_was_token = false;
          last_was_word = false;
          continue;
        default:
          if (token_ends_at_point (expression_buffer, expression_buffer_size))
          {
            last_token_was_close_paren = false;
            last_char_was_token = true;
            last_was_word = false;
          }
          continue;
      }
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
    else if (current_char == NEWLINE_CHAR || (current_char == SEQUENCE_COMMAND_CHAR && open_paren_count == close_paren_count))
    {
      if (current_char == NEWLINE_CHAR)
        current_expr_line_number++;

      // Firstly, comments: newlines end comments.
      if (in_comment && current_char != SEQUENCE_COMMAND_CHAR)
      {
        expression_buffer = add_char_to_expression (current_char, expression_buffer, &current_expression_size, &expression_buffer_size);
        in_comment = false;
      }

      // If we're in a comment and it's a sequence, keep going
      else if (in_comment && current_char == SEQUENCE_COMMAND_CHAR)
        continue;

      // Next, handle cases where lines end with tokens. The expression continues.
      else if (token_ends_at_point (expression_buffer, current_expression_size) && !last_token_was_close_paren)
      {
        last_char_was_token = true;
        expression_buffer = add_char_to_expression (current_char, expression_buffer, &current_expression_size, &expression_buffer_size);
      }

      // We've found the end of an expression! Note we allow expressions to end in tokens
      // if it's a close paren and we've currently matched parens. We will also continue if we've
      // gotten this far and we've reached a semicolon.
      else if ((!last_char_was_token && seen_word) || (open_paren_count == close_paren_count && last_token_was_close_paren) || (current_char == SEQUENCE_COMMAND_CHAR && open_paren_count == close_paren_count && last_was_word))
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

        open_paren_count = 0;
        close_paren_count = 0;

        last_was_word = false;
        seen_word = false;
      }

      // If all else fails, keep going
      else
        expression_buffer = add_char_to_expression (current_char, expression_buffer, &current_expression_size, &expression_buffer_size);
    }
   }

   // If there was anything extra in our buffer, we need to make sure we add it...
   if (strlen (expression_buffer) > 0)
   {
    if (is_valid_expression (expression_buffer, &current_expr_line_number))
      add_expression_to_stream (expression_buffer, expression_stream);

    // Display an error message to stderr and exit if there's an error.
    else
      show_error(total_lines_read + current_expr_line_number, expression_buffer);
   }

   // Clean up the buffer
   free (expression_buffer);

   return expression_stream;
}

void
show_error (int line_number, char *desc)
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
          // We also reset our paren flags unless this is also a parenthesis.
          case AND_COMMAND:
          case OR_COMMAND:
            i+= 1;
            last_paren_was_open = false;
            last_paren_was_close = false;
            previous_token_type = current_token_type;
            continue;
          case SUBSHELL_COMMAND:
            break; // This is a paren, don't touch the flags
          case PIPE_COMMAND:
            previous_token_type = current_token_type;
            break;
          case SEQUENCE_COMMAND:
            // These are okay after a close paren
            if (last_paren_was_close)
            {
              last_paren_was_open = false;
              last_paren_was_close = false;
              previous_token_type = current_token_type;
              continue;
            }
          default:
            if (current_char != FILE_IN_CHAR && current_char != FILE_OUT_CHAR)
            {
              last_paren_was_open = false;
              last_paren_was_close = false;              
          }
        }

      // If the previous character was a close parenthesis, ensure this is not an open parenthesis
      if (last_paren_was_close && current_char != SUBSHELL_COMMAND_CHAR_OPEN)
      {
        previous_token_type = current_token_type;

        if (current_char == SUBSHELL_COMMAND_CHAR_OPEN)
        {
          last_paren_was_open = true;
          last_paren_was_close = false;
        }
        else if (current_char == SUBSHELL_COMMAND_CHAR_CLOSE)
        {
          last_paren_was_open = false;
          last_paren_was_close = true;        
        }

        continue;
      }

      // If the previous token was an open parenthesis, allow only open parenthesis
      else if (last_paren_was_open && current_char == SUBSHELL_COMMAND_CHAR_OPEN)
      {
        previous_token_type = current_token_type;

        if (current_char == SUBSHELL_COMMAND_CHAR_OPEN)
        {
          last_paren_was_open = true;
          last_paren_was_close = false;
        }
        else if (current_char == SUBSHELL_COMMAND_CHAR_CLOSE)
        {
          last_paren_was_open = false;
          last_paren_was_close = true;        
        }

        continue;
      }

      // Allow an open parenthesis after any token
      else if (current_char == SUBSHELL_COMMAND_CHAR_OPEN && previous_token_type != SUBSHELL_COMMAND)
      {
        previous_token_type = current_token_type;

        if (current_char == SUBSHELL_COMMAND_CHAR_OPEN)
        {
          last_paren_was_open = true;
          last_paren_was_close = false;
        }
        else if (current_char == SUBSHELL_COMMAND_CHAR_CLOSE)
        {
          last_paren_was_open = false;
          last_paren_was_close = true;        
        }

        continue;
      }

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

  // Third pass: expression order
  return expression_redirect_order_is_valid (expr, expr_line_number);
}

bool
expression_redirect_order_is_valid (const char *expr, int *line_number)
{
  /**
   * Iterate over our expression. We look for the following invalid conditions:
   * More than one > or <
   * > < appearing in this order.
   *
   * Note that these conditions are reset each time we hit a token. Further, as
   * soon as we find a subshell, we repeat this search recursively.
   *
   * Also:
   * A < can be preceded by ')' or whitespace or a word.
   * It MUST have a word after it.
   * A > can be preceded by ')' or whitespace or a word. 
   * It MUST have a word after it.
   */

  char current_char;
  int open_paren_count = 0, close_paren_count = 0, i = 0;

  size_t subshell_buffer_size = 0;
  size_t subshell_buffer_max = 1024;
  char* subshell_buffer = checked_malloc (sizeof (char) * subshell_buffer_max);

  bool last_was_token = false;
  char last_token_char;

  bool found_in_redirect = false, found_out_redirect = false;

  size_t pre_recursion_linecount = 0;

  for (i = 0; (current_char = expr[i]) != '\0'; i++)
  {
    // If it's a space, keep going
    if (current_char == ' ')
    {
      continue;
    }

    // If this is a word character, continue on.
    if (is_valid_word_char (current_char))
    {
      last_was_token = false;
      continue;
    }

    // If it's a newline, increment the line counter and continue
    else if (current_char == NEWLINE_CHAR)
    {
      (*line_number)++;
      last_was_token = false;
      continue;
    }

    // Handle redirect characters
    else if (current_char == FILE_IN_CHAR)
    {
      // No more than one is allowed
      if (found_in_redirect)
        return false;

      // This can only be preceded by a ) (within the token set)
      if (last_was_token && last_token_char != SUBSHELL_COMMAND_CHAR_CLOSE)
        return false;

      // This can't follow an out redirect
      if (found_out_redirect)
        return false;

      found_in_redirect = true;
      continue;
    }

    else if (current_char == FILE_OUT_CHAR)
    {
      // No more than one is allowed
      if (found_out_redirect)
        return false;

       // This can only be preceded by a ) (within the token set)
      if (last_was_token && last_token_char != SUBSHELL_COMMAND_CHAR_CLOSE)
        return false;

      found_out_redirect = true;
      continue;
    }

    // Handle other tokens
    else if (token_ends_at_point (expr, (size_t) (i - 1)))
    {
      // If we have a subshell open, we need to look for the end
      if (current_char == SUBSHELL_COMMAND_CHAR_OPEN)
      {
        for (subshell_buffer_size = 0; (current_char = expr[i + subshell_buffer_size + 1]) != '\0'; ++subshell_buffer_size)
        {
          // If this is our first iteration, we need to count the first paren
          if (subshell_buffer_size == 0)
            open_paren_count = 1;

          // Keep looking until we find the close parenthesis
          if (current_char == SUBSHELL_COMMAND_CHAR_OPEN)
            open_paren_count++;

          else if (current_char == SUBSHELL_COMMAND_CHAR_CLOSE)
          {
            close_paren_count++;

            // If the count matches, we've found the end!
            if (open_paren_count == close_paren_count)
            {
              pre_recursion_linecount = *line_number;

              subshell_buffer = add_char_to_expression ('\0', subshell_buffer, &subshell_buffer_size, &subshell_buffer_max);
              // If it's valid, keep moving
              if (expression_redirect_order_is_valid (subshell_buffer, line_number))
              {
                i += subshell_buffer_size + 1; // To include the close paren
                last_was_token = true;
                last_token_char = SUBSHELL_COMMAND_CHAR_CLOSE;

                // Reset the buffer
                free (subshell_buffer);
                subshell_buffer = malloc (sizeof (char) * 1024);

                *line_number = pre_recursion_linecount;

                break;
              }

              // There's an error, so react appropriately
              else
              {
                free (subshell_buffer);
                return false;
              }
            }
          }

          // If we got this far, nothing special is going to happen, so add to the buffer and move on.
          subshell_buffer = add_char_to_expression (current_char, subshell_buffer, &subshell_buffer_size, &subshell_buffer_max);
        }
      }

      // Otherwise, we're just going to update our token-related stuff and move along
      else
      {
        last_was_token = true;
        last_token_char = current_char;

        // Reset our pipe information
        found_in_redirect = false;
        found_out_redirect = false;

        continue;
      }
    }
  }

  // If we have reached this point, there are no errors.
  return true;
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
