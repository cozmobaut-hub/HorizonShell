#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "extras.h"
#include "parser.h"

#define HSH_MAX_TOKENS 64

/* forward declarations of main.c builtins */
int hsh_builtin_help(char **args);
int hsh_builtin_sys(char **args);
int hsh_builtin_fs(char **args);
int hsh_builtin_net(char **args);
int hsh_builtin_ps(char **args);
int hsh_builtin_config(char **args);
int hsh_builtin_alias(char **args);
int hsh_builtin_cd(char **args);
int hsh_builtin_lang(char **args);

/* local helpers */
static int   hsh_execute(char **args, int *cmd_status_out);
static int   hsh_execute_pipeline(char *line, int *cmd_status_out);
static char **hsh_split_line_local(char *line);

/* ----- simple tokenizer ----- */

static char **hsh_split_line_local(char *line) {
    int bufsize = HSH_MAX_TOKENS, position = 0;
    char **tokens = malloc(bufsize * sizeof(char *));
    char *token;

    if (!tokens) {
        perror("hsh: allocation error");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, " \t\r\n");
    while (token != NULL && position < bufsize - 1) {
        tokens[position++] = token;
        token = strtok(NULL, " \t\r\n");
    }
    tokens[position] = NULL;
    return tokens;
}

/* ----- public entry: run one line (may contain pipes or () / )( operator) ----- */

int hsh_run_line(char *line, int *last_status_out) {
    int dummy_status = 0;
    if (!last_status_out)
        last_status_out = &dummy_status;

    if (!line) {
        *last_status_out = 0;
        return 1;
    }

    /* handle pipelines first, on a copy because strtok_r mutates */
    if (strchr(line, '|') != NULL) {
        char *pipe_copy = strdup(line);
        if (!pipe_copy) {
            perror("hsh: strdup");
            *last_status_out = 1;
            return 1;
        }
        int s = hsh_execute_pipeline(pipe_copy, last_status_out);
        free(pipe_copy);
        return s;      /* 0 = exit shell, 1 = keep running */
    }

    /* tokenize the line */
    char *work = strdup(line);
    if (!work) {
        perror("hsh: strdup");
        *last_status_out = 1;
        return 1;
    }
    char **tokens = hsh_split_line_local(work);

    /* find () or )( token, but NOT for lang ... lines */
    int split = -1;
    int end   = 0;
    int is_lang = (tokens[0] && strcmp(tokens[0], "lang") == 0);
    enum { OP_NONE, OP_BOTH, OP_ON_ERROR } op_kind = OP_NONE;

    for (int i = 0; tokens[i] != NULL; i++) {
        if (!is_lang) {
            if (strcmp(tokens[i], "()") == 0) {
                split   = i;
                op_kind = OP_BOTH;
            } else if (strcmp(tokens[i], ")(") == 0) {
                split   = i;
                op_kind = OP_ON_ERROR;
            }
        }
        end = i + 1;
    }

    if (split != -1 && op_kind != OP_NONE) {
        char left_buf[1024]  = {0};
        char right_buf[1024] = {0};

        /* left: tokens[0..split-1] */
        for (int i = 0; i < split; i++) {
            if (i > 0)
                strncat(left_buf, " ", sizeof(left_buf) - strlen(left_buf) - 1);
            strncat(left_buf, tokens[i], sizeof(left_buf) - strlen(left_buf) - 1);
        }

        /* right: tokens[split+1..end-1] */
        for (int i = split + 1; i < end; i++) {
            if (i > split + 1)
                strncat(right_buf, " ", sizeof(right_buf) - strlen(right_buf) - 1);
            strncat(right_buf, tokens[i], sizeof(right_buf) - strlen(right_buf) - 1);
        }

        free(tokens);
        free(work);

        int status_left = 0;  /* command exit code */
        int status_right = 0;

        int shell_status_left  = 1;
        int shell_status_right = 1;

        if (left_buf[0] != '\0')
            shell_status_left = hsh_run_line(left_buf, &status_left);

        if (op_kind == OP_BOTH) {
            /* () : always run right */
            if (right_buf[0] != '\0')
                shell_status_right = hsh_run_line(right_buf, &status_right);
        } else if (op_kind == OP_ON_ERROR) {
            /* )( : only run right if left failed (non-zero exit code) */
            if (status_left != 0 && right_buf[0] != '\0')
                shell_status_right = hsh_run_line(right_buf, &status_right);
        }

        /* propagate last command's exit code */
        *last_status_out = (shell_status_right == 0 ? status_right : status_left);

        /* if either side requested shell exit (0), propagate 0; otherwise keep running */
        if (shell_status_left == 0 || shell_status_right == 0)
            return 0;
        return 1;
    }

    /* no ()/)( operator: normal single-command execution */
    int shell_status = hsh_execute(tokens, last_status_out);

    free(tokens);
    free(work);
    return shell_status;
}

/* ----- single-command path (no pipes) ----- */

