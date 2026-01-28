#ifndef HSH_EXTRAS_H
#define HSH_EXTRAS_H

struct hsh_config {
    int fg;
    int bg;
    int sb_enabled;
    int sb_time;
    int sb_cpu;
    int sb_ram;
};

struct hsh_alias {
    char *name;
    char *value;
};

int  hsh_load_config(const char *path, struct hsh_config *cfg);
void hsh_draw_statusbar(const struct hsh_config *cfg);

/* alias functions */
int  hsh_load_aliases(const char *path, struct hsh_alias **aliases, int *count);
void hsh_free_aliases(struct hsh_alias *aliases, int count);
char *hsh_expand_alias(struct hsh_alias *aliases, int count, const char *cmd);

#endif
