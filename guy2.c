#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

int set_signal_handler(int signum, void (*action)(int));
int exec_command(char **arglist);
int is_pipe(int count, char **arglist);
int sin_piping(int pipe_index, char **arglist);
int redirecting(int type, int count, char **arglist);
int exec__background(int count, char **arglist);
int prepare(void);
int process_arglist(int count, char** arglist);
int finalize(void);

// signal handlers -  set the action that should be taken. returns 1 on failure, 0 on success.
int set_signal_handler(int signum, void (*action)(int)) {
    if (signal(signum, action) == SIG_ERR) {
        perror("Error - failed to change signal handling");
        return 1;}
    return 0;
}

//executes foreground command and waits until it completes before starts another one
int exec_command(char **arglist) {
    pid_t pid;
    pid = fork();
    if (pid == -1) { // fork failure
        perror("failure during forking");
        return 0;}
    // child
    if (pid == 0) {
        // foreground child processes should terminate upon SIGINT.
        // restore default handler for SIGCHLD before execvp
        if (set_signal_handler(SIGINT, SIG_DFL) + set_signal_handler(SIGCHLD, SIG_DFL) > 0) {
            exit(1);}
        // executing the command, if it fails - print error and exit from child process
        if (execvp(arglist[0], arglist) == -1) { 
            perror("failure during executing the command");
            exit(1);;
        }   
    }
    // parent - waiting for child process to terminate
    if (waitpid(pid, NULL, 0) == -1 && errno != ECHILD && errno != EINTR) {
        // ECHILD and EINTR in the parent shell process are not considered as an actual errors according to the instructions
        perror("failure during waitpid");
        return 0;
        }   
    return 1;
}

// checks if the command in arglist is pipe (contains "|"). returns the index if it is piping, count+2 else.
int is_pipe(int count, char **arglist) {
    int i;
    for (i = 0; i < count; i++) {
        if (arglist[i][0] == '|'){
            return i;
        }
    }
    return count+2;
}

// run two child process, with the output of the first process as the input of the second one, using single piping
int sin_piping(int pipe_index, char **arglist) {
    int pipefd[2];
    pid_t pid1; pid_t pid2;
    // pipefd is an array of two file descriptors. 
    // After pipe() - pipefd[0] will refer to the read end, and pipefd[1] will refer to the write end.
    if (pipe(pipefd) == -1) {
        perror("failure during pipe");
        return 0;}
    pid1 = fork();
    if (pid1 == -1) { // fork failure
        perror("failure during forking");
        return 0;}
    // first child - write to the pipe his output
    if (pid1 == 0) {
        close(pipefd[0]); // close read end of the pipe
        // foreground child processes should terminate upon SIGINT.
        // restore default handler for SIGCHLD before execvp
        if (set_signal_handler(SIGINT, SIG_DFL) + set_signal_handler(SIGCHLD, SIG_DFL) > 0) {
            exit(1);}
        // redirect first child output (stdout) to the pipe write end
        if (dup2(pipefd[1],STDOUT_FILENO) == -1) {
            perror("failure during redirection (stdout) to the pipe write end");
            exit(1);}
        close(pipefd[1]); // close write end of the pipe after redirection
        // execute first command
        arglist[pipe_index] = NULL;
        if (execvp(arglist[0], arglist) == -1) {
                perror("failure during executing the command");
                exit(1);}
    }
    // parent
    pid2 = fork();
    if (pid2 == -1) { // fork failure
        perror("failure during forking");
        return 0;}
    // second child - read from the pipe his input
    if (pid2 == 0) {
        close(pipefd[1]); // close write end of the pipe
        // foreground child processes should terminate upon SIGINT.
        // restore default handler for SIGCHLD before execvp
        if (set_signal_handler(SIGINT, SIG_DFL) + set_signal_handler(SIGCHLD, SIG_DFL) > 0) {
            exit(1);}
        // redirect second child input (stdin) to the pipe read end
        if (dup2(pipefd[0],STDIN_FILENO) == -1) {
            perror("failure during redirection (stdin) to the pipe read end");
            exit(1);}
        close(pipefd[0]); // close read end of the pipe after redirection
        // execute second command
        if (execvp(arglist[pipe_index+1], &arglist[pipe_index+1]) == -1) {
                perror("failure during executing the command");
                exit(1);}
    }
    // parent
    close(pipefd[0]); close(pipefd[1]); // close write and read ends of the pipe 
    // waiting for both childs to terminate
    // ECHILD and EINTR in the parent shell process are not considered as an actual errors according to the instructions
    if (waitpid(pid1, NULL, 0) == -1 && errno != ECHILD && errno != EINTR) {
        perror("failure during waitpid");
        return 0;}
    if (waitpid(pid2, NULL, 0) == -1 && errno != ECHILD && errno != EINTR) {
        perror("failure during waitpid");
        return 0;}
    return 1;
}

