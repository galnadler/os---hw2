#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h> 
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

void an_error_has_occured(const char*);
pid_t fork_without_errors();
int process_arglist(int, char**);
int execute_command_in_background(int, char**);
int execute_command(int, char**);
int piping_commands(int, char**, int);
int input_redirecting(int, char**);
int output_redirecting(int, char**);
int prepare(void);
int finalize(void);

void an_error_has_occured(const char *error_message){
    perror(error_message);
    fprintf(stderr,"%s\n", strerror(errno));
    exit(1);
}

pid_t fork_without_errors(){
    int ppid = fork();
    if (ppid < 0){
        an_error_has_occured("error while attempting to fork");
    } 
    return ppid;
}

int process_arglist(int count, char **arglist)
{
    for (int i = 0; i < count; i++){
        if (strcmp(arglist[i],"&")==0){
            return execute_command_in_background(count, arglist);
        }
        if (strcmp(arglist[i],"|")==0){
            return piping_commands(count, arglist, i);
        }
        if (strcmp(arglist[i],"<")==0){
            return input_redirecting(count, arglist);
        }
        if (strcmp(arglist[i],">")==0){
            return output_redirecting(count, arglist);
        }
    }
    return execute_command(count, arglist);
}

int execute_command_in_background(int count, char** arglist){
    pid_t ppid;
    ppid = fork_without_errors();
    //child process
    if (ppid == 0){
        arglist[count - 1] = NULL;
        if (signal(SIGCHLD, SIG_DFL) == SIG_ERR) { 
            an_error_has_occured("Error occurred while attempting to change signal handling");
        }
        if (execvp(arglist[0],arglist) == -1){
            an_error_has_occured("Error occurred during execution of child process");
        }
    }
    return 1;
}

int execute_command(int count, char** arglist){
    pid_t ppid;
    ppid = fork_without_errors();
    //child process
    if(ppid == 0){
        // Changing the treatment of signal back to default
        if ((SIG_ERR == signal(SIGINT,SIG_DFL)) || (signal(SIGCHLD, SIG_DFL) == SIG_ERR)) {
            an_error_has_occured("Error occurred while attempting to change signal handling");
        }
        // Trying to execute the command
        if(execvp(arglist[0], arglist) == -1){
            an_error_has_occured("Error occurred during execution of the child process");
        }
    }
    // Parent process
    else{
        // Waiting for the child process to complete
        if((waitpid(ppid, NULL, 0) < 0) && ((errno != ECHILD) && (errno != EINTR))){
            an_error_has_occured("Waiting for the child process failed");
        }   
    }
    return 1;
}

/*
 * Execute commands connected by a pipe.
 * 
 * Parameters:
 *   - count: The total number of arguments.
 *   - arglist: An array of strings containing the command and its arguments.
 *   - index: The index where the pipe character '|' was found in the arglist.
 * 
 * Returns:
 *   - 1 on success, 0 on failure.
 */
int piping_commands(int count, char** arglist, int index){
    int pipefd[2];
    arglist[index] = NULL;

    if(pipe(pipefd) == -1){
        an_error_has_occured("Error while attempting to create pipe");
    }

    // Fork the first child process
    pid_t pid_1;
    pid_1 = fork_without_errors();

    // Child 1 process
    if (pid_1 == 0){

        // Set signal handling for child 1
        if ((signal(SIGINT, SIG_DFL) == SIG_ERR) || (signal(SIGCHLD, SIG_DFL) == SIG_ERR)){
            an_error_has_occured("Error while attempting to change signal handling for child 1");
        }

        // Close read channel of the pipe
        if(close(pipefd[0]) < 0){
            an_error_has_occured("Error in closing read channel of pipe for child 1");
        }

        // Redirect stdout to the write end of the pipe
        if(dup2(pipefd[1], 1) == -1){
            an_error_has_occured("Error in redirecting stdout of child 1 to pipe");
        }

        // Close the write end of the pipe
        if(close(pipefd[1]) < 0){
            an_error_has_occured("Error in closing the write end of the pipe for child 1");
        }

        // Execute command for child 1
        if(execvp(arglist[0], arglist) == -1){
            an_error_has_occured("Error occurred during execution of command for child 1");
        }
    }

    // Fork the second child process
    pid_t pid_2;
    pid_2 = fork_without_errors();

    // Child 2 process
    if(pid_2 == 0){
        // Set signal handling for child 2
        if ((SIG_ERR == signal(SIGINT,SIG_DFL)) || (signal(SIGCHLD, SIG_DFL) == SIG_ERR)) {
            an_error_has_occured("Error while attempting to change signal handling for child 2");
        }

        // Close the write end of the pipe (child 2 only needs to read)
        if(close(pipefd[1]) < 0){
            an_error_has_occured("Error in closing the write end of the pipe for child 2");
        }

        // Connect the read end of the pipe to STDIN of child 2 
        if(dup2(pipefd[0], 0) == -1){
            an_error_has_occured("Error in redirecting stdin of child 2 to the pipe");
        }

        // Close the read end of the pipe
        if(close(pipefd[0]) < 0){
            an_error_has_occured("Error in closing the read end of the pipe for child 2");
        }

        // Execute command for child 2
        if(execvp(arglist[index + 1], arglist + index + 1) == -1){
            an_error_has_occured("Error occurred during execution of command for child 2");
        }
    }

    // Close both ends of the pipe in the parent process
    if(close(pipefd[0]) < 0){
            an_error_has_occured("Error in closing the read end of the pipe in the parent process");
    }
    if(close(pipefd[1]) < 0){
            an_error_has_occured("Error in closing the write end of the pipe in the parent process");
    }

    // Parent process waits for both child processes to finish
    int status;
    if((waitpid(pid_1, &status, 0) == -1 && errno != ECHILD && errno != EINTR) || (waitpid(pid_2, &status, 0) == -1 && errno != ECHILD && errno != EINTR)){
        perror("Waiting for children was failed");
        return 0;
    }

    // Execution completed successfully
    return 1;
}

