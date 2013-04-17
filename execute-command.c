// UCLA CS 111 Lab 1 command execution

#include "alloc.h"
#include "command.h"
#include "command-internals.h"

#include <error.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define FREE_EXECUTED_COMMANDS

int
command_status (command_t c)
{
  return c->status;
}

char *
copy_buffer (const char * const buffer)
{
  if(buffer == NULL)
    return NULL;

  size_t len_copy = strlen (buffer) + 1;
  char *copy = checked_malloc (sizeof (char) * len_copy);
  strcpy (copy, buffer);

  return copy;
}

char*
copy_and_concat_buffers (const char * const buff_one, const char * const buff_two)
{
  if(buff_one == NULL && buff_two == NULL)
    return NULL;
  else if(buff_one == NULL)
    return copy_buffer (buff_two);
  else if(buff_two == NULL)
    return copy_buffer (buff_one);

  size_t len_buff_one = strlen (buff_one);
  size_t len_buff_two = strlen (buff_two);

  size_t len_concat = len_buff_one + len_buff_two + 1;

  char *concat = checked_malloc (sizeof (char) * len_concat);

  memcpy (concat, buff_one, len_buff_one);

  // Overwrite the EOF from the first buffer, and don't forget to copy the NULL byte from buff_two
  memcpy (concat + len_buff_one - 1, buff_two, len_buff_two+1);

  return concat;
}

void
recursive_execute_command (command_t c, bool time_travel, bool is_subshell)
{
  // Ignore time travel for subshells
  time_travel = (is_subshell ? false : time_travel);

  // Copy any stdin down to children
  switch (c->type)
    {
      case SEQUENCE_COMMAND:
      case AND_COMMAND:
      case OR_COMMAND:
      case PIPE_COMMAND:
        c->u.command[0]->stdin = copy_buffer (c->stdin);
        break;
      case SUBSHELL_COMMAND:
        c->u.subshell_command->stdin = copy_buffer (c->stdin);
        break;
      case SIMPLE_COMMAND: break;
      default: break;
    }

  // Execute commands based on tokens and properly set the status up the tree
  switch (c->type)
    {
      case SEQUENCE_COMMAND:
        // Status of SEQUENCE_COMMAND is based on last executed command
        recursive_execute_command (c->u.command[0], time_travel, is_subshell);
        recursive_execute_command (c->u.command[1], time_travel, is_subshell);
        c->status = c->u.command[1]->status;
        break;

      case SUBSHELL_COMMAND:
        // Status of SUBSHELL_COMMAND is the same as the command inside it
        recursive_execute_command (c->u.subshell_command, time_travel, true);
        c->status = c->u.subshell_command->status;
        break;

      case AND_COMMAND:
        // Status of AND_COMMAND is either the first failed command, or the last executed
        recursive_execute_command (c->u.command[0], time_travel, is_subshell);
        if(c->u.command[0]->status == 0)
          {
            recursive_execute_command (c->u.command[1], time_travel, is_subshell);
            c->status = c->u.command[1]->status;
          }
        else // first command has NOT exited successfully
          {
           c->status = c->u.command[0]->status;
          }
        break;

      case OR_COMMAND:
        // Status of OR_COMMAND is either the first successful command, or the last executed
        recursive_execute_command (c->u.command[0], time_travel, is_subshell);
        if(c->u.command[0]->status == 0)
          {
            c->status = c->u.command[0]->status;
          }
        else
          {
            recursive_execute_command (c->u.command[1], time_travel, is_subshell);
            c->status = c->u.command[1]->status;
          }
          break;

        case PIPE_COMMAND:
          recursive_execute_command (c->u.command[0], time_travel, is_subshell);
          c->u.command[1]->stdin = c->u.command[0]->stdout; // "Pipe" the output of the first command

          recursive_execute_command (c->u.command[1], time_travel, is_subshell);
          c->status = c->u.command[1]->status;
          break;

        case SIMPLE_COMMAND:
          execute_simple_command (c);
          break;

        default: break;
    }

  // Set the stdout from the children
  switch (c->type)
    {
      case SEQUENCE_COMMAND:
      case AND_COMMAND:
      case OR_COMMAND:
        // A single command has been run
        if( (c->u.command[0]->status != 0 && c->type == AND_COMMAND) ||
            (c->u.command[0]->status == 0 && c->type == OR_COMMAND)  )
          c->stdout = copy_buffer (c->u.command[0]->stdout);

        // Both commands have run and within subshell
        else if(is_subshell)
          c->stdout = copy_and_concat_buffers (c->u.command[0]->stdout,c->u.command[1]->stdout);

        // Both commands have run but NOT within subshell
        else
          c->stdout = copy_buffer (c->u.command[1]->stdout);
        break;

      case PIPE_COMMAND:
        c->stdout = copy_buffer (c->u.command[1]->stdout);
        break;

      case SUBSHELL_COMMAND:
        c->stdout = copy_buffer (c->u.subshell_command->stdout);
        break;

      case SIMPLE_COMMAND: break;
      default: break;
    }

#ifdef FREE_EXECUTED_COMMANDS
    // Free up memory from already executed commands to avoid
    // memory bloats from buffer copying and concatenations
    switch (c->type)
      {
        case SEQUENCE_COMMAND:
        case AND_COMMAND:
        case OR_COMMAND:
        case PIPE_COMMAND:
          free_command (c->u.command[0]);
          free_command (c->u.command[1]);
          c->u.command[0] = NULL;
          c->u.command[1] = NULL;
          break;
        case SUBSHELL_COMMAND:
          free_command (c->u.subshell_command);
          c->u.subshell_command = NULL;
          break;
        // No need to free the words in SIMPLE_COMMANDs, they'll get
        // cleaned up at at the end of the previous recursive call
        case SIMPLE_COMMAND: break;
        default: break;
      }
#endif // FREE_EXECUTED_COMMANDS
}

