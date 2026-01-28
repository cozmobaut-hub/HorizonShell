#ifndef HSH_PARSER_H
#define HSH_PARSER_H

/* Execute a full command line (after alias expansion).
 * Handles:
 *   - Builtins (help, exit, config, alias, sys, fs, net, ps)
 *   - External commands
 *   - Simple pipelines with '|'
 * Returns 0 to exit shell, 1 to continue.
 */
int hsh_run_line(char *line);

#endif
