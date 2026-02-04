#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "extras.h"
#include "lang.h"

/* Simple state for CPU usage between calls */
static unsigned long long last_total_jiffies = 0;
static unsigned long long last_work_jiffies = 0;

/* builtin implemented in hsh_lang.c */
int hsh_builtin_lang(char **args);

int hsh_load_config(const char *path, struct hsh_config *cfg) {
    FILE *f = fopen(path, "r");
    if (!f) {
        perror("hsh: fopen config");
        return -1;
    }

    /* defaults */
    cfg->fg = 32;
    cfg->bg = 40;
    cfg->sb_enabled = 1;
    cfg->sb_time = 1;
    cfg->sb_cpu = 1;
    cfg->sb_ram = 1;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        int value;
        if (sscanf(line, "fg = %d", &value) == 1) {
            cfg->fg = value;
        } else if (sscanf(line, "bg = %d", &value) == 1) {
            cfg->bg = value;
        } else if (sscanf(line, "enabled = %d", &value) == 1) {
            cfg->sb_enabled = value;
        } else if (sscanf(line, "show_time = %d", &value) == 1) {
            cfg->sb_time = value;
        } else if (sscanf(line, "show_cpu = %d", &value) == 1) {
            cfg->sb_cpu = value;
        } else if (sscanf(line, "show_ram = %d", &value) == 1) {
            cfg->sb_ram = value;
        }
    }

    fclose(f);
    return 0;
}

/* helper: read /proc/stat and compute CPU usage since last call */
static double hsh_get_cpu_usage(void) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return -1.0;

    char buf[256];
    if (!fgets(buf, sizeof(buf), f)) {
        fclose(f);
        return -1.0;
    }
    fclose(f);

    char cpu_label[5];
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    int scanned = sscanf(buf, "%4s %llu %llu %llu %llu %llu %llu %llu %llu",
                         cpu_label, &user, &nice, &system, &idle, &iowait,
                         &irq, &softirq, &steal);
    if (scanned < 5) return -1.0;

    unsigned long long idle_all = idle + iowait;
    unsigned long long non_idle = user + nice + system + irq + softirq + steal;
    unsigned long long total = idle_all + non_idle;

    double cpu_percent = 0.0;
    if (last_total_jiffies != 0) {
        unsigned long long total_diff = total - last_total_jiffies;
        unsigned long long work_diff = non_idle - last_work_jiffies;
        if (total_diff > 0) {
            cpu_percent = (double)work_diff * 100.0 / (double)total_diff;
        }
    }

    last_total_jiffies = total;
    last_work_jiffies = non_idle;

    return cpu_percent;
}

/* helper: read /proc/meminfo and compute used / total in GiB */
static void hsh_get_ram_usage(double *used_gib, double *total_gib) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) {
        *used_gib = *total_gib = 0.0;
        return;
    }

    unsigned long long value_kb;
    unsigned long long mem_total = 0, mem_available = 0;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "MemTotal: %llu kB", &value_kb) == 1) {
            mem_total = value_kb;
        } else if (sscanf(line, "MemAvailable: %llu kB", &value_kb) == 1) {
            mem_available = value_kb;
        }
        if (mem_total && mem_available) break;
    }
    fclose(f);

    if (mem_total == 0) {
        *used_gib = *total_gib = 0.0;
        return;
    }

    unsigned long long mem_used = mem_total - mem_available;
    *total_gib = (double)mem_total / (1024.0 * 1024.0);
    *used_gib = (double)mem_used / (1024.0 * 1024.0);
}

void hsh_draw_statusbar(const struct hsh_config *cfg) {
    if (!cfg->sb_enabled)
        return;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char tbuf[32] = {0};

    if (cfg->sb_time && tm) {
        strftime(tbuf, sizeof(tbuf), "%H:%M:%S", tm);
    }

    double cpu = -1.0;
    double ram_used = 0.0, ram_total = 0.0;

    if (cfg->sb_cpu)
        cpu = hsh_get_cpu_usage();
    if (cfg->sb_ram)
        hsh_get_ram_usage(&ram_used, &ram_total);

    printf("\033[0m\033[7m");  /* reset, then reverse video */

    if (cfg->sb_time)
        printf(" %s ", tbuf);

    if (cfg->sb_cpu && cpu >= 0.0)
        printf(" CPU:%.1f%% ", cpu);

    if (cfg->sb_ram && ram_total > 0.0)
        printf(" RAM:%.1f/%.1fGiB ", ram_used, ram_total);

    printf("\033[0m\n");
}

/* ====== ALIASES ====== */

int hsh_load_aliases(const char *path, struct hsh_alias **aliases, int *count) {
    FILE *f = fopen(path, "r");
    if (!f) {
        *aliases = NULL;
        *count = 0;
        return 0;  /* no aliases is fine */
    }

    struct hsh_alias *list = NULL;
    int cap = 0, n = 0;
    char line[512];

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n')
            continue;

        char name[128], value[384];
        if (sscanf(line, "%127s %383[^\n]", name, value) == 2) {
            if (n >= cap) {
                cap = cap ? cap * 2 : 8;
                struct hsh_alias *tmp =
                    realloc(list, cap * sizeof(struct hsh_alias));
                if (!tmp) {
                    perror("hsh: realloc aliases");
                    fclose(f);
                    if (list) {
                        for (int i = 0; i < n; i++) {
                            free(list[i].name);
                            free(list[i].value);
                        }
                        free(list);
                    }
                    *aliases = NULL;
                    *count = 0;
                    return -1;
                }
                list = tmp;
            }
            list[n].name = strdup(name);
            list[n].value = strdup(value);
            if (!list[n].name || !list[n].value) {
                perror("hsh: strdup alias");
                fclose(f);
                for (int i = 0; i <= n; i++) {
                    if (list[i].name) free(list[i].name);
                    if (list[i].value) free(list[i].value);
                }
                free(list);
                *aliases = NULL;
                *count = 0;
                return -1;
            }
            n++;
        }
    }

    fclose(f);
    *aliases = list;
    *count = n;
    return 0;
}

void hsh_free_aliases(struct hsh_alias *aliases, int count) {
    if (!aliases) return;
    for (int i = 0; i < count; i++) {
        free(aliases[i].name);
        free(aliases[i].value);
    }
    free(aliases);
}

/* Returns malloc'd string if alias found, else NULL */
char *hsh_expand_alias(struct hsh_alias *aliases, int count, const char *cmd) {
    for (int i = 0; i < count; i++) {
        if (strcmp(aliases[i].name, cmd) == 0) {
            return strdup(aliases[i].value);
        }
    }
    return NULL;
}
