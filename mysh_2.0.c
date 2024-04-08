#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <glob.h>
#include <stdbool.h>

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

// Improved read_line function to handle input from a file descriptor
// char *read_line_fd(int fd) {
//     char *line = NULL;
//     size_t bufsize = 0;  // getline will allocate a buffer.
//     getline(&line, &bufsize, fdopen(fd, "r"));
//     return line;
// }

char *read_line_fd(int fd) {
    char *line = NULL;
    size_t bufsize = 0;  // getline will allocate a buffer.
    ssize_t read_size = getline(&line, &bufsize, fdopen(fd, "r"));
    if (read_size == -1) {
        free(line);  // Free the allocated buffer
        return NULL; // Return NULL to indicate end-of-file or error
    }
    return line;
}


char **split_line_and_expand_wildcards(char *line) {
    int bufsize = 64, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) {
        fprintf(stderr, "allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, DELIM);
    while (token != NULL) {
        if (strcmp(token, "<") == 0 || strcmp(token, ">") == 0) {
            // Handle redirection
            tokens[position++] = strdup(token);  // Add the redirection token
            token = strtok(NULL, DELIM);  // Skip the redirection file name
            if (token == NULL) {
                fprintf(stderr, "Syntax error: Missing file name after redirection\n");
                exit(EXIT_FAILURE);
            }
        } else {
            // Expand wildcards if needed
            glob_t glob_result;
            if (strchr(token, '*') != NULL) {
                glob(token, GLOB_NOCHECK | GLOB_TILDE, NULL, &glob_result);
                for (unsigned int i = 0; i < glob_result.gl_pathc; ++i) {
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
            }
        }

        token = strtok(NULL, DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

// char **split_line_and_expand_wildcards(char *line) {
//     int bufsize = 64, position = 0;
//     char **tokens = malloc(bufsize * sizeof(char*));
//     char *token;
//     glob_t glob_result;

//     if (!tokens) {
//         fprintf(stderr, "allocation error\n");
//         exit(EXIT_FAILURE);
//     }

//     token = strtok(line, DELIM);
//     while (token != NULL) {
//         if (strchr(token, '*') != NULL) {
//             glob(token, GLOB_NOCHECK | GLOB_TILDE, NULL, &glob_result);
//             for (unsigned int i = 0; i < glob_result.gl_pathc; i++) {
//                 tokens[position++] = strdup(glob_result.gl_pathv[i]);
//                 if (position >= bufsize) {
//                     bufsize += 64;
//                     tokens = realloc(tokens, bufsize * sizeof(char*));
//                     if (!tokens) {
//                         fprintf(stderr, "allocation error\n");
//                         exit(EXIT_FAILURE);
//                     }
//                 }
//             }
//             globfree(&glob_result);
//         } else {
//             tokens[position++] = strdup(token);
//             if (position >= bufsize) {
//                 bufsize += 64;
//                 tokens = realloc(tokens, bufsize * sizeof(char*));
//                 if (!tokens) {
//                     fprintf(stderr, "allocation error\n");
//                     exit(EXIT_FAILURE);
//                 }
//             }
//         }
//         token = strtok(NULL, DELIM);
//     }
//     tokens[position] = NULL;
//     return tokens;
// }

int execute_builtin(char **args) {
    for (int i = 0; i < num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }
    return 0; // Not a built-in command
}

// Example of handling commands, including built-ins like "cd"
int execute_command(char **args) {
    if (args[0] == NULL) {
        // An empty command was entered.
        return 1;
    }
    // Check if the command is a built-in command.
    for (int i = 0; i < num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            // The command is a built-in command. Execute it.
            return (*builtin_func[i])(args);
        }
    }
    // Not a built-in command. Attempt to execute it as an external command.
    return launch_process(args);
}

// Assuming launch_process is your function to handle external commands using execvp.


// Definitions of the built-in command functions
//int handle_cd(char **args) { /* Implementation omitted for brevity */ }
//int handle_pwd(char **args) { /* Implementation omitted for brevity */ }
//int handle_exit(char **args) { /* Implementation omitted for brevity */ }
//int handle_which(char **args) { /* Implementation omitted for brevity */ }

//int launch_process(char **args) {
    // The implementation should include piping logic here
    // Please refer to the previously discussed approach for handling pipes
//}

int handle_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "Expected argument to \"cd\"\n");
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
        return 0; // Indicate success
    } else {
        perror("pwd");
        return 1; // Indicate failure
    }
}

int handle_exit(char **args) {
    // Optional: Handle any arguments to exit here
    if (isatty(STDIN_FILENO)) {
        printf("mysh: Exiting my shell\n");
    }
    exit(0); // Exit the shell
}

int handle_which(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "which: missing argument\n");
        return 1; // Indicate failure
    }
    const char* paths[] = {"/usr/local/bin", "/usr/bin", "/bin"};
    char executable_path[1024];
    for (int i = 0; i < sizeof(paths)/sizeof(paths[0]); i++) {
        snprintf(executable_path, sizeof(executable_path), "%s/%s", paths[i], args[1]);
        if (access(executable_path, X_OK) == 0) {
            printf("%s\n", executable_path);
            return 0; // Indicate success
        }
    }
    fprintf(stderr, "which: no %s in (%s)\n", args[1], getenv("PATH"));
    return 1; // Indicate failure
}