static int hsh_execute(char **args, int *cmd_status_out) {
    pid_t pid;
    int status = 0;

    if (!cmd_status_out)
        cmd_status_out = &status;

    if (args[0] == NULL) {
        *cmd_status_out = 0;
        return 1;
    }

    /* builtins */
    if (strcmp(args[0], "exit") == 0)
        return 0;  /* signal main loop to exit */

    if (strcmp(args[0], "cd") == 0) {
        *cmd_status_out = hsh_builtin_cd(args);
        return 1;
    }

    if (strcmp(args[0], "help") == 0) {
        *cmd_status_out = hsh_builtin_help(args);
        return 1;
    }

    if (strcmp(args[0], "config") == 0) {
        *cmd_status_out = hsh_builtin_config(args);
        return 1;
    }

    if (strcmp(args[0], "alias") == 0) {
        *cmd_status_out = hsh_builtin_alias(args);
        return 1;
    }

    if (strcmp(args[0], "sys") == 0) {
        *cmd_status_out = hsh_builtin_sys(args);
        return 1;
    }

    if (strcmp(args[0], "fs") == 0) {
        *cmd_status_out = hsh_builtin_fs(args);
        return 1;
    }

    if (strcmp(args[0], "net") == 0) {
        *cmd_status_out = hsh_builtin_net(args);
        return 1;
    }

    if (strcmp(args[0], "ps") == 0) {
        *cmd_status_out = hsh_builtin_ps(args);
        return 1;
    }

    if (strcmp(args[0], "lang") == 0) {
        *cmd_status_out = hsh_builtin_lang(args);
        return 1;
    }

    /* external command */
    pid = fork();
    if (pid == 0) {
        execvp(args[0], args);
        perror("hsh");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("hsh: fork");
        *cmd_status_out = 1;
    } else {
        do {
            if (waitpid(pid, &status, 0) == -1) {
                perror("hsh: waitpid");
                status = 1;
                break;
            }
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));

        if (WIFEXITED(status))
            *cmd_status_out = WEXITSTATUS(status);
        else
            *cmd_status_out = 1;
    }

    return 1;  /* keep shell running */
}

/* ----- pipeline path: cmd1 | cmd2 | ... ----- */

static int hsh_execute_pipeline(char *line, int *cmd_status_out) {
    char *segments[HSH_MAX_TOKENS];
    int seg_count = 0;

    char *saveptr;
    char *seg = strtok_r(line, "|", &saveptr);
    while (seg && seg_count < HSH_MAX_TOKENS - 1) {
        while (*seg == ' ' || *seg == '\t') seg++;
        char *end = seg + strlen(seg) - 1;
        while (end >= seg && (*end == ' ' || *end == '\t' || *end == '\n')) {
            *end = '\0';
            end--;
        }
        if (*seg != '\0') {
            segments[seg_count++] = seg;
        }
        seg = strtok_r(NULL, "|", &saveptr);
    }
    segments[seg_count] = NULL;

    if (seg_count == 0) {
        if (cmd_status_out) *cmd_status_out = 0;
        return 1;
    }

    int num_cmds = seg_count;
    int pipes[HSH_MAX_TOKENS][2];

    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("hsh: pipe");
            if (cmd_status_out) *cmd_status_out = 1;
            return 1;
        }
    }

    for (int i = 0; i < num_cmds; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("hsh: fork");
            if (cmd_status_out) *cmd_status_out = 1;
            return 1;
        }

        if (pid == 0) {
            if (i > 0) {
                if (dup2(pipes[i-1][0], STDIN_FILENO) < 0) {
                    perror("hsh: dup2 in");
                    exit(EXIT_FAILURE);
                }
            }
            if (i < num_cmds - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) < 0) {
                    perror("hsh: dup2 out");
                    exit(EXIT_FAILURE);
                }
            }

            for (int j = 0; j < num_cmds - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            char *cmdline = segments[i];
            char *argv_buf = strdup(cmdline);
            if (!argv_buf) {
                perror("hsh: strdup");
                exit(EXIT_FAILURE);
            }

            char *argv_tokens[HSH_MAX_TOKENS];
            int argc = 0;
            char *tok = strtok(argv_buf, " \t\r\n");
            while (tok && argc < HSH_MAX_TOKENS - 1) {
                argv_tokens[argc++] = tok;
                tok = strtok(NULL, " \t\r\n");
            }
            argv_tokens[argc] = NULL;

            if (argv_tokens[0] == NULL) {
                free(argv_buf);
                exit(EXIT_SUCCESS);
            }

            execvp(argv_tokens[0], argv_tokens);
            perror("hsh");
            free(argv_buf);
            exit(EXIT_FAILURE);
        }
        /* parent continues loop */
    }

    for (int i = 0; i < num_cmds - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    int status = 0;
    int last_status = 0;
    while (wait(&status) > 0) {
        if (WIFEXITED(status))
            last_status = WEXITSTATUS(status);
        else
            last_status = 1;
    }

    if (cmd_status_out) *cmd_status_out = last_status;
    return 1;
}
