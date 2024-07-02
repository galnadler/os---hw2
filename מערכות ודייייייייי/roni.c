#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>

#define FALSE 0
#define TRUE 1
#define ERROR -1

int prepare(void);
int execute_in_background(char **arglist, int count);
int execute_pipe(char **arglist, int pipe_idx);
int redirect_and_exec(char **arglist, int fd, int std_fd);
int execute_redirection(int is_input, char **arglist, int count);
int execute_commands(char **arglist);
int process_arglist(int count, char **arglist);
int finalize(void);

int prepare(void)
{
    if ((signal(SIGCHLD, SIG_IGN) == SIG_ERR) || (signal(SIGINT, SIG_IGN) == SIG_ERR))
    {
        perror("Error handling signals");
        return 1;
    }
    return 0;
}

int execute_in_background(char **arglist, int count)
{
    pid_t p = fork();
    if (p == ERROR)
    {
        perror("Error in fork");
        return 0;
    }

    else if (p == 0)
    {
        if (signal(SIGCHLD, SIG_DFL) == SIG_ERR)
        {
            perror("Error handling signal");
            exit(1);
        }
        arglist[count - 1] = NULL;
        if (execvp(arglist[0], arglist) == ERROR)
        {
            perror("Error in command execution");
            exit(1);
        }
    }
    return 1;
}

int execute_pipe(char **arglist, int pipe_idx)
{
    int pipefd[2];
    if (pipe(pipefd) == ERROR)
    {
        perror("Error creating pipe");
        return 0;
    }
    pid_t p1 = fork();
    if (p1 == ERROR)
    {
        perror("Error in fork");
        return 0;
    }
    if (p1 == 0)
    {
        close(pipefd[0]);
        if ((signal(SIGCHLD, SIG_DFL) == SIG_ERR) || (signal(SIGINT, SIG_DFL) == SIG_ERR))
        {
            perror("Error handling signal");
            exit(1);
        }
        if (dup2(pipefd[1], STDOUT_FILENO) == ERROR)
        {
            perror("Error redirecting to pipe");
            exit(1);
        }
        close(pipefd[1]);
        arglist[pipe_idx] = NULL;
        if (execvp(arglist[0], arglist) == ERROR)
        {
            perror("Error in command execution");
            exit(1);
        }
    }
    pid_t p2 = fork();
    if (p2 == ERROR)
    {
        perror("Error in fork");
        return 0;
    }
    if (p2 == 0)
    {
        close(pipefd[1]);
        if ((signal(SIGCHLD, SIG_DFL) == SIG_ERR) || (signal(SIGINT, SIG_DFL) == SIG_ERR))
        {
            perror("Error handling signal");
            exit(1);
        }
        if (dup2(pipefd[0], STDIN_FILENO) == ERROR)
        {
            perror("Error redirecting to pipe");
            exit(1);
        }
        close(pipefd[0]);
        if (execvp(arglist[pipe_idx + 1], &arglist[pipe_idx + 1]) == ERROR)
        {
            perror("Error in command execution");
            exit(1);
        }
    }
    for (int i = 0; i < 2; i++)
    {
        close(pipefd[i]);
    }
    int waitpid_error = (waitpid(p1, NULL, 0) == ERROR) || (waitpid(p2, NULL, 0) == ERROR);
    if ((waitpid_error) && (errno != ECHILD) && (errno != EINTR))
    {
        perror("Error in waitpid");
        return 0;
    }
    return 1;
}

int redirect_and_exec(char **arglist, int fd, int std_fd)
{
    if (dup2(fd, std_fd) == ERROR)
    {
        perror("Error in redirecting");
        close(fd);
        exit(1);
    }
    close(fd);
    if (execvp(arglist[0], arglist) == ERROR)
    {
        perror("Error in command execution");
        exit(1);
    }
    return 0;
}

int execute_redirection(int is_input, char **arglist, int count)
{
    arglist[count - 2] = NULL;
    pid_t p = fork();
    if (p == ERROR)
    {
        perror("Error in fork");
        return 0;
    }
    if (p == 0)
    {
        if ((signal(SIGCHLD, SIG_DFL) == SIG_ERR) || (signal(SIGINT, SIG_DFL) == SIG_ERR))
        {
            perror("Error handling signal");
            exit(1);
        }
        if (is_input)
        {
            int input_fd = open(arglist[count - 1], O_RDONLY);
            if (input_fd == ERROR)
            {
                perror("Error in opening file");
                exit(1);
            }
            redirect_and_exec(arglist, input_fd, STDIN_FILENO);
        }
        else
        {
            int output_fd = open(arglist[count - 1], O_WRONLY | O_CREAT | O_APPEND, 0777);
            if (output_fd == ERROR)
            {
                perror("Error in opening file");
                exit(1);
            }
            redirect_and_exec(arglist, output_fd, STDOUT_FILENO);
        }
    }
    if ((waitpid(p, NULL, 0) == ERROR) && (errno != ECHILD) && (errno != EINTR))
    {
        perror("Error in waitpid");
        return 0;
    }
    return 1;
}

int execute_commands(char **arglist)
{
    pid_t p = fork();
    if (p == ERROR)
    {
        perror("Error in fork");
        return 0;
    }
    if (p == 0)
    {
        if ((signal(SIGCHLD, SIG_DFL) == SIG_ERR) || (signal(SIGINT, SIG_DFL) == SIG_ERR))
        {
            perror("Error handling signal");
            exit(1);
        }
        if (execvp(arglist[0], arglist) == ERROR)
        {
            perror("Error in command execution");
            exit(1);
        }
    }
    if ((waitpid(p, NULL, 0) == ERROR) && (errno != ECHILD) && (errno != EINTR))
    {
        perror("Error in waitpid");
        return 0;
    }
    return 1;
}

int process_arglist(int count, char **arglist)
{
    if (arglist[count - 1][0] == '&')
    {
        return execute_in_background(arglist, count);
    }

    int pipe_idx = 0;
    while (pipe_idx < count)
    {
        if (arglist[pipe_idx][0] == '|')
        {
            return execute_pipe(arglist, pipe_idx);
        }
        pipe_idx++;
    }

    if (count > 1)
    {
        if (arglist[count - 2][0] == '<')
        {
            return execute_redirection(TRUE, arglist, count);
        }
        else if ((strlen(arglist[count - 2]) == 2) && (strcmp(arglist[count - 2], ">>") == FALSE))
        {
            return execute_redirection(FALSE, arglist, count);
        }
    }
    return execute_commands(arglist);
}

int finalize(void)
{
    return 0;
}
