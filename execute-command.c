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
#include <fcntl.h>

#define PIPE_READ 0
#define PIPE_WRITE 1

int
command_status (command_t c)
{
  return c->status;
}

void
show_error (int line_number, char *desc, char *desc2)
{
  if(desc2 == NULL)
    fprintf (stderr, "%d: %s\n", line_number, desc);
  else
    fprintf (stderr, "%d: %s \"%s\"\n", line_number, desc, desc2);
  exit (EXIT_FAILURE);
}

void
close_command_exec_resources (command_t c)
{
  if(c == NULL)
    return;

  switch (c->type)
    {
      case SEQUENCE_COMMAND:
      case AND_COMMAND:
      case OR_COMMAND:
      case PIPE_COMMAND:
        close_command_exec_resources (c->u.command[1]);
        close_command_exec_resources (c->u.command[0]);
        break;
      case SUBSHELL_COMMAND:
        close_command_exec_resources (c->u.subshell_command);
        break;
      case SIMPLE_COMMAND:
        if(c->pid > 0)
          {
            int exit_status;
            waitpid (c->pid, &exit_status, 0);

            // Check whether the child exited successfully
            // If it exited due to a signal record the signal instead
            if(WIFEXITED (exit_status))
              c->status = WEXITSTATUS (exit_status);
          }

        if(c->fd_read_from > -1)
          close (c->fd_read_from);

        // fd_writing_to should be closed by its reader
        break;
      default: break;
    }

    c->fd_read_from = -1;
    c->fd_writing_to = -1;
    c->pid = -1;

    // Propagate the status to the parent
    switch (c->type)
      {
        case SEQUENCE_COMMAND:
        case PIPE_COMMAND:
          c->status = command_status (c->u.command[1]);
          break;

        case AND_COMMAND:
          if(command_status (c->u.command[0]) == 0)
            c->status = command_status (c->u.command[1]);
          else
            c->status = command_status (c->u.command[0]);
          break;

        case OR_COMMAND:
          if(command_status (c->u.command[0]) == 0)
            c->status = command_status (c->u.command[0]);
          else
            c->status = command_status (c->u.command[1]);
          break;

        case SUBSHELL_COMMAND:
          c->status = command_status (c->u.subshell_command);
          break;

        case SIMPLE_COMMAND: break;
        default: break;
      }
}

