#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

int process_arglist(int count, char **arglist);
int run_process_background(int count, char **arglist);
int pipe_it_up(int count, char **arglist, int i);
int open_child_process_input(int count, char **arglist);
int open_child_process_output(int count, char **arglist);
int execute_general(int count, char **arglist);
void raise_error(const char *error_type);
int prepare(void);
int handle_signal(int signum, void (*action)(int));
int finalize(void);

// arglist - a list of char* arguments (words) provided by the user
// it contains count+1 items, where the last item (arglist[count]) and *only* the last is NULL
// RETURNS - 1 if should continue, 0 otherwise
int process_arglist(int count, char **arglist)
{

	if (count == 1)
	{
		return execute_general(count, arglist);
	}
	for (int i = 0; i < count; i++)
	{
		if (strcmp(arglist[i], "|") == 0)
		{
			// run two child processes, with the output of the first process piped to the input of the second process.
			return pipe_it_up(count, arglist, i);
		}

		if (strcmp(arglist[i], "&") == 0)
		{
			// run the child process in the background
			return run_process_background(count, arglist);
		}

		if (count >= 2 && (strcmp(arglist[i], "<") == 0))
		{
			// open the specified file and then run the child process, with the input redirected from the input file.
			return open_child_process_input(count, arglist);
		}

		if (count > 1 && (strcmp(arglist[i], ">>") == 0))
		{
			// open the specified file and then run the child process, with the output redirected from the output file.
			return open_child_process_output(count, arglist);
		}
	}
	return execute_general(count, arglist);
}

int run_process_background(int count, char **arglist)
{

	pid_t pid;
	pid = fork();
	if (pid == -1)
	{
		raise_error("Failed during forking");
	}
	if (pid == 0)
	{
		// Child process
		if (handle_signal(SIGCHLD, SIG_DFL) == 1)
		{
			raise_error("Error - Could not change signal handling");
		}
		arglist[count - 1] = NULL;
		if (execvp(arglist[0], arglist) == -1)
		{
			raise_error("Error - Could not execute child process");
		}
	}
	return 1;
}

int pipe_it_up(int count, char **arglist, int i)
{

	pid_t pid1, pid2;
	int pipefd[2];
	if (pipe(pipefd) == -1)
	{
		perror("Error - could not create pipe");
		return 0;
	}

	pid1 = fork();
	if (pid1 == -1)
	{
		perror("Failed during forking");
		return 0;
	}
	// Child process - 1st child.
	if (pid1 == 0)
	{

		close(pipefd[0]);
		if (handle_signal(SIGINT, SIG_DFL) + handle_signal(SIGCHLD, SIG_DFL) > 0)
		{
			raise_error("Error");
		}
		// Stdout to pipe
		if (dup2(pipefd[1], STDOUT_FILENO) == -1)
		{
			raise_error("Error - Could not redirect stdout of child process");
		}
		close(pipefd[1]);  // Close Write end
		arglist[i] = NULL; // Split arglist
		if (execvp(arglist[0], arglist) == -1)
		{
			raise_error("Error - while executing command");
		}
	}

	pid2 = fork();
	if (pid2 == -1)
	{
		perror("Failed during forking");
		return 0;
	}
	// Child process - 2nd child.
	if (pid2 == 0)
	{

		close(pipefd[1]);
		if (handle_signal(SIGINT, SIG_DFL) + handle_signal(SIGCHLD, SIG_DFL) > 0)
		{
			raise_error("Error");
		}
		// Stdin to pipe
		if (dup2(pipefd[0], STDIN_FILENO) == -1)
		{
			raise_error("Error - Could not redirect stdin of child process");
		}
		close(pipefd[0]); // Close Read end
		if (execvp(arglist[i + 1], &arglist[i + 1]) == -1)
		{
			raise_error("Error - Could not complete executing command");
		}
	}
	// Close both ends of the pipe (parent)
	close(pipefd[0]);
	close(pipefd[1]);

	// Wait for both child processes to finish
	if (waitpid(pid1, NULL, 0) == -1 && errno != ECHILD && errno != EINTR)
	{
		perror("failure during waitpid");
		return 0;
	}
	if (waitpid(pid2, NULL, 0) == -1 && errno != ECHILD && errno != EINTR)
	{
		perror("failure during waitpid");
		return 0;
	}

	return 1;
}

