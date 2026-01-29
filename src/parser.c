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

/* local helpers */
static int   hsh_execute(char **args);
static int   hsh_execute_pipeline(char *line);
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

/* ----- public entry: run one line (may contain pipes or () operator) ----- */

int hsh_run_line(char *line) {
    if (!line)
        return 1;

    /* handle pipelines first, on a copy because strtok_r mutates */
    if (strchr(line, '|') != NULL) {
        char *pipe_copy = strdup(line);
        if (!pipe_copy) {
            perror("hsh: strdup");
            return 1;
        }
        int s = hsh_execute_pipeline(pipe_copy);
        free(pipe_copy);
        return s;      /* currently always 1 (keep shell running) */
    }

    /* tokenize the line */
    char *work = strdup(line);
    if (!work) {
        perror("hsh: strdup");
        return 1;
    }
    char **tokens = hsh_split_line_local(work);

    /* find () token */
    int split = -1;
    int end   = 0;
    for (int i = 0; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "()") == 0)
            split = i;
        end = i + 1;
    }

    if (split != -1) {
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

        int status_left = 1;
        if (left_buf[0] != '\0')
            status_left = hsh_run_line(left_buf);

        int status_right = 1;
        if (right_buf[0] != '\0')
            status_right = hsh_run_line(right_buf);

        /* if either side requested exit (0), propagate 0; otherwise keep running */
        if (status_left == 0 || status_right == 0)
            return 0;
        return 1;
    }

    /* no () operator: normal single-command execution */
    int status = hsh_execute(tokens);

    free(tokens);
    free(work);
    return status;
}

/* ----- single-command path (no pipes) ----- */

static int hsh_execute(char **args) {
    pid_t pid;
    int status = 1;

    if (args[0] == NULL)
        return 1;

    /* builtins */
    if (strcmp(args[0], "exit") == 0)
        return 0;  /* signal main loop to exit */

    if (strcmp(args[0], "cd") == 0)
        return hsh_builtin_cd(args);

    if (strcmp(args[0], "help") == 0)
        return hsh_builtin_help(args);

    if (strcmp(args[0], "config") == 0)
        return hsh_builtin_config(args);

    if (strcmp(args[0], "alias") == 0)
        return hsh_builtin_alias(args);

    if (strcmp(args[0], "sys") == 0)
        return hsh_builtin_sys(args);

    if (strcmp(args[0], "fs") == 0)
        return hsh_builtin_fs(args);

    if (strcmp(args[0], "net") == 0)
        return hsh_builtin_net(args);

    if (strcmp(args[0], "ps") == 0)
        return hsh_builtin_ps(args);

    /* external command */
    pid = fork();
    if (pid == 0) {
        execvp(args[0], args);
        perror("hsh");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("hsh: fork");
    } else {
        do {
            if (waitpid(pid, &status, 0) == -1) {
                perror("hsh: waitpid");
                break;
            }
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    /* ignore child exit code for loop control; keep shell running */
    if (WIFEXITED(status)) {
        (void)WEXITSTATUS(status);
        return 1;
    }
    return 1;
}

/* ----- pipeline path: cmd1 | cmd2 | ... ----- */

static int hsh_execute_pipeline(char *line) {
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

    if (seg_count == 0)
        return 1;

    int num_cmds = seg_count;
    int pipes[HSH_MAX_TOKENS][2];

    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("hsh: pipe");
            return 1;
        }
    }

    for (int i = 0; i < num_cmds; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("hsh: fork");
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

    int status;
    while (wait(&status) > 0)
        ;

    /* always keep shell running after a pipeline */
    return 1;
}