// execute the command so the standard output\input is redirected to the output\input file accordingly (depends on type)
// type - 0 is input redirection and 1 is output redirection
int redirecting(int type, int count, char **arglist) {
    pid_t pid; int fd;
    arglist[count - 2] = NULL; // leave only the command part in arglist
    pid = fork();
    if (pid == -1) { // fork failure
        perror("failure during forking");
        return 0;}
    // child
    if (pid == 0) {
        // foreground child processes should terminate upon SIGINT.
        // restore default handler for SIGCHLD before execvp
        if (set_signal_handler(SIGINT, SIG_DFL) + set_signal_handler(SIGCHLD, SIG_DFL) > 0) {
            exit(1);}
        
        // input redirection
        if(type == 0) {
            // open file in order to redirect the input for the command.
            // read only permission (reasonable for stdin).
            fd = open(arglist[count - 1], O_RDONLY);
            if (fd == -1) {
                perror("failure during opening file");
                exit(1);}
            // Duplicate the file descriptor for standard input (STDIN_FILENO)
            if (dup2(fd, STDIN_FILENO) == -1) {
                perror("failure during redirection (stdin)");
                exit(1);}
            // Close the original file descriptor
            close(fd);
        }
        // output redirection
        if(type == 1) {
            // open file for redirecting the output of the command.
            // if the file exists, its contents will be overwritten, and if it doesn't exist, a new file will be created.
            // read, write, and execute permissions for all users (reasonable for stdout).
            fd = open(arglist[count - 1], O_WRONLY | O_CREAT | O_TRUNC, 0777);
            if (fd == -1) {
                perror("failure during opening file");
                exit(1);}
            // Duplicate the file descriptor for standard output (STDOUT_FILENO)
            if (dup2(fd, STDOUT_FILENO) == -1) {
                perror("failure during redirection (stdout)");
                exit(1);}
            // Close the original file descriptor
            close(fd);
        }
        // executing the command for both cases (input\output), if it fails - print error and exit from child process
        if (execvp(arglist[0], arglist) == -1) { 
            perror("failure during executing the command");
            exit(1);}   
    }
    // parent - waiting for child process to terminate
    if (waitpid(pid, NULL, 0) == -1 && errno != ECHILD && errno != EINTR) {
        // ECHILD and EINTR in the parent shell process are not considered as an actual errors according to the instructions
        perror("failure during waitpid");
        return 0;
        }   
    return 1;
}

// execute the background command - doesnt wait for termination before accepting another command from the shell
int exec__background(int count, char **arglist) {
    pid_t pid;
    pid = fork();
    if (pid == -1) { // fork failure
        perror("failure during forking");
        return 0;} 
    // child
    else if (pid == 0) { 
        if(set_signal_handler(SIGCHLD,SIG_DFL) == 1) { // restore default handler for SIGCHLD before execvp
            exit(1);}
        arglist[count - 1] = NULL; // remove & argument from arglist
        if (execvp(arglist[0], arglist) == -1) { // executing the command, if it fails - print error and exit from child process
            perror("failure during executing the command");
            exit(1);
        }
    }
    // parent (doesn't wait for child termination)
    return 1;
}

// after prepare() finishes, the shell and background childs should not terminate upon SIGINT.
// setting the handler of SIGCHLD to SIG_IGN in order to prevent zombies.
// returns - 0 on success, > 0 on failure
int prepare(void) { 
    return set_signal_handler(SIGINT, SIG_IGN) + set_signal_handler(SIGCHLD, SIG_IGN);
}

// execute the command from the shell
// waits for foreground child process, doesnt wait for background child process
// returns 1 on success, 0 on failure
int process_arglist(int count, char **arglist) {
    int ret, pipe_index;
    ret = 0;
    pipe_index = is_pipe(count, arglist);
    // background command
    if (arglist[count - 1][0] == '&') {
        ret = exec__background(count, arglist);} 
    // piping
    else if (pipe_index != count+2) {
        ret = sin_piping(pipe_index, arglist);}
    // input redirection
    else if (count > 1 && arglist[count - 2][0] == '<') {
        ret = redirecting(0, count, arglist);} 
    // output redirection
    else if (count > 1 && arglist[count - 2][0] == '>') {
        ret = redirecting(1, count, arglist);} 
    // other (foreground) commands
    else {
        ret = exec_command(arglist);
    }
    return ret;
}

// do nothing
int finalize(void) {
    return 0;
}