void
recursive_execute_command (command_t c, bool pipe_output)
{
  // Copy any specified file descriptors down to the children
  switch (c->type)
    {
      case SEQUENCE_COMMAND:
      case AND_COMMAND:
      case OR_COMMAND:
      case PIPE_COMMAND:
        c->u.command[0]->fd_read_from = c->fd_read_from;
        c->u.command[0]->fd_writing_to = c->fd_writing_to;

        c->u.command[1]->fd_read_from = c->fd_read_from;
        c->u.command[1]->fd_writing_to = c->fd_writing_to;
        break;
      case SUBSHELL_COMMAND:
        c->u.subshell_command->fd_read_from = c->fd_read_from;
        c->u.subshell_command->fd_writing_to = c->fd_writing_to;
        break;
      case SIMPLE_COMMAND: break;
      default: break;
    }

  // Execute commands based on tokens and properly set the status up the tree
  switch (c->type)
    {
      case SEQUENCE_COMMAND:
        // Status of SEQUENCE_COMMAND is based on last executed command
        recursive_execute_command (c->u.command[0], pipe_output);
        recursive_execute_command (c->u.command[1], pipe_output);
        c->status = command_status (c->u.command[1]);
        break;

      case SUBSHELL_COMMAND:
        {
          // Store a copy to stdin and stdout in case the shell wants to redirect input/output
          int fd_old_stdin  = dup (STDIN_FILENO);
          int fd_old_stdout = dup (STDOUT_FILENO);

          if(fd_old_stdin < 0 || fd_old_stdout < 0)
            show_error(c->line_number, "Too many files open!", NULL);

          if(c->input != NULL)
            {
              char *input_path = get_redirect_file_path (c->input);
              int fd_in = open (input_path, O_RDONLY);

              free (input_path);
              if(fd_in == -1) show_error (c->line_number, "Error opening input file", c->input);

              dup2 (fd_in, STDIN_FILENO);
              close (fd_in);
            }

          if(c->output != NULL)
            {
              char *output_path = get_redirect_file_path (c->output);
              int fd_out = open (c->output,
                  O_WRONLY | O_CREAT | O_TRUNC,           // Data from the file will be truncated
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); // By default bash in posix mode will create files as rw-r--r--

              free (output_path);
              if(fd_out == -1) show_error (c->line_number, "Error opening output file", c->output);

              dup2 (fd_out, STDOUT_FILENO);
              close (fd_out);
            }

          // Status of SUBSHELL_COMMAND is the same as the command inside it
          recursive_execute_command (c->u.subshell_command, pipe_output);
          c->status = c->u.subshell_command->status;
          c->fd_writing_to = c->u.subshell_command->fd_writing_to;

          // Reset stdin and stdout
          dup2 (fd_old_stdin, STDIN_FILENO);
          dup2 (fd_old_stdout, STDOUT_FILENO);
          close (fd_old_stdin);
          close (fd_old_stdout);
          break;
        }

      case AND_COMMAND:
        // Status of AND_COMMAND is either the first failed command, or the last executed
        recursive_execute_command (c->u.command[0], pipe_output);
        if(command_status (c->u.command[0]) == 0)
          {
            recursive_execute_command (c->u.command[1], pipe_output);
            c->status = command_status (c->u.command[1]);
          }
        else // first command has NOT exited successfully
          {
           c->status = command_status (c->u.command[0]);
          }
        break;

      case OR_COMMAND:
        // Status of OR_COMMAND is either the first successful command, or the last executed
        recursive_execute_command (c->u.command[0], pipe_output);
        if(command_status (c->u.command[0]) == 0)
          {
            c->status = command_status (c->u.command[0]);
          }
        else
          {
            recursive_execute_command (c->u.command[1], pipe_output);
            c->status = command_status (c->u.command[1]);
          }
        break;

        case PIPE_COMMAND:
          recursive_execute_command (c->u.command[0], true);

          // Set the next command to read from the opened pipe
          c->u.command[1]->fd_read_from = c->u.command[0]->fd_writing_to;

          recursive_execute_command (c->u.command[1], pipe_output);
          c->fd_writing_to = c->u.command[1]->fd_writing_to;

          // Close the resources if and only if we aren't in a nested pipe
          // otherwise it's possible to cause a deadlock! Everything will get recursively
          // freed in a higher frame anyways.
          if(!pipe_output)
            {
              close_command_exec_resources (c->u.command[1]);
              close_command_exec_resources (c->u.command[0]);
            }

          c->status = c->u.command[1]->status;
          break;

        case SIMPLE_COMMAND:
          execute_simple_command (c, pipe_output);

          // Since we aren't piping we can go ahead and free any CPU resources
          // Otherwise they will get freed when the pipe is completed
          if(!pipe_output)
              close_command_exec_resources (c);
          break;

        default: break;
    }
}

int
execute_command (command_t c)
{
  if(c == NULL)
    return -1;

  recursive_execute_command (c, false);
  return c->status;
}

