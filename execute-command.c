// UCLA CS 111 Lab 1 command execution

#include "alloc.h"
#include "command.h"
#include "command-internals.h"

#include <error.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>


/* FIXME: You may need to add #include directives, macro definitions,
   static function definitions, etc.  */

int
command_status (command_t c)
{
  return c->status;
}

void
execute_command (command_t c, bool time_travel)
{
  /* FIXME: Replace this with your implementation.  You may need to
     add auxiliary functions and otherwise modify the source code.
     You can also use external functions defined in the GNU C Library.  */

  // "Use" the arguments to avoid compiler warnings until implemented
  (void)c;
  (void)time_travel;
  error (1, 0, "command execution not yet implemented");
}

void
execute_simple_command (command_t c, char **output, bool in_subshell)
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