int open_child_process_input(int count, char **arglist)
{
	// Input
	pid_t pid;
	int input_file;
	arglist[count - 2] = NULL;
	pid = fork();
	if (pid == -1)
	{
		raise_error("Failed during forking");
	}
	// Child process
	if (pid == 0)
	{

		if ((handle_signal(SIGINT, SIG_DFL) + handle_signal(SIGCHLD, SIG_DFL) > 0))
		{
			raise_error("Error - Could not change signal handling");
		}

		input_file = open(arglist[count - 1], O_RDONLY);
		if (input_file == -1)
		{
			raise_error("Error - Could not open the file descriptor - input");
		}

		if (dup2(input_file, STDIN_FILENO) < 0)
		{
			raise_error("Error - Could not reference the stdin to the file descriptor");
		}

		close(input_file);

		if (execvp(arglist[0], arglist) < 0)
		{
			raise_error("Error - failed closing the read end of the pipe - child process");
		}
	}
	if ((waitpid(pid, NULL, 0) == -1) && (errno != EINTR) && (errno != ECHILD))
	{
		perror("Error - failed waiting for children ");
		return 0;
	}
	return 1;
}

int open_child_process_output(int count, char **arglist)
{
	// Output
	pid_t pid;
	int output_file;
	arglist[count - 2] = NULL;
	pid = fork();
	if (pid == -1)
	{
		raise_error("Failed during forking");
	}
	// Child process
	if (pid == 0)
	{

		if (handle_signal(SIGINT, SIG_DFL) + handle_signal(SIGCHLD, SIG_DFL) > 0)
		{
			raise_error("Error - Could not change signal handling");
		}

		output_file = open(arglist[count - 1], O_WRONLY | O_CREAT | O_TRUNC, 0777);
		if (output_file == -1)
		{
			raise_error("Error - Could not open the file descriptor - output");
		}

		if (dup2(output_file, STDOUT_FILENO) == -1)
		{
			raise_error("Error - Could not reference the stdout to the file descriptor");
		}

		close(output_file);

		if (execvp(arglist[0], arglist) < 0)
		{
			raise_error("Error - failed closing the read end of the pipe - child process");
		}
	}
	if ((waitpid(pid, NULL, 0) == -1) && (errno != ECHILD) && (errno != EINTR))
	{
		perror("Error - failed waiting for children ");
		return 0;
	}
	return 1;
}

int execute_general(int count, char **arglist)
{
	// Executes command and starts another one only after it is completed.
	pid_t pid = fork();
	if (pid == -1)
	{
		raise_error("Failed during forking");
	}
	// Child process
	if (pid == 0)
	{

		if (handle_signal(SIGINT, SIG_DFL) + handle_signal(SIGCHLD, SIG_DFL) > 0)
		{
			raise_error("Error - Could not change signal handling");
		}

		if (execvp(arglist[0], arglist) == -1)
		{
			raise_error("Error - Could not execute child process");
		}
	}
	if ((waitpid(pid, NULL, 0) < 0) && ((errno != ECHILD) && (errno != EINTR)))
	{
		raise_error("Error - failed waiting for children ");
	}

	return 1;
}

void raise_error(const char *error_type)
{
	perror(error_type);
	fprintf(stderr, "%s\n", strerror(errno));
	exit(1);
}

int prepare(void)
{

	return handle_signal(SIGINT, SIG_IGN) + handle_signal(SIGCHLD, SIG_IGN);
}

int handle_signal(int sig, void (*to_do)(int))
{
	// Sets the next action to do. Returns 1 on failure, 0 on succes
	if (signal(sig, to_do) == SIG_ERR)
	{
		perror("Error - Failed to change signal handling");
		return 1;
	}
	return 0;
}

int finalize(void)
{

	return 0;
}

// End of code :)