// Rename or ensure you're using read_line_fd in the main_loop
void main_loop(void) {
    char *line;
    char **args;
    int interactive = isatty(STDIN_FILENO);

    if (interactive) {
        printf("Welcome to my shell!\n");
    }

    do {
        if (interactive) {
            printf("mysh> ");
        }
        // Change here: Use file descriptor STDIN_FILENO with read_line_fd
        line = read_line_fd(STDIN_FILENO); 
        if (line == NULL) { // Handle EOF
            break;
        }

        args = split_line_and_expand_wildcards(line);
        int shouldExecute = 1; // Flag to determine command execution based on conditionals

        if ((strcmp(args[0], "then") == 0 && last_exit_status != 0) ||
            (strcmp(args[0], "else") == 0 && last_exit_status == 0)) {
            shouldExecute = 0; // Do not execute the command based on last_exit_status
        }

        char **commandToExecute = args;
        if (strcmp(args[0], "then") == 0 || strcmp(args[0], "else") == 0) {
            commandToExecute += 1; // Skip 'then'/'else'
        }

        if (shouldExecute) {
            if (!execute_builtin(commandToExecute)) {
                printf("main Loop");
                launch_process(commandToExecute);
            }
        }

        free(line);
        free(args);
    } while (1);

    if (interactive) {
        printf("\nExiting my shell.\n");
    }
}

// int launch_process(char **args) {
//     int pipefd[2];
//     int pipe_position = -1;
//     pid_t pid1, pid2;
//     int status;

