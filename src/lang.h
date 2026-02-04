#ifndef HSH_LANG_H
#define HSH_LANG_H

typedef enum {
    HSH_NODE_CALL,
    HSH_NODE_CHAIN
} hsh_node_kind;

typedef enum {
    HSH_CHAIN_ON_SUCCESS,
    HSH_CHAIN_ON_ERROR
} hsh_chain_kind;

typedef struct hsh_node hsh_node;

struct hsh_node {
    hsh_node_kind kind;
    union {
        struct {
            char *name;  /* e.g. "do_network" */
        } call;
        struct {
            hsh_chain_kind op;
            hsh_node *left;
            hsh_node *right;
        } chain;
    } u;
};

/* Parse a single statement:
 *   do_network()
 *   do_network() () next_step()
 *   do_network() )( handle_error()
 * Returns NULL on syntax error.
 */
hsh_node *hsh_lang_parse_stmt(const char *line);

/* Free AST */
void hsh_lang_free(hsh_node *node);

/* Evaluate AST:
 * 0 = success, non‑zero = failure
 */
int hsh_lang_eval(hsh_node *node);

#endif