void
execute_simple_command (command_t c, bool pipe_output)
{
  // This is a "blank command" which was probably all newlines as a result of comments being read
  // No need to do anything further
  if(c->u.word[0] == NULL)
    return;

  // If the command is the keyword `exec`, run the exec utility, which replaces the current process
  // Control is NOT returned
  if(strcmp (c->u.word[0], "exec") == 0)
    exec_utility(c);

  // First, find the proper binary.
  char *exec_bin = get_executable_path (c->u.word[0]);
  if (exec_bin == NULL)
    show_error (c->line_number, "Could not find binary to execute", c->u.word[0]);

  // If we found it, let's execute it!
  // First, create a pipe to handle input/output to the command
  int pipefd[2], pid;
  pipe (pipefd);

  if(c->output == NULL && pipe_output) // Signifies where to read the pipe from
    c->fd_writing_to = pipefd[PIPE_READ];
  else
    c->fd_writing_to = -1;

  if ((pid = fork ()) < 0)
    show_error (c->line_number, "Could not fork.", NULL);

  if (pid == 0) // Child process
  {
    close (pipefd[PIPE_READ]); // Child will never read from this newly opened pipe

    // File redirects trump pipes, thus we ignore specified pipes if redirects are present
    if(c->input != NULL)
      {
        char *input_path = get_redirect_file_path (c->input);
        int fd_in = open (input_path, O_RDONLY);
        free (input_path);

        if(fd_in == -1)
          show_error (c->line_number, "Error opening input file", c->input);

        // dup2 will close STDIN_FILENO for us
        dup2 (fd_in, STDIN_FILENO);
        close (fd_in);
      }
    else if (c->fd_read_from > -1)
      {
        dup2 (c->fd_read_from, STDIN_FILENO);
        close (c->fd_read_from);
      }
    else
      {
        // Leave STDIN_FILENO open!
      }

    // File redirects trump pipes, thus we ignore specified pipes if redirects are present
    if(c->output != NULL)
      {
        char *output_path = get_redirect_file_path (c->output);
        int fd_out = open (output_path,
            O_WRONLY | O_CREAT | O_TRUNC,           // Data from the file will be truncated
            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); // By default bash in posix mode will create files as rw-r--r--

        free (output_path);
        if(fd_out == -1)
          show_error (c->line_number, "Error opening output file", c->output);

        dup2 (fd_out, STDOUT_FILENO);
        close (fd_out);
        close (pipefd[PIPE_WRITE]); // We already have an output location, we don't need this pipe
      }
    else if(pipe_output)
      {
        dup2 (pipefd[PIPE_WRITE], STDOUT_FILENO);
        close (pipefd[PIPE_WRITE]);
      }
    else
      {
        // Leave STDOUT_FILENO open!
        close (pipefd[PIPE_WRITE]);
      }

    // Execute!
    execvp (exec_bin, c->u.word);

    // If we got here, there's a problem
    show_error (c->line_number, "Execution error", NULL);
  }
  else // Parent process
  {
    c->pid = pid; // Set the child's PID

    close (pipefd[PIPE_WRITE]);

    if(!pipe_output)
      close (pipefd[PIPE_READ]);
  }
}

void
exec_utility (command_t c)
{
  // First, find the proper binary.
  char *exec_bin = get_executable_path (c->u.word[1]);
  if (exec_bin == NULL)
    show_error (c->line_number, "Could not find binary to execute", c->u.word[0]);

  // Next, open any file redirects and replace STDIN and STDOUT
  if(c->input != NULL)
    {
      char *input_path = get_redirect_file_path (c->input);
      int fd_in = open (input_path, O_RDONLY);

      free (input_path);
      if(fd_in == -1) show_error (c->line_number, "Error opening input file", c->input);

      dup2 (fd_in, STDIN_FILENO);
      close (fd_in);
    }

    if(c->output != NULL)
      {
        char *output_path = get_redirect_file_path (c->output);
        int fd_out = open (c->output,
            O_WRONLY | O_CREAT | O_TRUNC,           // Data from the file will be truncated
            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); // By default bash in posix mode will create files as rw-r--r--

        free (output_path);
        if(fd_out == -1) show_error (c->line_number, "Error opening output file", c->output);

        dup2 (fd_out, STDOUT_FILENO);
        close (fd_out);
      }

    // Execute!
    execvp (exec_bin, c->u.word + 1);

    // If we got here, there's a problem
    show_error (c->line_number, "Exec error", NULL);
}

