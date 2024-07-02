#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

// arglist - a list of char* arguments (words) provided by the user
// it contains count+1 items, where the last item (arglist[count]) and *only* the last is NULL
// RETURNS - 1 if should continue, 0 otherwise
int process_arglist(int count, char **arglist)
{ // should return 1 if no error occurs after every foreground child process it created exits.
	int i = 0;
	while (i < count)
	{
		if (strcasecmp(arglist[i], "&") == 0)
		{ // run the child process in the background
			return run_process_background(count, arglist);
		}
		if (strcasecmp(arglist[i], "|") == 0)
		{ // run two child processes, with the output of the first process piped to the input of the second process.
			return pipe_it_up(count, arglist, i);
		}
		if (strcasecmp(arglist[i], "<") == 0)
		{ // open the specified file and then run the child process, with the input redirected from the input file.
			return open_child_process_input(count, arglist);
		}
		if (strcasecmp(arglist[i], ">") == 0)
		{ // open the specified file and then run the child process, with the output redirected from the output file.
			return open_child_process_output(count, arglist);
		}
		i++;
	}
	return execute_general(count, arglist);
}
int run_process_background(int count, char **arglist)
{
	pid_t pid;
	pid = fork();
	if (pid == -1)
	{
		raise_error("Faild  during forking");
	}
	if (pid == 0)
	{ // Handaling child process
		arglist[count - 1] = NULL;
		if (signal(SIGCHLD, SIG_DFL) == SIG_ERR)
		{
			raise_error("Error - Could not change signal handling");
		}
		if (execvp(arglist[0], arglist) == -1)
		{
			raise_error("Error - Could not execute child process");
		}
	}
	return 1;
}
int pipe_it_up(int count, char **arglist, int i)
{
	pid_t pid1, pid2, curr_pid;
	int pipefd[2];
	int j, check;
	arglist[i] = NULL;
	if (pipe(pipefd[2]) == -1)
	{
		raise_error("Error - could not create pipe");
	}

	pid1 = fork();
	if (pid1 == -1)
	{
		raise_error("Faild  during forking");
	}
	curr_pid = pid1;
	for (int j = 0; j < 2; j++)
	{
		if (curr_pid == 0)
		{
			char error_message[100];

			if ((signal(SIGINT, SIG_DFL) == SIG_ERR) || (signal(SIGCHLD, SIG_DFL) == SIG_ERR))
			{
				snprintf(error_message, sizeof(error_message), "Error - Could not change signal handling *child %d*", j + 1);
				raise_error(error_message);
			}
			if (close(pipefd[j]) < 0)
			{
				snprintf(error_message, sizeof(error_message), "Error - Could not close read channel of pipe *child %d*", j + 1);
				raise_error(error_message);
			}
			if (dup2(pipefd[1 - j], 1 - j) == -1)
			{
				snprintf(error_message, sizeof(error_message), "Error - Could not redirect stdout of *child %d*", j + 1);
				raise_error(error_message);
			}
			if (close(pipefd[1 - j]) < 0)
			{
				snprintf(error_message, sizeof(error_message), "Error - Could not close write end of pipe *child %d*", j + 1);
				raise_error(error_message);
			}
			if (curr_pid == pid1 && execvp(arglist[0], arglist) == -1)
			{
				raise_error("Error - while executing command for *child 1* ");
			}
			if (curr_pid == pid2 && execvp(arglist[i + 1], arglist + i + 1) == -1)
			{
				raise_error("Error - while executing command for *child 2* ");
			}
		}
		pid2 = fork();
		if (pid2 == -1)
		{
			raise_error("Faild  during forking");
		}
		curr_pid = pid2;
	}
	if (close(pipefd[0]) < 0)
	{
		raise_error("Error - failed closing the read end of the pipe - parent process");
	}
	if (close(pipefd[1]) < 0)
	{
		raise_error("Error - failed closing the write end of the pipe - parent process");
	}
	if ((waitpid(pid1, &check, 0) == -1 && errno != ECHILD && errno != EINTR) || (waitpid(pid2, &check, 0) == -1 && errno != ECHILD && errno != EINTR))
	{
		perror("Error - failed waiting for children ");
		return 0;
	}
	return 1;
}
int open_child_process_input(int count, char **arglist)
{
	pid_t pid1;
	int input_file, check;
	arglist[count - 2] = NULL;
	pid1 = fork();
	if (pid1 == -1)
	{
		raise_error("Faild  during forking");
	}
	if (pid1 == 0)
	{
		if ((signal(SIGINT, SIG_DFL) == SIG_ERR) || (signal(SIGCHLD, SIG_DFL) == SIG_ERR))
		{
			raise_error("Error - Could not change signal handling");
		}

		input_file = open(arglist[count - 1], O_RDONLY | O_CREAT, 0777);
		if (input_file == -1)
		{
			raise_error("Error - Could not open the file descriptor - input");
		}

		if (dup2(input_file, STDIN_FILENO) < 0)
		{
			raise_error("Error - Could not reference the stdin to the file descriptor");
		}

		if (close(input_file) < 0)
		{
			raise_error("Error - Could not close the file descriptor");
		}

		if (execvp(arglist[0], arglist) < 0)
		{
			raise_error("Error - failed closing the read end of the pipe - child process");
		}
	}
	if ((waitpid(pid1, &check, 0) == -1) && (errno != ECHILD) && (errno != EINTR))
	{
		perror("Error - failed waiting for children ");
		return 0;
	}
	return 1;
}
int open_child_process_output(int count, char **arglist)
{
	pid_t pid1;
	int output_file, check;
	arglist[count - 2] = NULL;
	pid1 = fork();
	if (pid1 == -1)
	{
		raise_error("Faild  during forking");
	}
	if (pid1 == 0)
	{
		if ((signal(SIGINT, SIG_DFL) == SIG_ERR) || (signal(SIGCHLD, SIG_DFL) == SIG_ERR))
		{
			raise_error("Error - Could not change signal handling");
		}

		output_file = open(arglist[count - 1], O_RDONLY | O_CREAT | O_TRUNC, 0777);
		if (output_file == -1)
		{
			raise_error("Error - Could not open the file descriptor - output");
		}

		if (dup2(output_file, STDIN_FILENO) < 0)
		{
			raise_error("Error - Could not reference the stdout to the file descriptor");
		}

		if (close(output_file) < 0)
		{
			raise_error("Error - Could not close the file descriptor");
		}

		if (execvp(arglist[0], arglist) < 0)
		{
			raise_error("Error - failed closing the read end of the pipe - child process");
		}
	}
	if ((waitpid(pid1, &check, 0) == -1) && (errno != ECHILD) && (errno != EINTR))
	{
		perror("Error - failed waiting for children ");
		return 0;
	}
	return 1;
}
int execute_general(int count, char **arglist)
{
	pid_t pid = fork();
	if (pid1 == -1)
	{
		raise_error("Faild  during forking");
	}

	if (pid == 0)
	{

		if ((SIG_ERR == signal(SIGINT, SIG_DFL)) || (signal(SIGCHLD, SIG_DFL) == SIG_ERR))
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
	exit(1);
}

// prepare and finalize calls for initialization and destruction of anything required
int prepare(void)
{
	// source: http://www.microhowto.info/howto/reap_zombie_processes_using_a_sigchld_handler.html
	if (SIG_ERR == signal(SIGINT, SIG_IGN))
	{
		fprintf(stderr, "Error: Failed to set signal handler for SIGINT: %s\n", strerror(errno));
		return 1;
	}
	// For any other use, particularly setting a signal handler, you must use the sigaction system call.
	void sigchld_handler(int sig)
	{
		int new_errno = errno;
		while (waitpid((pid_t)(-1), 0, WNOHANG) > 0)
		{
		}
		errno = new_errno;
	}

	struct sigaction sa;
	sa.sa_handler = &sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
	if (sigaction(SIGCHLD, &sa, 0) == -1)
	{
		fprintf(stderr, "Error: Failed to set up SIGCHLD handler: %s\n", strerror(errno));
		return 1;
	}
	return 0;
};
int finalize(void)
{
	return 0 + 0;
}
