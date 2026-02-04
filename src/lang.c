#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lang.h"

/* ===== tiny lexer ===== */

typedef enum {
    TOK_EOF,
    TOK_IDENT,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_CHAIN      /* ")(" */
} token_kind;

typedef struct {
    token_kind kind;
    char       ident[64]; /* for TOK_IDENT */
} token;

typedef struct {
    const char *p;
} lexer;

static void lx_init(lexer *lx, const char *src) {
    lx->p = src;
}

static void lx_skip_ws(lexer *lx) {
    while (*lx->p == ' ' || *lx->p == '\t' || *lx->p == '\r' || *lx->p == '\n')
        lx->p++;
}

static token lx_next(lexer *lx) {
    token t;
    t.kind = TOK_EOF;
    t.ident[0] = '\0';

    lx_skip_ws(lx);

    char c = *lx->p;
    if (c == '\0') {
        t.kind = TOK_EOF;
        return t;
    }

    /* ")(" operator */
    if (c == ')' && lx->p[1] == '(') {
        lx->p += 2;
        t.kind = TOK_CHAIN;
        return t;
    }

    if (isalpha((unsigned char)c) || c == '_') {
        size_t n = 0;
        while (isalpha((unsigned char)*lx->p) ||
               isdigit((unsigned char)*lx->p) ||
               *lx->p == '_') {
            if (n + 1 < sizeof(t.ident)) {
                t.ident[n++] = *lx->p;
            }
            lx->p++;
        }
        t.ident[n] = '\0';
        t.kind = TOK_IDENT;
        return t;
    }

    if (c == '(') {
        lx->p++;
        t.kind = TOK_LPAREN;
        return t;
    }

    if (c == ')') {
        lx->p++;
        t.kind = TOK_RPAREN;
        return t;
    }

    /* unknown char: skip and signal EOF */
    lx->p++;
    t.kind = TOK_EOF;
    return t;
}

/* ===== one-token lookahead parser wrapper ===== */

typedef struct {
    lexer lx;
    token cur;
    int   has_cur;
} parser;

static void ps_init(parser *ps, const char *src) {
    lx_init(&ps->lx, src);
    ps->has_cur = 0;
}

static token ps_peek(parser *ps) {
    if (!ps->has_cur) {
        ps->cur = lx_next(&ps->lx);
        ps->has_cur = 1;
    }
    return ps->cur;
}

static token ps_next(parser *ps) {
    if (ps->has_cur) {
        ps->has_cur = 0;
        return ps->cur;
    }
    return lx_next(&ps->lx);
}

/* ===== AST allocation / free ===== */

static hsh_node *hsh_make_call(const char *name) {
    hsh_node *n = malloc(sizeof *n);
    if (!n) return NULL;
    n->kind = HSH_NODE_CALL;
    n->u.call.name = strdup(name);
    if (!n->u.call.name) {
        free(n);
        return NULL;
    }
    return n;
}

static hsh_node *hsh_make_chain(hsh_chain_kind op,
                                hsh_node *left,
                                hsh_node *right) {
    hsh_node *n = malloc(sizeof *n);
    if (!n) return NULL;
    n->kind = HSH_NODE_CHAIN;
    n->u.chain.op = op;
    n->u.chain.left = left;
    n->u.chain.right = right;
    return n;
}

void hsh_lang_free(hsh_node *node) {
    if (!node) return;
    if (node->kind == HSH_NODE_CALL) {
        free(node->u.call.name);
    } else if (node->kind == HSH_NODE_CHAIN) {
        hsh_lang_free(node->u.chain.left);
        hsh_lang_free(node->u.chain.right);
    }
    free(node);
}

/* ===== recursive-descent parser ===== */

/* Call := IDENT '(' ')' */
static hsh_node *parse_call(parser *ps) {
    token t = ps_next(ps);
    if (t.kind != TOK_IDENT) {
        fprintf(stderr, "hsh-lang syntax error: expected identifier\n");
        return NULL;
    }

    char name[64];
    strncpy(name, t.ident, sizeof(name));
    name[sizeof(name) - 1] = '\0';

    t = ps_next(ps);
    if (t.kind != TOK_LPAREN) {
        fprintf(stderr, "hsh-lang syntax error: expected '('\n");
        return NULL;
    }

    t = ps_next(ps);
    if (t.kind != TOK_RPAREN) {
        fprintf(stderr, "hsh-lang syntax error: expected ')'\n");
        return NULL;
    }

    return hsh_make_call(name);
}

/* Expr := Call ( ')(' Call )* */
static hsh_node *parse_expr(parser *ps) {
    hsh_node *left = parse_call(ps);
    if (!left) return NULL;

    for (;;) {
        token t = ps_peek(ps);
        if (t.kind != TOK_CHAIN)
            break;

        /* consume ')(' */
        (void)ps_next(ps);

        hsh_node *right = parse_call(ps);
        if (!right) {
            hsh_lang_free(left);
            return NULL;
        }

        /* use success chaining for now; tweak if you add error chaining */
        left = hsh_make_chain(HSH_CHAIN_ON_SUCCESS, left, right);
        if (!left) {
            hsh_lang_free(right);
            return NULL;
        }
    }

    return left;
}

hsh_node *hsh_lang_parse_stmt(const char *line) {
    parser ps;
    ps_init(&ps, line);

    hsh_node *root = parse_expr(&ps);
    if (!root) return NULL;

    token t = ps_peek(&ps);
    if (t.kind != TOK_EOF) {
        fprintf(stderr, "hsh-lang syntax error after expression\n");
        hsh_lang_free(root);
        return NULL;
    }

    return root;
}

/* ===== evaluation ===== */

/* stub: no builtins; just echo the call */
static int eval_call(hsh_node *n) {
    if (!n || n->kind != HSH_NODE_CALL) return 1;
    fprintf(stdout, "called %s()\n", n->u.call.name);
    return 0;
}

static int eval_node(hsh_node *n) {
    if (!n) return 1;

    if (n->kind == HSH_NODE_CALL) {
        return eval_call(n);
    }

    if (n->kind == HSH_NODE_CHAIN) {
        int st_left = eval_node(n->u.chain.left);

        if (n->u.chain.op == HSH_CHAIN_ON_SUCCESS) {
            if (st_left == 0) {
                int st_right = eval_node(n->u.chain.right);
                return st_right;
            }
            return st_left;
        } else { /* HSH_CHAIN_ON_ERROR */
            if (st_left != 0) {
                int st_right = eval_node(n->u.chain.right);
                return st_right;
            }
            return st_left;
        }
    }

    return 1;
}

int hsh_lang_eval(hsh_node *node) {
    return eval_node(node);
}
