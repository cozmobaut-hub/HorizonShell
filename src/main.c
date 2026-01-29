/*
 * HorizonShell - simple hybrid shell
 * Copyright (c) 2026
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "extras.h"
#include "parser.h"


#define HSH_NAME    "HorizonShell"
#define HSH_VERSION "0.1.0"

#define HSH_MAX_LINE   1024

static volatile sig_atomic_t hsh_got_sigint = 0;

static void hsh_sigint_handler(int sig) {
    (void)sig;
    hsh_got_sigint = 1;
}

static void hsh_loop(const struct hsh_config *cfg,
                     struct hsh_alias *aliases, int alias_count);
static char *hsh_read_line(void);
static int  hsh_run_script(FILE *f, const struct hsh_config *cfg,
                           struct hsh_alias *aliases, int alias_count);


/* builtin handlers (used by parser.c via extern prototypes there) */
int hsh_builtin_help(char **args);
int hsh_builtin_sys(char **args);
int hsh_builtin_fs(char **args);
int hsh_builtin_net(char **args);
int hsh_builtin_ps(char **args);
int hsh_builtin_config(char **args);
int hsh_builtin_alias(char **args);
int hsh_builtin_cd(char **args);


int main(int argc, char **argv) {
    char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "hsh: HOME not set\n");
        return 1;
    }

    char confpath[512];
    char aliaspath[512];
    struct stat st;
    struct hsh_config cfg;
    struct hsh_alias *aliases = NULL;
    int alias_count = 0;

    snprintf(confpath, sizeof(confpath), "%s/.config/hsh/config", home);
    snprintf(aliaspath, sizeof(aliaspath), "%s/.config/hsh/aliases", home);

    /* If config missing, run setup */
    if (stat(confpath, &st) != 0) {
        printf("hsh: first run, launching setup...\n");
        int rc = system("hsh-setup");
        if (rc == -1) {
            perror("hsh: failed to run hsh-setup");
            return 1;
        }
        if (rc != 0) {
            fprintf(stderr, "hsh: setup failed (rc=%d)\n", rc);
            return 1;
        }
        if (stat(confpath, &st) != 0) {
            fprintf(stderr, "hsh: config still missing after setup\n");
            return 1;
        }
    }

    if (hsh_load_config(confpath, &cfg) != 0) {
        return 1;
    }

    /* load aliases (no error if file missing) */
    if (hsh_load_aliases(aliaspath, &aliases, &alias_count) != 0) {
        fprintf(stderr, "hsh: failed to load aliases\n");
    }

    /* script mode: hsh myscript.hsh */
    if (argc > 1) {
        FILE *f = fopen(argv[1], "r");
        if (!f) {
            perror("hsh: fopen script");
            hsh_free_aliases(aliases, alias_count);
            return 1;
        }
        int rc = hsh_run_script(f, &cfg, aliases, alias_count);
        fclose(f);
        hsh_free_aliases(aliases, alias_count);
        return rc;
    }

    /* install Ctrl-C handler for interactive mode */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = hsh_sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);

    /* interactive mode */
    hsh_loop(&cfg, aliases, alias_count);

    hsh_free_aliases(aliases, alias_count);
    return 0;
}


static void hsh_loop(const struct hsh_config *cfg,
                     struct hsh_alias *aliases, int alias_count) {
    char *line;
    int status;

    do {
        if (hsh_got_sigint) {
            hsh_got_sigint = 0;
            write(STDOUT_FILENO, "\n", 1);
        }

        /* draw status bar and prompt */
        hsh_draw_statusbar(cfg);
        printf("\033[%d;%dmhsh$ \033[0m", cfg->fg, cfg->bg);
        fflush(stdout);

        line = hsh_read_line();
        if (!line)
            break;

        /* alias expansion on first word only */
        char *expanded = NULL;
        {
            char *tmp = strdup(line);
            if (!tmp) {
                perror("hsh: strdup");
                free(line);
                continue;
            }
            char *first = strtok(tmp, " \t\r\n");
            if (first) {
                expanded = hsh_expand_alias(aliases, alias_count, first);
            }
            free(tmp);
        }

        if (expanded) {
            free(line);
            line = expanded;
        }

        status = hsh_run_line(line);

        free(line);
    } while (status);

    printf("\n");
}


