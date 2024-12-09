#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGS 64
#define MAX_ENV_VARS 128
// Structure for storing environment variables
typedef struct {
    char *name;
    char *value;
} EnvVar;
// Global variables
EnvVar env_vars[MAX_ENV_VARS];
int env_var_count = 0;
char cwd[MAX_COMMAND_LENGTH];
// Function to add a new environment variable
void set_env_var(char *name, char *value) {
    for (int i = 0; i < env_var_count; i++) {
        if (strcmp(env_vars[i].name, name) == 0) {
            free(env_vars[i].value);
            env_vars[i].value = strdup(value);
            return;
        }
    }
    if (env_var_count < MAX_ENV_VARS) {
        env_vars[env_var_count].name = strdup(name);
        env_vars[env_var_count].value = strdup(value);
        env_var_count++;
    } else {
        fprintf(stderr, "Error: Too many environment variables.\n");
    }
}
// Function to remove an environment variable
void unset_env_var(char *name) {
    for (int i = 0; i < env_var_count; i++) {
        if (strcmp(env_vars[i].name, name) == 0) {
            free(env_vars[i].name);
            free(env_vars[i].value);
            // Shift remaining variables
            for (int j = i; j < env_var_count - 1; j++) {
                env_vars[j] = env_vars[j + 1];
            }
            env_var_count--;
            return;
        }
    }
    fprintf(stderr, "Error: Environment variable not found: %s\n", name);
}
// Function to get the value of an environment variable
char *get_env_var(char *name) {
    for (int i = 0; i < env_var_count; i++) {
        if (strcmp(env_vars[i].name, name) == 0) {
            return env_vars[i].value;
        }
    }
    return NULL;
}
// Function to parse a command line into arguments
void parse_command(char *command, char **args) {
    char *token = strtok(command, " ");
    int i = 0;
    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;
}
// Function to execute a command
int execute_command(char **args) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process: Execute the command
        execvp(args[0], args);
        perror("execvp");
        exit(1);
    } else if (pid > 0) {
        // Parent process: Wait for child to finish
        wait(NULL);
        return 0;
    } else {
        perror("fork");
        return 1;
    }
}
// Function to replace environment variables in a command
void replace_env_vars(char *command) {
    char *p = command;
    while (*p) {
        if (*p == '$' && *(p + 1) == '{') {
            // Found environment variable
            char *var_name = p + 2;
            char *end_name = strchr(var_name, '}');
            if (end_name) {
                *end_name = '\0';
                char *value = get_env_var(var_name);
                if (value) {
                    // Replace the environment variable
                    int len = strlen(value);
                    memmove(p, value, len);
                    p += len;
                    *p = '\0';
                    p += 2; // Skip '}'
                } else {
                    // Environment variable not found
                    p += strlen(var_name) + 2; // Skip '}'
                }
            } else {
                // Invalid environment variable syntax
                p++;
            }
        } else {
            p++;
        }
    }
}
// Function to handle piping
int pipe_commands(char **args1, char **args2) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }
    pid_t pid1 = fork();
    if (pid1 == 0) {
        // Child process 1: Write output to pipe
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execvp(args1[0], args1);
        perror("execvp");
        exit(1);
    } else if (pid1 > 0) {
        // Parent process: Read input from pipe
        pid_t pid2 = fork();
        if (pid2 == 0) {
            // Child process 2: Read input from pipe
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
            execvp(args2[0], args2);
            perror("execvp");
            exit(1);
        } else if (pid2 > 0) {
            // Parent process: Wait for both children
            close(pipefd[0]);
            close(pipefd[1]);
            wait(NULL);
            wait(NULL);
            return 0;
        } else {
            perror("fork");
            return 1;
        }
    } else {
        perror("fork");
        return 1;
    }
}
// Function to handle redirects
int handle_redirect(char **args, char *redirect_type) {
    if (strcmp(redirect_type, ">") == 0) {
        // Redirect output to file
        FILE *fp = fopen(args[strlen(args) - 1], "w");
        if (!fp) {
            perror("fopen");
            return 1;
        }
        // Remove the filename from args
        args[strlen(args) - 1] = NULL;
        pid_t pid = fork();
        if (pid == 0) {
            // Child process: Redirect output to file
            dup2(fileno(fp), STDOUT_FILENO);
            fclose(fp);
            execvp(args[0], args);
            perror("execvp");
            exit(1);
        } else if (pid > 0) {
            // Parent process: Wait for child
            wait(NULL);
            return 0;
        } else {
            perror("fork");
            return 1;
        }
    } else if (strcmp(redirect_type, "<") == 0) {
        // Redirect input from file
        FILE *fp = fopen(args[strlen(args) - 1], "r");
        if (!fp) {
            perror("fopen");
            return 1;
        }
        // Remove the filename from args
        args[strlen(args) - 1] = NULL;
        pid_t pid = fork();
        if (pid == 0) {
            // Child process: Redirect input from file
            dup2(fileno(fp), STDIN_FILENO);
            fclose(fp);
            execvp(args[0], args);
            perror("execvp");
            exit(1);
        } else if (pid > 0) {
            // Parent process: Wait for child
            wait(NULL);
            return 0;
        } else {
            perror("fork");
            return 1;
        }
    }
    return 0;
}
// Function to handle background execution
int run_in_background(char **args) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process: Execute the command
        execvp(args[0], args);
        perror("execvp");
        exit(1);
    } else if (pid > 0) {
        // Parent process: Print message and continue
        printf("Started %s in the background.\n", args[0]);
        return 0;
    } else {
        perror("fork");
        return 1;
    }
}
int main() {
    getcwd(cwd, sizeof(cwd));
    // Initialize environment variables
    char *path = getenv("PATH");
    if (path) {
        set_env_var("PATH", path);
    }
    // Main loop
    while (1) {
        char command[MAX_COMMAND_LENGTH];
        printf("xsh%s$ ", cwd);
        fgets(command, MAX_COMMAND_LENGTH, stdin);
        command[strcspn(command, "\n")] = '\0';
        // Check for exit commands
        if (strcmp(command, "quit") == 0 || strcmp(command, "exit") == 0) {
            break;
        }
        // Parse the command line
        char *args[MAX_ARGS];
        parse_command(command, args);
        // Handle built-in commands
        if (strcmp(args[0], "cd") == 0) {
            if (args[1]) {
                if (chdir(args[1]) == -1) {
                    perror("chdir");
                } else {
                    getcwd(cwd, sizeof(cwd));
                }
            } else {
                fprintf(stderr, "Usage: cd <directory>\n");
            }
        } else if (strcmp(args[0], "pwd") == 0) {
            printf("%s\n", cwd);
        } else if (strcmp(args[0], "set") == 0) {
            if (args[1] && args[2]) {
                set_env_var(args[1], args[2]);
            } else {
                fprintf(stderr, "Usage: set <variable> <value>\n");
            }
        } else if (strcmp(args[0], "unset") == 0) {
            if (args[1]) {
                unset_env_var(args[1]);
            } else {
                fprintf(stderr, "Usage: unset <variable>\n");
            }
        } else {
            // Replace environment variables
            replace_env_vars(command);
            parse_command(command, args); // Reparse after replacement
            // Handle piping and redirects
            char *args1[MAX_ARGS];
            char *args2[MAX_ARGS];
            char *redirect_type = NULL;
            if (strchr(command, '|')) {
                // Pipe command
                char *pipe_command = strchr(command, '|') + 1;
                *strchr(command, '|') = '\0';
                parse_command(command, args1);
                parse_command(pipe_command, args2);
                if (pipe_commands(args1, args2) != 0) {
                    continue;
                }
            } else if (strchr(command, '>') || strchr(command, '<')) {
                // Redirect command
                if (strchr(command, '>')) {
                    redirect_type = ">";
                } else {
                    redirect_type = "<";
                }
                if (handle_redirect(args, redirect_type) != 0) {
                    continue;
                }
            } else if (command[strlen(command) - 1] == '&') {
                // Background execution
                command[strlen(command) - 1] = '\0';
                parse_command(command, args);
                if (run_in_background(args) != 0) {
                    continue;
                }
            } else {
                // Execute command directly
                if (execute_command(args) != 0) {
                    continue;
                }
            }
        }
    }
    // Clean up environment variables
    for (int i = 0; i < env_var_count; i++) {
        free(env_vars[i].name);
        free(env_vars[i].value);
    }
    return 0;
}
