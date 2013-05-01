// UCLA CS 111 Lab 1 main program

#include <errno.h>
#include <error.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>

#include "command.h"

static char const *program_name;
static char const *script_name;

static void
usage (void)
{
  error (1, 0, "usage: %s [-p] [-t | [-n <NUM SUBPROCESSES>]] SCRIPT-FILE", program_name);
}

static int
get_next_byte (void *stream)
{
  return getc (stream);
}

int
validate_subproc_argument (const char *arg)
{
  const char bad_arg_msg[] = "%s: <NUM SUBPROCESSES> must be a non-negative integer\n";
  const char out_of_bounds_msg[] = "%s: <NUM SUBPROCESSES> out of bounds\n";
  const char *p;
  int n;

  // Null arguments are invalid. getopt should report if the argument is missing, however...
  if (arg == NULL)
    {
      fprintf (stderr, bad_arg_msg, program_name);
      usage ();
    }

  // Check that the argument is a valid int
  p = arg;
  while(*p)
    {
      if (!isdigit (*p++))
        {
          fprintf (stderr, bad_arg_msg, program_name);
          usage ();
        }
    }

  // Check for overflows
  n = strtol (optarg, NULL, 10);
  if( (n == LONG_MAX || n == LONG_MAX) && errno == ERANGE )
    {
      fprintf (stderr, out_of_bounds_msg, program_name);
      usage ();
    }

  // n must be a non-negative integer
  if(n <= 0)
    {
      fprintf (stderr, bad_arg_msg, program_name);
      usage ();
    }

  return n;
}

int
main (int argc, char **argv)
{
  int command_number = 1;
  int num_processes = 0;
  bool print_tree = false;
  bool time_travel = false;
  bool print_lines = false;
  program_name = argv[0];

  for (;;)
    switch (getopt (argc, argv, "ptln:"))
      {
      case 'p': print_tree = true; break;
      case 't': time_travel = true; break;
      case 'l': print_lines = true; break;
      case 'n': num_processes = validate_subproc_argument (optarg); break;
      default: usage (); break;
      case -1: goto options_exhausted;
      }
 options_exhausted:;

  // There must be exactly one file argument.
  if (optind != argc - 1)
    usage ();

  script_name = argv[optind];
  FILE *script_stream = fopen (script_name, "r");
  if (! script_stream)
    error (1, errno, "%s: cannot open", script_name);
  command_stream_t command_stream =
    make_command_stream (get_next_byte, script_stream);

  int status = 0;
  command_t command;

  if (time_travel && !print_tree)
    {
      if(num_processes > 0)
        fprintf (stderr, "%s: using limit of %i subprocesses\n", program_name, num_processes);

      int status = timetravel (command_stream, num_processes);
      free_command_stream (command_stream);
      return status;
    }

  while ((command = read_command_stream (command_stream)))
    {
      if (print_tree)
        {
          printf ("# %d\n", command_number++);
          print_command (command, print_lines);

          status = 0;
        }
      else
        {
          execute_command (command, false);
          status = command_status (command);
        }
    }

  free_command_stream (command_stream);
  return status;
}