/* script mode: run each non-comment line through hsh_run_line */
static int hsh_run_script(FILE *f, const struct hsh_config *cfg,
                          struct hsh_alias *aliases, int alias_count) {
    char *line = NULL;
    size_t sz = 0;
    int status = 1;

    while (getline(&line, &sz, f) != -1) {
        /* skip comments and blank lines */
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0')
            continue;

        /* no readline, but we still want aliases */
        char *expanded = NULL;
        {
            char *tmp = strdup(line);
            if (!tmp) {
                perror("hsh: strdup");
                free(line);
                return 1;
            }
            char *first = strtok(tmp, " \t\r\n");
            if (first) {
                expanded = hsh_expand_alias(aliases, alias_count, first);
            }
            free(tmp);
        }

        char *exec_line = line;
        if (expanded) {
            exec_line = expanded;
        }

        status = hsh_run_line(exec_line);

        if (expanded)
            free(expanded);

        if (status == 0)  /* exit in script */
            break;
    }

    free(line);
    (void)cfg; /* cfg is unused here today, but kept for future script features */
    return 0;
}


/* use GNU readline for line input + history */
static char *hsh_read_line(void) {
    /* we already printed the prompt; give readline an empty one */
    char *line = readline("");
    if (!line)
        return NULL;

    if (line[0] != '\0')
        add_history(line);

    return line;  /* caller will free(line) */
}

/* ====== BUILTINS (used by parser.c) ====== */