//     // Find if there's a pipeline
//     for (int i = 0; args[i] != NULL; i++) {
//         if (strcmp(args[i], "|") == 0) {
//             pipe_position = i;
//             break;
//         }
//     }
int launch_process(char **args) {
    int input_fd = STDIN_FILENO; // Default to standard input
    int output_fd = STDOUT_FILENO; // Default to standard output
    int pipe_position = -1;
    pid_t pid1, pid2;
    int status;

    // Find if there's a pipeline
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            pipe_position = i;
            break;
        }
    }

    // Find if there's input or output redirection
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            // Input redirection found
            input_fd = open(args[i + 1], O_RDONLY);
            if (input_fd < 0) {
                perror("Input redirection");
                return EXIT_FAILURE;
            }
            // Remove redirection tokens and file name from args
            args[i] = NULL;
            args[i + 1] = NULL;
        } else if (strcmp(args[i], ">") == 0) {
            // Output redirection found
            output_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            if (output_fd < 0) {
                perror("Output redirection");
                return EXIT_FAILURE;
            }
            // Remove redirection tokens and file name from args
            args[i] = NULL;
            args[i + 1] = NULL;
        }
    }

    // Handle pipelines
    if (pipe_position == -1) { // Handle commands without pipes
        pid1 = fork();
        if (pid1 == 0) { // Child process
            // Redirect input if necessary
            if (input_fd != STDIN_FILENO) {
                dup2(input_fd, STDIN_FILENO);
                close(input_fd);
            }
            // Redirect output if necessary
            if (output_fd != STDOUT_FILENO) {
                dup2(output_fd, STDOUT_FILENO);
                close(output_fd);
            }
            execvp(args[0], args);
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else { // Handling pipelines
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
        pid1 = fork();
        if (pid1 == 0) { // First child for the command before the pipe
            close(pipefd[0]); 
            dup2(pipefd[1], STDOUT_FILENO); 
            close(pipefd[1]);
            execvp(args[0], args);
            perror("execvp");
            exit(EXIT_FAILURE);
        }

        pid2 = fork();
        if (pid2 == 0) { // Second child for the command after the pipe
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
            execvp(args[pipe_position + 1], &args[pipe_position + 1]);
            perror("execvp");
            exit(EXIT_FAILURE);
        }

        close(pipefd[0]);
        close(pipefd[1]);
    }

    // Close file descriptors if they were opened
    if (input_fd != STDIN_FILENO) close(input_fd);
    if (output_fd != STDOUT_FILENO) close(output_fd);

    // Wait for child processes to finish and update last_exit_status
    if (pipe_position == -1) { // Single command
        //printf("waiting");
        waitpid(pid1, &status, 0);
    } else { // Pipeline
        waitpid(pid1, NULL, 0); // Wait for the first command; its status is not used for conditionals
        waitpid(pid2, &status, 0); // Wait for the second command; its status determines the outcome
    }

    if (WIFEXITED(status)) {
        last_exit_status = WEXITSTATUS(status);
    } else {
        last_exit_status = 1; // Non-zero to indicate failure if not exited normally
    }

    return last_exit_status == 0 ? 1 : 0;
}

// Updated main function for improved batch mode handling
// int main(int argc, char **argv) {
//     int fd = STDIN_FILENO;  // Default to standard input
//     bool batchMode = false;

//     if (argc > 1) {
//         // Attempt to open the script file
//         fd = open(argv[1], O_RDONLY);
//         if (fd < 0) {
//             perror("Error opening script file");
//             return EXIT_FAILURE;
//         }
//         batchMode = true;
//     }

// //    main_loop();

//     char *line;
//     char **args;
//     int status = 0;

//     if (!batchMode){
//         printf("Welcome to my shell!\n");
//     }

//     // Main loop to process each command
//     while (true) {
//         if (!batchMode) printf("mysh> ");
//         line = read_line_fd(fd);
//         if (line == NULL) break;  // End of file

//         if (strcmp(line, "\n") == 0) {
//             free(line);
//             continue;  // Skip empty lines
//         }


//         args = split_line_and_expand_wildcards(line);
//         if (args[0] != NULL) {  // Skip empty commands
//             status = execute_builtin(args);
//             if (!status) {
//                 status = launch_process(args);
//             }
//         }

//         free(line);
//         free(args);
//     }

//     if (!batchMode) {
//         // Print exit message if in interactive mode
//         printf("\nExiting my shell.\n");
//     }

//     if (batchMode) close(fd);  // Close the script file if in batch mode
//     return 0;
// }

int main(int argc, char **argv) {
    int fd = STDIN_FILENO;  // Default to standard input
    bool batchMode = false;

    if (argc > 1) {
        // Attempt to open the script file
        fd = open(argv[1], O_RDONLY);
        if (fd < 0) {
            perror("Error opening script file");
            return EXIT_FAILURE;
        }
        batchMode = true;
    }

    char *line;
    char **args;
    int status = 0;

    if (!batchMode) {
        printf("Welcome to my shell!\n");
    }

    // Main loop to process each command
    while (true) {
        if (!batchMode) printf("mysh> ");
        line = read_line_fd(fd);
        if (!line) break;  // End of file

        args = split_line_and_expand_wildcards(line);
        if (args[0] != NULL) {  // Skip empty commands
            status = execute_builtin(args);
            if (!status) {
                status = launch_process(args);
            }
        }

        free(line);
        free(args);
    }

    // Print exit message if in interactive mode
    if (!batchMode) {
        printf("\nExiting my shell.\n");
    }

    if (batchMode) close(fd);  // Close the script file if in batch mode
    return 0;
}
