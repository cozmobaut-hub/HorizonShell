#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lang.h"

/* HorizonShell builtin: lang ... */
int hsh_builtin_lang(char **args) {
    if (!args[1]) {
        fprintf(stderr, "hsh-lang: empty input\n");
        return 1;
    }

    char buf[512] = {0};

    /* join args[1..] into a single program string */
    for (int i = 1; args[i]; i++) {
        if (i > 1)
            strncat(buf, " ", sizeof(buf) - strlen(buf) - 1);
        strncat(buf, args[i], sizeof(buf) - strlen(buf) - 1);
    }

    if (buf[0] == '\0') {
        fprintf(stderr, "hsh-lang: empty input\n");
        return 1;
    }

    hsh_node *n = hsh_lang_parse_stmt(buf);
    if (!n)
        return 1;

    int st = hsh_lang_eval(n);
    hsh_lang_free(n);

    (void)st; /* keep HorizonShell running */
    return 1;
}

#ifdef BUILD_HSH_MAIN
/* Standalone interpreter: hsh-lang <file> */
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: hsh-lang <file>\n");
        return 1;
    }

    const char *path = argv[1];
    FILE *f = fopen(path, "r");
    if (!f) {
        perror("hsh-lang: fopen");
        return 1;
    }

    char line[512];
    int st = 0;

    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0')
            continue;

        hsh_node *n = hsh_lang_parse_stmt(p);
        if (!n) {
            st = 1;
            break;
        }
        st = hsh_lang_eval(n);
        hsh_lang_free(n);
    }

    fclose(f);
    return st;
}
#endif