int hsh_builtin_cd(char **args) {
    char *target = NULL;

    if (args[1] == NULL || strcmp(args[1], "~") == 0) {
        /* cd or cd ~ -> HOME */
        target = getenv("HOME");
        if (!target) {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
    } else if (args[1][0] == '~') {
        /* handle things like ~/foo */
        char *home = getenv("HOME");
        if (!home) {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
        size_t len = strlen(home) + strlen(args[1]);
        char *buf = malloc(len);
        if (!buf) {
            perror("cd");
            return 1;
        }
        snprintf(buf, len, "%s%s", home, args[1] + 1);
        int rc = chdir(buf);
        free(buf);
        if (rc != 0) {
            perror("cd");
            return 1;
        }
        return 1;
    } else if (args[1][0] == '$') {
        /* cd $VAR */
        const char *var = args[1] + 1;
        target = getenv(var);
        if (!target || target[0] == '\0') {
            fprintf(stderr, "cd: %s not set or empty\n", args[1]);
            return 1;
        }
    } else {
        target = args[1];
    }

    if (chdir(target) != 0) {
        perror("cd");
        return 1;
    }

    return 1;
}

int hsh_builtin_help(char **args) {
    if (args[1] == NULL) {
        printf("%s %s\n", HSH_NAME, HSH_VERSION);
        printf("A hybrid interactive shell with system-aware status bar and extended commands.\n\n");

        printf("Builtins:\n");
        printf("  help [name]        - show this help or details about a builtin\n");
        printf("  exit               - exit %s\n", HSH_NAME);
        printf("  cd [dir]           - change directory\n");
        printf("  config             - edit HorizonShell config file\n");
        printf("  alias [name value] - manage command aliases\n\n");

        printf("System commands:\n");
        printf("  sys info           - system info (OS, kernel, host, uptime)\n");
        printf("  sys resources      - CPU/RAM/disk summary\n");
        printf("  sys config         - open HorizonShell config in your editor\n\n");

        printf("Filesystem commands:\n");
        printf("  fs tree [path]     - directory tree view (uses tree or find)\n");
        printf("  fs ls [path]       - colored ls wrapper\n\n");

        printf("Network commands:\n");
        printf("  net ip             - show IP addresses\n");
        printf("  net ping <host>    - ping host with sane defaults\n\n");

        printf("Process commands:\n");
        printf("  ps top             - show top processes by CPU\n");
        printf("  ps find <pattern>  - list processes matching pattern\n\n");

        printf("Scripting helpers:\n");
        printf("  let NAME = VALUE   - set environment variable NAME to VALUE\n");
        printf("  hsh script.hsh     - run script file line by line\n\n");

        printf("Usage:\n");
        printf("  <external-command> [args...]    - runs like a normal shell (ls, cat, etc.)\n");
        printf("  <namespace> <verb> [args...]    - HorizonShell extended syntax (sys, fs, net, ps)\n");

        return 1;
    }

    if (strcmp(args[1], "sys") == 0) {
        printf("sys: system-related commands\n");
        printf("  sys info           - show OS, kernel, host, uptime\n");
        printf("  sys resources      - show CPU, RAM, disk summary\n");
        printf("  sys config         - choose an editor and open ~/.config/hsh/config\n");
        return 1;
    } else if (strcmp(args[1], "fs") == 0) {
        printf("fs: filesystem commands\n");
        printf("  fs tree [path]     - print a directory tree (max depth 3 if tree missing)\n");
        printf("  fs ls [path]       - colored long listing of a directory\n");
        return 1;
    } else if (strcmp(args[1], "net") == 0) {
        printf("net: networking commands\n");
        printf("  net ip             - show IP configuration using ip or ifconfig\n");
        printf("  net ping <host>    - ping host with 4 echo requests\n");
        return 1;
    } else if (strcmp(args[1], "ps") == 0) {
        printf("ps: process inspection commands\n");
        printf("  ps top             - top CPU processes (ps -eo ... | head)\n");
        printf("  ps find <pattern>  - search processes by name using ps aux\n");
        return 1;
    } else if (strcmp(args[1], "exit") == 0) {
        printf("exit: exit %s\n", HSH_NAME);
        printf("  exit               - terminate the current shell session\n");
        return 1;
    } else if (strcmp(args[1], "config") == 0) {
        printf("config: edit HorizonShell config file\n");
        printf("  config             - choose an editor and open ~/.config/hsh/config\n");
        printf("                       restart hsh after changing settings.\n");
        return 1;
    } else if (strcmp(args[1], "alias") == 0) {
        printf("alias: manage command aliases\n");
        printf("  alias              - show where aliases are stored and usage\n");
        printf("  alias name value   - append an alias (name -> value) to aliases file\n");
        printf("                       HSH reloads aliases on startup.\n");
        return 1;
    } else if (strcmp(args[1], "cd") == 0) {
        printf("cd: change the current working directory\n");
        printf("  cd [dir]           - change to dir, or $HOME if omitted\n");
        printf("  cd ~               - change to $HOME\n");
        printf("  cd $VAR            - change to directory in environment variable VAR\n");
        return 1;
    }

    printf("help: no detailed help for '%s' yet.\n", args[1]);
    return 1;
}

int hsh_builtin_sys(char **args) {
    char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "sys: HOME not set\n");
        return 1;
    }

    char confpath[512];
    snprintf(confpath, sizeof(confpath), "%s/.config/hsh/config", home);

    if (args[1] == NULL || strcmp(args[1], "info") == 0) {
        printf("=== System info ===\n");
        system("uname -a");
        system("echo");
        system("echo User: $USER");
        system("echo Host: $(hostname)");
        system("echo");
        system("uptime");
        return 1;
    }

    if (strcmp(args[1], "resources") == 0) {
        printf("=== CPU / Memory / Disk ===\n");
        system("echo CPU: && lscpu | head -n 5");
        system("echo");
        system("echo Memory: && free -h");
        system("echo");
        system("echo Disk: && df -h");
        return 1;
    }

    if (strcmp(args[1], "config") == 0) {
        char *env_editor = getenv("EDITOR");
        char editor[64] = {0};

        printf("=== Edit HorizonShell config ===\n");
        printf("Config file: %s\n", confpath);
        if (env_editor && env_editor[0] != '\0') {
            printf("Detected $EDITOR = %s\n", env_editor);
        }

        printf("Choose editor:\n");
        if (env_editor && env_editor[0] != '\0')
            printf("  1) Use $EDITOR (%s)\n", env_editor);
        else
            printf("  1) nano\n");
        printf("  2) nano\n");
        printf("  3) vim\n");
        printf("  4) code (VS Code CLI)\n");
        printf("Select [1-4] (default 1): ");

        int choice = 1;
        char buf[32];
        if (fgets(buf, sizeof(buf), stdin)) {
            int v;
            if (sscanf(buf, "%d", &v) == 1 && v >= 1 && v <= 4)
                choice = v;
        }

        if (choice == 1) {
            if (env_editor && env_editor[0] != '\0') {
                strncpy(editor, env_editor, sizeof(editor) - 1);
            } else {
                strncpy(editor, "nano", sizeof(editor) - 1);
            }
        } else if (choice == 2) {
            strncpy(editor, "nano", sizeof(editor) - 1);
        } else if (choice == 3) {
            strncpy(editor, "vim", sizeof(editor) - 1);
        } else if (choice == 4) {
            strncpy(editor, "code", sizeof(editor) - 1);
        }

        if (editor[0] == '\0') {
            fprintf(stderr, "sys config: no editor selected\n");
            return 1;
        }

        char cmd[600];
        snprintf(cmd, sizeof(cmd), "%s %s", editor, confpath);
        printf("Opening config with: %s\n", cmd);
        system(cmd);
        printf("Done editing. Changes take effect next time you start hsh (or after reload).\n");
        return 1;
    }

    printf("sys: unknown subcommand '%s'\n", args[1]);
    return 1;
}

int hsh_builtin_fs(char **args) {
    if (args[1] == NULL || strcmp(args[1], "tree") == 0) {
        const char *path = ".";
        if (args[1] && args[2])
            path = args[2];

        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "command -v tree >/dev/null 2>&1 && tree -C %s || (echo \"tree not found, using find\"; find %s -maxdepth 3 -print)",
                 path, path);
        system(cmd);
        return 1;
    }

    if (strcmp(args[1], "ls") == 0) {
        const char *path = ".";
        if (args[2])
            path = args[2];

        char cmd[512];
        snprintf(cmd, sizeof(cmd), "ls --color=auto -al %s", path);
        system(cmd);
        return 1;
    }

    printf("fs: unknown subcommand '%s'\n", args[1]);
    return 1;
}