char*
get_executable_path (char* bin_name)
{
  if(bin_name == NULL)
    return NULL;

  // Check if this is a valid absolute price
  if (file_exists (bin_name))
    return bin_name;

  // First, find the current working directory
  char *cwd = getcwd (NULL, 0);

  // Does it exist relative to the current directory?
  char *path = checked_malloc (sizeof(char) * (strlen(cwd) + strlen(bin_name) + 1 + 1)); // Add one char for '/' and one for NULL
  strcpy (path, cwd);
  strcat (path, "/");
  strcat (path, bin_name);

  if (file_exists (path))
    return path;

  // If not, let's try the system PATH variable and see if we have any matches there
  char *syspath = getenv ("PATH");

  // "The application shall ensure that it does not modify the string pointed to by the getenv() function."
  // By definition of getenv, we copy the string and modify the copy with strtok
  char *syspath_copy = checked_malloc(sizeof (char) * (strlen (syspath) + 1 + 1)); // Add one char for '/' and one for NULL
  strcpy (syspath_copy, syspath);

  // Split the path
  char **path_elements = NULL;
  char *p = strtok (syspath_copy, ":");
  int path_items = 0, i;

  while (p)
  {
    path_elements = checked_realloc (path_elements, sizeof (char*) * (path_items + 2));
    path_elements[path_items] = p;

    path_items++;

    p = strtok (NULL, ":");
  }

  path_elements[path_items] = NULL; // Make sure the last item is null for later looping

  // Now, let's iterate over each item in the path and see if it's a fit
  for (i = 0; path_elements[i] != NULL; i++)
  {
    size_t len = sizeof(char) * (strlen(bin_name) + strlen(path_elements[i]) + 1);
    path = checked_grow_alloc (path, &len);
    strcpy (path, path_elements[i]);
    strcat (path, "/");
    strcat (path, bin_name);

    if (file_exists (path))
    {
      free (path_elements);
      free (syspath_copy);
      return path;
    }
  }

  free (path_elements);
  free (syspath_copy);
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
  char *path = checked_malloc (sizeof(char) * (strlen(cwd) + strlen(redirect_file) + 1 + 1)); // Add a space for the slash and NULL
  strcpy (path, cwd);
  strcat (path, "/");
  strcat (path, redirect_file);

  return path;
}

int
timetravel (command_stream_t c_stream)
{
  if(c_stream == NULL || c_stream->stream_size == 0)
    return 0;

  form_dependency_graph (c_stream);

  int status;
  int i = 0;
  int num_finished = 0;
  command_t c = NULL;

  while (num_finished < c_stream->stream_size)
    {
      c = c_stream->commands[i];

      // If the command isn't currently running, hasn't finished, and all dependencies finished, start it!
      if(c->pid < 0 && !c->finished_running && !has_unran_dependency (c))
        {
          c->pid = fork();

          if(c->pid == 0) // We are in the child
            return execute_command (c);
        }

      // Use polling to see if the current command has finished
      if(c->pid > -1 && !c->finished_running)
        {
          // A return of 0 means the child is not yet finished
          // A return of -1 means an error, so we'll try again later
          if(waitpid (c->pid, &status, WNOHANG) > 0)
            {
              c->pid = -1;
              c->finished_running = true;

              // Check whether the child exited successfully
              // If it exited due to a signal record the signal instead
              if(WIFEXITED (status))
                c->status = status = WEXITSTATUS (status);

              num_finished++;
            }
        }

      // Loop around the stream circularly
      i++;
      i = i % c_stream->stream_size;
    }

  // Return the status of whichever command exited last
  return status;
}

