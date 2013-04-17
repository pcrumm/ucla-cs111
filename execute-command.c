// UCLA CS 111 Lab 1 command execution

#include "alloc.h"
#include "command.h"
#include "command-internals.h"

#include <error.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#define PIPE_READ 0
#define PIPE_WRITE 1

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
  printf("%s", c->stdout);
}

void
execute_simple_command (command_t c)
{
  // First, find the proper binary.
  char *exec_bin = get_executable_path (c->u.word[0]);
  if (exec_bin == NULL)
    show_error (c->line_number, "Could not find binary to execute");

  free (c->u.word[0]);
  c->u.word[0] = exec_bin;

  // If we found it, let's execute it!
  // First, create a pipe to handle input/output to the command
  int outfd[2], infd[2], pid, exit_status;
  pipe (outfd);
  pipe (infd);

  if ((pid = fork ()) < 0)
    show_error (c->line_number, "Could not fork.");

  if (pid == 0) // Child process
  {
    close (STDOUT_FILENO);
    close (STDIN_FILENO);

    dup2 (outfd[PIPE_READ], STDIN_FILENO);
    dup2 (infd[PIPE_WRITE], STDOUT_FILENO);

    // We are not using the read end here, so close it
    close (infd[PIPE_READ]);
    close (infd[PIPE_WRITE]);

    close (outfd[PIPE_READ]);
    close (outfd[PIPE_WRITE]);

    // Execute!
    execvp (c->u.word[0], c->u.word);

    // If we got here, there's a problem
    show_error (c->line_number, "Execution error");
  }
  else // Parent process
  {
    close (outfd[PIPE_READ]); // used by the child
    close (infd[PIPE_WRITE]);

    // write to child's stdin (outfd[1] - outfd[PIPE_WRITE])
    // If we have any stdin, write it to the pipe
    if (c->stdin != NULL)
    {
      if (write (outfd[PIPE_WRITE], c->stdin, strlen(c->stdin)) == -1)
        show_error (c->line_number, "Could not write to input.");
    }

    // If we have an input redirection, read that. Note that we will ignore this if
    // there is already data to write to stdin.
    if (c->input != NULL && c->stdin == NULL)
    {
      if ((c->input = get_redirect_file_path (c->input)) != NULL)
      {
        char *input = get_file_contents (c->input);

        // now write it into our standard input for the child
        write (outfd[PIPE_WRITE], input, strlen(input));
        free (input);
      }
      else
        show_error (c->line_number, "Could not read.");
    }

    // Read from the standard output and write it to a file, as necessary
    int stdout_buffer_size = 1024;
    char *stdout_input = checked_malloc (sizeof (char) * stdout_buffer_size);
    c->stdout = checked_malloc (sizeof (char) * stdout_buffer_size);

    ssize_t read_bytes = 0, total_read_bytes = 0;
    while ((read_bytes = read (infd[PIPE_READ], stdout_input, stdout_buffer_size)) > 0)
    {
      c->stdout = checked_realloc(c->stdout, sizeof (char) * total_read_bytes + read_bytes);
      strcat (c->stdout + total_read_bytes, stdout_input);

      total_read_bytes += read_bytes;

      // Reset the old memory
      memset (stdout_input, '\0', stdout_buffer_size);
    }

    free (stdout_input);

    close (outfd[PIPE_WRITE]);
    close (infd[PIPE_READ]);

    if (c->output != NULL)
    {
      // We need to write c->stdout to a file!
      c->output = get_redirect_file_path (c->output);
      if (c->output == NULL)
        show_error (c->line_number, "Could not find output file");

      FILE *fp;
      fp = fopen (c->output, "w");
      if (fp == NULL)
        show_error (c->line_number, "Could not open output file");

      fprintf(fp, "%s", c->stdout);
      fclose (fp);

      free (c->stdout);
      c->stdout = NULL;
    }

    // Set our exit code for the command
    waitpid (pid, &exit_status, 0);
    c->status = exit_status;
  }
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

  if (file_exists (path))
    return path;

  // If not, let's try the system PATH variable and see if we have any matches there
  char *syspath = getenv ("PATH");

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

char*
get_redirect_file_path (char *redirect_file)
{
  // Check if this is an absolute path (started by /)
  if (redirect_file [0] == '/')
    return redirect_file;

  // First, find the current working directory
  char *cwd = getcwd (NULL, 0);

  // Otherwise, return the relative path
  char *path = checked_malloc (sizeof(char) * (strlen(cwd) + strlen(redirect_file) + 1));
  strcpy (path, cwd);
  strcat (path, "/");
  strcat (path, redirect_file);

  return path;
}

char* get_file_contents (char *file_path)
{
  FILE *fp;
  struct stat st;
  char *buffer;

  fp = fopen (file_path, "r");
  if (fp == NULL)
  {
    return NULL;
  }

  fstat (fileno (fp), &st);
  buffer = checked_malloc (sizeof (char) * (st.st_size + 1));
  fread (buffer, sizeof (char), st.st_size, fp);
  fclose (fp);

  // Set the last byte as the null byte
  buffer[st.st_size] = '\0';

  return buffer;
}
