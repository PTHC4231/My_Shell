
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <glob.h>

#define MAX_LEN 1024
#define DELIM " \t\r\n\a"

// Function prototypes
int handle_cd(char **args);
int handle_pwd(char **args);
int handle_exit(char **args);
int handle_which(char **args);
char **split_line_and_expand_wildcards(char *line);
int execute_command(char **args);
int launch_process(char **args);
int last_exit_status = 0;

// List of built-in command names and corresponding functions
char *builtin_str[] = {"cd", "pwd", "exit", "which"};
int (*builtin_func[]) (char **) = {&handle_cd, &handle_pwd, &handle_exit, &handle_which};

int num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}

char *read_line(void) {
    char *buffer = malloc(MAX_LEN);
    int position = 0;
    int c;

    if (!buffer) {
        fprintf(stderr, "mysh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        if (read(STDIN_FILENO, &c, 1) == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        if (c == EOF || c == '\n') {
            buffer[position] = '\0';
            if (c == EOF && position == 0) {
                free(buffer);
                return NULL;
            }
            return buffer;
        } else {
            buffer[position] = c;
        }
        position++;

        if (position >= MAX_LEN) {
            fprintf(stderr, "mysh: command too long\n");
            exit(EXIT_FAILURE);
        }
    }
}

char **split_line_and_expand_wildcards(char *line) {
    int bufsize = 64, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;
    glob_t glob_result;

    if (!tokens) {
        fprintf(stderr, "allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, DELIM);
    while (token != NULL) {
        if (strchr(token, '*') != NULL) {
            glob(token, GLOB_NOCHECK | GLOB_TILDE, NULL, &glob_result);
            for (unsigned int i = 0; i < glob_result.gl_pathc; i++) {
                tokens[position++] = strdup(glob_result.gl_pathv[i]);
                if (position >= bufsize) {
                    bufsize += 64;
                    tokens = realloc(tokens, bufsize * sizeof(char*));
                    if (!tokens) {
                        fprintf(stderr, "allocation error\n");
                        exit(EXIT_FAILURE);
                    }
                }
            }
            globfree(&glob_result);
        } else {
            tokens[position++] = strdup(token);
            if (position >= bufsize) {
                bufsize += 64;
                tokens = realloc(tokens, bufsize * sizeof(char*));
                if (!tokens) {
                    fprintf(stderr, "allocation error\n");
                    exit(EXIT_FAILURE);
                }
            }
        }
        token = strtok(NULL, DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

int execute_builtin(char **args) {
    for (int i = 0; i < num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }
    return 0; // Not a built-in command
}

// Definitions of the built-in command functions
int handle_cd(char **args) { /* Implementation omitted for brevity */ }
int handle_pwd(char **args) { /* Implementation omitted for brevity */ }
int handle_exit(char **args) { /* Implementation omitted for brevity */ }
int handle_which(char **args) { /* Implementation omitted for brevity */ }

int launch_process(char **args) {
    // The implementation should include piping logic here
    // Please refer to the previously discussed approach for handling pipes
}

void main_loop(void) {
    char *line;
    char **args;
    int status;
    int interactive = isatty(STDIN_FILENO);

    do {
        if (interactive) {
            printf("> ");
        }
        line = read_line();
        if (line == NULL) {
            if (interactive) {
                printf("\n");
            }
            break; // Exit on EOF
        }
        args = split_line_and_expand_wildcards(line);
        status = execute_builtin(args);

        if (!status) {
            status = launch_process(args);
        }

        free(line);
        for (int i = 0; args[i] != NULL; i++) {
            free(args[i]); // Free each argument string
        }
        free(args);
    } while (status);
}

int launch_process(char **args) {
    int pipefd[2];
    int pipe_position = -1;
    pid_t pid1, pid2;
    int status;

    // Scanning for the pipe symbol and its position
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            pipe_position = i;
            break;
        }
    }

    if (pipe_position == -1) {
        // Handle commands without pipes
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            if (execvp(args[0], args) == -1) {
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        } else if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else {
            // Parent process
            do {
                waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
        return 1;
    }

    // Creating a pipe for commands with pipes
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid1 = fork();
    if (pid1 == 0) {
        // First child for the command before the pipe
        close(pipefd[0]); // Close unused read end
        dup2(pipefd[1], STDOUT_FILENO); // Connect stdout to pipe write
        close(pipefd[1]);
        if (execvp(args[0], args) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    }

    pid2 = fork();
    if (pid2 == 0) {
        // Second child for the command after the pipe
        close(pipefd[1]); // Close unused write end
        dup2(pipefd[0], STDIN_FILENO); // Connect stdin to pipe read
        close(pipefd[0]);
        if (execvp(args[pipe_position + 1], &args[pipe_position + 1]) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    }

    // Closing pipe fds in the parent
    close(pipefd[0]);
    close(pipefd[1]);

    // Wait for both child processes to finish
    waitpid(pid1, &status, 0);
    waitpid(pid2, &status, 0);

    return 1;
}

// Main function and other necessary built-in command implementations follow...

int main(int argc, char **argv) {
    if (isatty(STDIN_FILENO)) {
        printf("Entering interactive mode.\n");
    } else {
        printf("Entering batch mode.\n");
    }
    main_loop();
    return EXIT_SUCCESS;
}