int input_redirecting(int count, char** arglist){
    // Null-terminate the arglist at the input redirection symbol
    arglist[count - 2] = NULL;

    // Fork a new process
    pid_t pid = fork_without_errors();

    // Child process
    if(pid == 0){
        // Change the signal handling to default for SIGINT and SIGCHLD
        if ((signal(SIGINT, SIG_DFL) == SIG_ERR) || (signal(SIGCHLD, SIG_DFL) == SIG_ERR)) {
            an_error_has_occured("Error occurred while attempting to change signal handling");
        }

        // Open the input file descriptor
        int input_fd = open(arglist[count - 1], O_RDONLY | O_CREAT , 0777);
        if(input_fd == -1){
            an_error_has_occured("Error occurred while opening the file descriptor");
        }

        // Redirect the stdin to the input file descriptor
        if(dup2(input_fd, STDIN_FILENO) < 0){
            an_error_has_occured("Error occurred while referencing the stdin to the file descriptor");
        }
        
        // Close the input file descriptor as it's no longer needed
        if(close(input_fd) < 0){
            an_error_has_occured("Error occurred while closing the file descriptor");
        }

        // Execute the command with redirected stdin
        if(execvp(arglist[0], arglist) < 0){
            an_error_has_occured("Error occurred during execution of the child process");
        }
    }
    // Parent process
    else{
        int status;
        // Wait for the child process to complete
        if((waitpid(pid, &status, 0) == -1) && (errno != ECHILD) && (errno != EINTR)){
            perror("Waiting for child process failed");
            return 0;
        }       
    }
    return 1;
}


int output_redirecting(int count, char** arglist){
    int output_fd;
    pid_t pid = fork_without_errors();

    // Child process
    if(pid == 0){
        // Change signal handling back to default
        if ((signal(SIGINT, SIG_DFL) == SIG_ERR) || (signal(SIGCHLD, SIG_DFL) == SIG_ERR)) {
            an_error_has_occured("Error while attempting to change signal handling");
        }

        // Open the output file for writing (create if it doesn't exist, truncate if it does)
        output_fd = open(arglist[count - 1], O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if(output_fd == -1){
            an_error_has_occured("Error in opening the output file descriptor");
        }

        // Redirect stdout to the output file
        if(dup2(output_fd, STDOUT_FILENO) < 0){
            an_error_has_occured("Error in redirecting stdout to the output file descriptor");
        }
        
        // Close the output file descriptor as it's no longer needed
        if(close(output_fd) < 0){
            an_error_has_occured("Error in closing the output file descriptor");
        }

        // Execute the command with redirected stdout
        if(execvp(arglist[0], arglist) < 0){
            an_error_has_occured("Error occurred in the child process during command execution");
        }
    }
    // Parent process
    else{
        int status;
        // Wait for the child process to complete
        if((waitpid(pid, &status, 0) == -1) && (errno != ECHILD) && (errno != EINTR)){
            perror("Waiting for child process was failed");
            return 0;
        }       
    }
    return 1;
}


int prepare(void){
    // taken from http://www.microhowto.info/howto/reap_zombie_processes_using_a_sigchld_handler.html
    // Ignore SIGINT to prevent shell termination
    if (SIG_ERR == signal(SIGINT, SIG_IGN)){
        fprintf(stderr, "Error: Failed to set signal handler for SIGINT: %s\n", strerror(errno));
        return 1;
    }

    // Define SIGCHLD handler to reap zombie processes
    void sigchld_handler(int sig) {
    int new_errno = errno;
    while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {}
    errno = new_errno;
    }

    // Set up SIGCHLD handler
    struct sigaction sa;
    sa.sa_handler = &sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, 0) == -1) {
        fprintf(stderr, "Error: Failed to set up SIGCHLD handler: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}

int finalize(void){
    return 0;
}