void
execute_command (command_t c, bool time_travel)
{
  recursive_execute_command (c, time_travel, false);

  // Output the final result. Note the lack of trailing newline
  // that's up to the actual command to output
  printf("%s", c->output);
}

void
execute_simple_command (command_t c)
{
  return;
}

char*
get_executable_path (char* bin_name)
{
  // Check if this is a valid absolute price
  if (file_exists (bin_name))
    return bin_name;

  // First, find the current working directory
  char *cwd = getcwd (NULL, 0);

  // Does it exist relative to the current directory?
  char *path = checked_malloc (sizeof(char) * (strlen(cwd) + strlen(bin_name) + 1));
  strcpy (path, cwd);
  strcat (path, "/");
  strcat (path, bin_name);

  printf("testing path %s\n\n", path);

  if (file_exists (path))
    return path;

  // If not, let's try the system PATH variable and see if we have any matches there
  char *syspath = getenv ("PATH");
  printf("path is %s\n\n", syspath);

  // Split the path
  char **path_elements = NULL;
  char *p = strtok (syspath, ":");
  int path_items = 0, i;

  while (p)
  {
    path_elements = checked_realloc (path_elements, sizeof (char*) * (path_items + 2));
    path_elements[path_items] = p;

    path_items++;

    p = strtok (NULL, ":");
  }

  path_elements[path_items] = NULL; // Make sure the last item is null for later looping
  free(p);

  // Now, let's iterate over each item in the path and see if it's a fit
  for (i = 0; path_elements[i] != NULL; i++)
  {
    path = checked_realloc (path, sizeof(char) * (strlen(bin_name) + strlen(path_elements[i] + 1)));
    strcpy (path, path_elements[i]);
    strcat (path, "/");
    strcat (path, bin_name);

    if (file_exists (path))
    {
      free (path_elements);
      return path;
    }
  }

  free (path);
  return NULL; // If we got this far, there's an error.
}

bool
file_exists (char *path)
{
  FILE *file;
  if ((file = fopen (path, "r")) != NULL)
  {
    fclose (file);
    return true;
  }

  return false;
}
