#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <glob.h>

#define MAX_LEN 1024
#define DELIM " \t\r\n\a"

int handle_cd(char **args);
int handle_pwd(char **args);
int handle_exit(char **args);
int handle_which(char **args);

char *builtin_str[] = {
    "cd",
    "pwd",
    "exit",
    "which"
};

int (*builtin_func[]) (char **) = {
    &handle_cd,
    &handle_pwd,
    &handle_exit,
    &handle_which
};

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
        // Read a character
        if (read(STDIN_FILENO, &c, 1) == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        // If we hit EOF, replace it with a null character and return.
        if (c == EOF || c == '\n') {
            buffer[position] = '\0';
            if (c == EOF && position == 0) { // Immediate EOF
                free(buffer);
                return NULL;
            }
            return buffer;
        } else {
            buffer[position] = c;
        }
        position++;

        // If we have exceeded the buffer, reallocate.
        if (position >= MAX_LEN) {
            fprintf(stderr, "mysh: command too long\n");
            exit(EXIT_FAILURE);
        }
    }
}

char **split_line(char *line) {
    int bufsize = 64, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) {
        fprintf(stderr, "allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, DELIM);
    while (token != NULL) {
        tokens[position++] = token;
        if (position >= bufsize) {
            bufsize += 64;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL, DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

int launch_process(char **args) {
    int pipefd[2];
    int pipe_position = -1;
    pid_t pid1, pid2;
    int status;

    // Detecting the pipe symbol and its position
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            pipe_position = i;
            break;
        }
    }

    if (pipe_position == -1) {
        // Handle commands without pipes as before
        return execute_command(args);
    }

    // Commands with pipes
    args[pipe_position] = NULL; // Split the args array into two at the pipe symbol

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
    } else if (pid1 < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
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
    } else if (pid2 < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    // Parent process
    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, &status, 0); // Wait for the first command to finish
    waitpid(pid2, &status, 0); // Wait for the second command to finish

    return 1;
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
        args = split_line(line);
        status = execute_builtin(args);
        if (!status) {
            status = launch_process(args);
        }

        free(line);
        free(args);
    } while (status);
}

int main(int argc, char **argv) {
    if (isatty(STDIN_FILENO)) {
        printf("Entering interactive mode.\n");
    } else {
        // No message for batch mode to avoid unnecessary output
    }
    main_loop();
    return EXIT_SUCCESS;
}
/*
int launch_process(char **args) {
    pid_t pid;
    int status, in_fd, out_fd;
    int redirection_in = -1, redirection_out = -1;

    // Scan for input/output redirection symbols
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            redirection_in = i;
            break; // Only consider the first occurrence
        }
    }

    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            redirection_out = i;
            break; // Only consider the first occurrence
        }
    }

    pid = fork();
    if (pid == 0) { // Child process
        if (redirection_in != -1) {
            in_fd = open(args[redirection_in + 1], O_RDONLY);
            if (in_fd < 0) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
            args[redirection_in] = NULL; // Prevent execvp from seeing the redirection
        }
        if (redirection_out != -1) {
            out_fd = open(args[redirection_out + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out_fd < 0) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
            args[redirection_out] = NULL; // Prevent execvp from seeing the redirection
        }
        if (execvp(args[0], args) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) { // Error forking
        perror("fork");
    } else { //
        // Parent process
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 1;
}

int execute_builtin(char **args) {
    for (int i = 0; i < num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }
    return 0; // Not a built-in command
}

// Built-in command function implementations
int handle_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("cd");
        }
    }
    return 1;
}

int handle_pwd(char **args) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("pwd");
    }
    return 1;
}

int handle_exit(char **args) {
    exit(EXIT_SUCCESS);
}

int handle_which(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "expected argument to \"which\"\n");
    } else {
        char *path = getenv("PATH");
        if (path == NULL) {
            fprintf(stderr, "PATH not set.\n");
            return 1;
        }

        char *path_copy = strdup(path);
        char *dir = strtok(path_copy, ":");
        while (dir != NULL) {
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir, args[1]);
            if (access(full_path, F_OK) == 0) {
                printf("%s\n", full_path);
                free(path_copy);
                return 1;
            }
            dir = strtok(NULL, ":");
        }
        fprintf(stderr, "%s not found in PATH\n", args[1]);
        free(path_copy);
    }
    return 1;
}
*/