int hsh_builtin_net(char **args) {
    if (args[1] == NULL || strcmp(args[1], "ip") == 0) {
        printf("=== IP addresses ===\n");
        system("command -v ip >/dev/null 2>&1 && ip addr show || ifconfig");
        return 1;
    }

    if (strcmp(args[1], "ping") == 0) {
        if (!args[2]) {
            printf("Usage: net ping <host>\n");
            return 1;
        }
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "ping -c 4 %s", args[2]);
        system(cmd);
        return 1;
    }

    printf("net: unknown subcommand '%s'\n", args[1]);
    return 1;
}

int hsh_builtin_ps(char **args) {
    if (args[1] == NULL || strcmp(args[1], "top") == 0) {
        printf("=== Top processes (CPU) ===\n");
        system("ps -eo pid,ppid,cmd,%mem,%cpu --sort=-%cpu | head -n 15");
        return 1;
    }

    if (strcmp(args[1], "find") == 0) {
        if (!args[2]) {
            printf("Usage: ps find <pattern>\n");
            return 1;
        }
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "ps aux | grep -i -- '%s' | grep -v grep", args[2]);
        system(cmd);
        return 1;
    }

    printf("ps: unknown subcommand '%s'\n", args[1]);
    return 1;
}

int hsh_builtin_config(char **args) {
    (void)args;

    char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "config: HOME not set\n");
        return 1;
    }

    char confpath[512];
    snprintf(confpath, sizeof(confpath), "%s/.config/hsh/config", home);

    char *env_editor = getenv("EDITOR");
    char editor[64] = {0};

    printf("=== Edit HorizonShell config ===\n");
    printf("Config file: %s\n", confpath);
    if (env_editor && env_editor[0] != '\0') {
        printf("Detected $EDITOR = %s\n", env_editor);
    }

    printf("Choose editor:\n");
    if (env_editor && env_editor[0] != '\0')
        printf("  1) Use $EDITOR (%s)\n", env_editor);
    else
        printf("  1) nano\n");
    printf("  2) nano\n");
    printf("  3) vim\n");
    printf("  4) code (VS Code CLI)\n");
    printf("Select [1-4] (default 1): ");

    int choice = 1;
    char buf[32];
    if (fgets(buf, sizeof(buf), stdin)) {
        int v;
        if (sscanf(buf, "%d", &v) == 1 && v >= 1 && v <= 4)
            choice = v;
    }

    if (choice == 1) {
        if (env_editor && env_editor[0] != '\0') {
            strncpy(editor, env_editor, sizeof(editor) - 1);
        } else {
            strncpy(editor, "nano", sizeof(editor) - 1);
        }
    } else if (choice == 2) {
        strncpy(editor, "nano", sizeof(editor) - 1);
    } else if (choice == 3) {
        strncpy(editor, "vim", sizeof(editor) - 1);
    } else if (choice == 4) {
        strncpy(editor, "code", sizeof(editor) - 1);
    }

    if (editor[0] == '\0') {
        fprintf(stderr, "config: no editor selected\n");
        return 1;
    }

    char cmd[600];
    snprintf(cmd, sizeof(cmd), "%s %s", editor, confpath);
    printf("Opening config with: %s\n", cmd);
    system(cmd);
    printf("Done editing. Changes take effect next time you start hsh (or after reload).\n");
    return 1;
}

int hsh_builtin_alias(char **args) {
    char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "alias: HOME not set\n");
        return 1;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/.config/hsh/aliases", home);

    if (args[1] == NULL) {
        printf("Aliases are stored in %s\n", path);
        printf("Format: name value...\n");
        printf("Example: ll ls -al --color=auto\n");
        printf("Restart hsh after editing this file or adding aliases.\n");
        return 1;
    }

    if (args[2] == NULL) {
        printf("Usage: alias name value...\n");
        return 1;
    }

    char value[384] = {0};
    for (int i = 2; args[i] != NULL; i++) {
        if (i > 2)
            strncat(value, " ", sizeof(value) - strlen(value) - 1);
        strncat(value, args[i], sizeof(value) - strlen(value) - 1);
    }

    FILE *f = fopen(path, "a");
    if (!f) {
        perror("alias: fopen");
        return 1;
    }
    fprintf(f, "%s %s\n", args[1], value);
    fclose(f);

    printf("Alias added: %s -> %s\n", args[1], value);
    printf("Restart hsh to load new aliases.\n");
    return 1;
}