void form_dependency_graph (command_stream_t c_stream)
{
  // If there is only one command, the graph is done
  if(c_stream == NULL || c_stream->stream_size == 1)
    return;

  int i, j;
  command_t independent = NULL;
  command_t dependent = NULL;

  // We offset the beginning of our loop by 1 as the first command is always independent
  for(i = 1; i < c_stream->stream_size; i++)
    {
      dependent = c_stream->commands[i];
      for(j = 0; j < i; j++)
        {
          independent = c_stream->commands[j];
          if(check_dependence (independent, dependent))
            add_dependency (dependent, independent);
        }
    }
}

bool
check_dependence (command_t indep, command_t dep)
{
  if(indep->output && dep->input && strcmp (indep->output, dep->input) == 0)
    return true;

  if(indep->output && dep->output && strcmp (indep->output, dep->output) == 0)
    return true;

  // If both commands are SIMPLE_COMMANs compare all their words and their I/O
  if(indep->type == SIMPLE_COMMAND && dep->type == SIMPLE_COMMAND)
    {
      // Skip the command names
      char **wi = indep->u.word + 1;
      while (*wi)
        {
          if( dep->input && strcmp(*wi, dep->input) == 0)
            return true;

          char **wd = dep->u.word + 1;
          while (*wd)
            {
              if((indep->output && strcmp (indep->output, *wd) == 0) || strcmp (*wi, *wd) == 0)
                return true;
              wd++;
            }
            wi++;
        }
    }
  else if(indep->type == SIMPLE_COMMAND) // dep is not a SIMPLE_COMMAND, compare only words in dep
    {
      char **wi = indep->u.word + 1;
      while (*wi)
        {
          if(dep->input && strcmp (*wi, dep->input) == 0)
            return true;
          wi++;
        }
    }
  else // dep is a SIMPLE_COMMAND, indep is not
    {
      char **wd = dep->u.word + 1;
      while (*wd)
        {
          if(indep->output && strcmp (indep->output, *wd) == 0)
            return true;
          wd++;
        }
    }

  // Traverse the independent command
  switch (indep->type)
    {
      case SEQUENCE_COMMAND:
      case PIPE_COMMAND:
      case OR_COMMAND:
      case AND_COMMAND:
        if(check_dependence (indep->u.command[0], dep) || check_dependence (indep->u.command[1], dep))
          return true;
        break;

      case SUBSHELL_COMMAND:
        if(check_dependence (indep->u.subshell_command, dep))
          return true;
        break;

      case SIMPLE_COMMAND: break;
      default: break;
    }

  // Traverse the dependent command
  switch (dep->type)
    {
      case SEQUENCE_COMMAND:
      case PIPE_COMMAND:
      case OR_COMMAND:
      case AND_COMMAND:
        if(check_dependence (indep, dep->u.command[0]) || check_dependence (indep, dep->u.command[1]))
          return true;
        break;

      case SUBSHELL_COMMAND:
        if(check_dependence (indep, dep->u.subshell_command))
          return true;
        break;

      case SIMPLE_COMMAND: break;
      default: break;
    }

  return false;
}

bool
add_dependency (command_t c, command_t dep)
{
  // We don't allow null commands or dependencies to be passed in
  if (c == NULL || dep == NULL)
    return false;

  // The two extra spaces allow us to have a NULL pointer at the end
  if (c->dependencies == NULL)
    {
      c->dep_alloc_size = 2;
      c->dependencies = checked_malloc (c->dep_alloc_size * sizeof (command_t));
    }
  else if (c->dep_size + 1 >= c->dep_alloc_size)
    c->dependencies = checked_grow_alloc (c->dependencies, &(c->dep_alloc_size));

  c->dependencies[c->dep_size++] = dep;
  c->dependencies[c->dep_size] = NULL; // This should be the case already, but let's be safe.
  
  return true;
}

bool
has_unran_dependency (command_t c)
{
  size_t i;
  for (i = 0; i < c->dep_size; i++)
  {
    if (c->dependencies[i]->finished_running == false)
      return true;
  }

  return false;
}