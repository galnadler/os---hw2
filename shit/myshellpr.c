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
{ // should return 1 if no error occurs after every foreground child process it created exits.
	printf("Entered process_arglist\n");
	int i = 0;
	while (i < count)
	{
		if (arglist[i][0] == '&')
		{ // run the child process in the background
			return run_process_background(count, arglist);
		}
		if (arglist[i][0] == '|')
		{ // run two child processes, with the output of the first process piped to the input of the second process.
			return pipe_it_up(count, arglist, i);
		}
		if (arglist[i][0] == '<' && count >= 2)
		{ // open the specified file and then run the child process, with the input redirected from the input file.
			return open_child_process_input(count, arglist);
		}
		if (strcmp(arglist[i], ">>") == 0)
		{ // open the specified file and then run the child process, with the output redirected from the output file.
			return open_child_process_output(count, arglist);
		}
		i++;
	}
	return execute_general(count, arglist);
}

int run_process_background(int count, char **arglist)
{
	printf("Entered run_process_background\n");
	pid_t pid;
	pid = fork();
	if (pid == -1)
	{
		raise_error("Failed during forking");
	}
	if (pid == 0)
	{ // Handling child process

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
	printf("Entered pipe_it_up\n");
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

	if (pid1 == 0)
	{ // First child process
		close(pipefd[0]);
		if (handle_signal(SIGINT, SIG_DFL) + handle_signal(SIGCHLD, SIG_DFL) > 0)
		{
			raise_error("Error");
		}
		if (dup2(pipefd[1], STDOUT_FILENO) == -1)
		{ // Redirect stdout to pipe
			raise_error("Error - Could not redirect stdout of child process");
		}
		close(pipefd[1]);  // Close the write end after redirection
		arglist[i] = NULL; // Split the arglist at the pipe
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

	if (pid2 == 0)
	{					  // Second child process
		close(pipefd[1]); // Close unused write end
		if (handle_signal(SIGINT, SIG_DFL) + handle_signal(SIGCHLD, SIG_DFL) > 0)
		{
			raise_error("Error");
		}
		if (dup2(pipefd[0], STDIN_FILENO) == -1)
		{ // Redirect stdin to pipe
			raise_error("Error - Could not redirect stdin of child process");
		}
		close(pipefd[0]); // Close the read end after redirection
		if (execvp(arglist[i + 1], arglist + i + 1) == -1)
		{
			raise_error("Error - while executing command");
		}
	}

	// Close both ends of the pipe in the parent process
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
	printf("Entered open_child_process_input\n");
	pid_t pid1;
	int input_file;
	arglist[count - 2] = NULL;
	pid1 = fork();
	if (pid1 == -1)
	{
		raise_error("Failed during forking");
	}
	if (pid1 == 0)
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
	if (waitpid(pid1, NULL, 0) == -1 && errno != ECHILD && errno != EINTR)
	{
		raise_error("Error - failure during waitpid");
	}
	return 1;
}

int open_child_process_output(int count, char **arglist)
{
	printf("Entered open_child_process_output\n");
	pid_t pid1;
	int output_file;
	arglist[count - 2] = NULL;
	pid1 = fork();
	if (pid1 == -1)
	{
		raise_error("Failed during forking");
	}
	if (pid1 == 0)
	{
		if (handle_signal(SIGINT, SIG_DFL) + handle_signal(SIGCHLD, SIG_DFL) > 0)
		{
			raise_error("Error - Could not change signal handling");
		}

		output_file = open(arglist[count - 1], O_CREAT | O_TRUNC | O_WRONLY, 0644);
		if (output_file == -1)
		{
			raise_error("Error - Could not open the file descriptor - output");
		}

		if (dup2(output_file, STDOUT_FILENO) < 0)
		{
			raise_error("Error - Could not reference the stdout to the file descriptor");
		}

		close(output_file);

		if (execvp(arglist[0], arglist) < 0)
		{
			raise_error("Error - failed closing the read end of the pipe - child process");
		}
	}
	if (waitpid(pid1, NULL, 0) == -1 && errno != ECHILD && errno != EINTR)
	{
		raise_error("Error - failure during waitpid");
	}
	return 1;
}

int execute_general(int count, char **arglist)
{
	printf("Entered execute_general\n");
	pid_t pid;
	int status;
	pid = fork();
	if (pid == -1)
	{
		raise_error("Failed during forking");
	}
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
	if (waitpid(pid, &status, 0) == -1 && errno != ECHILD && errno != EINTR)
	{
		raise_error("Error - failure during waitpid");
	}
	return 1;
}

void raise_error(const char *error_type)
{
	printf("%s, description : %s\n", error_type, strerror(errno));
	exit(EXIT_FAILURE);
}

int prepare(void)
{
	printf("Entered prepare\n");
	if (handle_signal(SIGCHLD, SIG_IGN) == 1)
	{
		raise_error("Error - could not ignore");
	}
	return 0;
}

int handle_signal(int signum, void (*action)(int))
{
	printf("Entered handle_signal\n");
	struct sigaction sa;
	sa.sa_handler = action;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(signum, &sa, NULL) == -1)
	{
		raise_error("Error - could not reference the signal handling");
		return 1;
	}
	return 0;
}

int finalize(void)
{
	printf("Entered finalize\n");
	return 0;
}
