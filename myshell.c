#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>

// Signal handling function to set specified signals to be ignored
void setup_signal_handler(int sig) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = SA_RESTART;

    if (sigaction(sig, &sa, 0) == -1) {
        perror("sigaction failed");
        exit(1);
    }
}

// Prepare function for initial setup
int prepare(void) {
    setup_signal_handler(SIGCHLD);  // Prevent zombie processes
    setup_signal_handler(SIGINT);   // Ignore interrupt signal in shell
    return 0;
}

// Function to reset signal handling to default behavior for child processes
void reset_signal_handler() {
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, 0) == -1) {
        perror("sigaction failed");
    }
}

// Function to execute a command with specified input/output redirection and background option
int execute_command(char **args, int in_background, int input_fd, int output_fd) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return 0;
    } else if (pid == 0) {  // Child process
        reset_signal_handler();
        if (input_fd != STDIN_FILENO) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }
        if (output_fd != STDOUT_FILENO) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }
        execvp(args[0], args);
        perror("execvp failed");
        exit(1);
    }
    if (!in_background) {  // Parent process waits for foreground processes
        waitpid(pid, NULL, 0);
    }
    return 1;
}

// Process argument list to determine type of command and execute accordingly
int process_arglist(int count, char **args) {
    int pipe_index = -1, input_redir_index = -1, append_redir_index = -1;
    int in_background = strcmp(args[count - 1], "&") == 0;

    for (int i = 0; i < count; i++) {
        if (strcmp(args[i], "|") == 0) {
            pipe_index = i;
        } else if (strcmp(args[i], "<") == 0) {
            input_redir_index = i;
        } else if (strcmp(args[i], ">>") == 0) {
            append_redir_index = i;
        }
    }

    if (in_background) {
        args[count - 1] = NULL;  // Remove '&' from arguments
    }

    if (pipe_index != -1) {
        args[pipe_index] = NULL;
        int pipe_fd[2];
        if (pipe(pipe_fd) == -1) {
            perror("pipe failed");
            return 0;
        }
        execute_command(args, 0, STDIN_FILENO, pipe_fd[1]);  // Execute first command
        close(pipe_fd[1]);
        execute_command(args + pipe_index + 1, in_background, pipe_fd[0], STDOUT_FILENO);  // Execute second command
        close(pipe_fd[0]);
    } else if (input_redir_index != -1) {
        args[input_redir_index] = NULL;
        int input_fd = open(args[input_redir_index + 1], O_RDONLY);
        if (input_fd == -1) {
            perror("open failed");
            return 0;
        }
        execute_command(args, in_background, input_fd, STDOUT_FILENO);
        close(input_fd);
    } else if (append_redir_index != -1) {
        args[append_redir_index] = NULL;
        int output_fd = open(args[append_redir_index + 1], O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (output_fd == -1) {
            perror("open failed");
            return 0;
        }
        execute_command(args, in_background, STDIN_FILENO, output_fd);
        close(output_fd);
    } else {
        execute_command(args, in_background, STDIN_FILENO, STDOUT_FILENO);
    }

    return 1;
}

// Finalize function for cleanup
int finalize(void) {
    return 0;
}
