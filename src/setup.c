#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

static void ask_yes_no(const char *prompt, int *out);
static void write_config(const char *path,
                         int fg, int bg,
                         int sb_enabled, int sb_time, int sb_cpu, int sb_ram);

int main(void) {
    char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "hsh-setup: HOME not set\n");
        return 1;
    }

    char confdir[512];
    char confpath[512];

    snprintf(confdir, sizeof(confdir), "%s/.config/hsh", home);
    snprintf(confpath, sizeof(confpath), "%s/config", confdir);

    /* ensure config dir exists */
    if (mkdir(confdir, 0755) && errno != EEXIST) {
        perror("hsh-setup: mkdir");
        return 1;
    }

    int fg_choice = 1;
    int bg_choice = 1;
    int fg, bg;
    int sb_enabled = 1, sb_time = 1, sb_cpu = 1, sb_ram = 1;

    printf("=== hsh setup ===\n");

    /* Foreground color menu */
    printf("Foreground color:\n");
    printf("  1) Green\n");
    printf("  2) Cyan\n");
    printf("  3) Yellow\n");
    printf("  4) Blue\n");
    printf("  5) Magenta\n");
    printf("  6) White\n");
    printf("  7) Red\n");
    printf("Select [1-7] (default 1): ");
    {
        char buf[16];
        if (fgets(buf, sizeof(buf), stdin)) {
            int v;
            if (sscanf(buf, "%d", &v) == 1 && v >= 1 && v <= 7)
                fg_choice = v;
        }
    }

    /* Background color menu */
    printf("Background color:\n");
    printf("  1) Black\n");
    printf("  2) Blue\n");
    printf("  3) Cyan\n");
    printf("  4) White\n");
    printf("Select [1-4] (default 1): ");
    {
        char buf[16];
        if (fgets(buf, sizeof(buf), stdin)) {
            int v;
            if (sscanf(buf, "%d", &v) == 1 && v >= 1 && v <= 4)
                bg_choice = v;
        }
    }

    /* map choices to ANSI SGR codes */
    fg = 32;  /* default green */
    switch (fg_choice) {
        case 1: fg = 32; break; /* green */
        case 2: fg = 36; break; /* cyan */
        case 3: fg = 33; break; /* yellow */
        case 4: fg = 34; break; /* blue */
        case 5: fg = 35; break; /* magenta */
        case 6: fg = 37; break; /* white */
        case 7: fg = 31; break; /* red */
    }

    bg = 40;  /* default black */
    switch (bg_choice) {
        case 1: bg = 40; break; /* black */
        case 2: bg = 44; break; /* blue */
        case 3: bg = 46; break; /* cyan */
        case 4: bg = 47; break; /* white */
    }

    /* Status bar options */
    ask_yes_no("Enable status bar at bottom? (y/n) [y]: ", &sb_enabled);
    if (sb_enabled) {
        ask_yes_no("Show time in status bar? (y/n) [y]: ", &sb_time);
        ask_yes_no("Show CPU usage? (y/n) [y]: ", &sb_cpu);
        ask_yes_no("Show RAM usage? (y/n) [y]: ", &sb_ram);
    } else {
        sb_time = sb_cpu = sb_ram = 0;
    }

    write_config(confpath, fg, bg, sb_enabled, sb_time, sb_cpu, sb_ram);

    printf("Config written to %s\n", confpath);
    return 0;
}

static void ask_yes_no(const char *prompt, int *out) {
    char buf[16];

    printf("%s", prompt);
    if (!fgets(buf, sizeof(buf), stdin))
        return;

    if (buf[0] == 'n' || buf[0] == 'N')
        *out = 0;
    else
        *out = 1;
}

static void write_config(const char *path,
                         int fg, int bg,
                         int sb_enabled, int sb_time, int sb_cpu, int sb_ram)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        perror("hsh-setup: fopen config");
        exit(1);
    }

    fprintf(f,
            "[theme]\n"
            "fg = %d\n"
            "bg = %d\n"
            "\n"
            "[statusbar]\n"
            "enabled = %d\n"
            "show_time = %d\n"
            "show_cpu = %d\n"
            "show_ram = %d\n",
            fg, bg,
            sb_enabled, sb_time, sb_cpu, sb_ram);

    fclose(f);
}
