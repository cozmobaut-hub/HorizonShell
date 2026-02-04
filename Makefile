CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -g

PREFIX  ?= /usr
BINDIR  ?= $(PREFIX)/bin

SRC_DIR := src
BIN_DIR := bin

# main shell uses hsh_lang_builtin.o (no main)
OBJS_HSH      := $(SRC_DIR)/main.o \
                 $(SRC_DIR)/extras.o \
                 $(SRC_DIR)/parser.o \
                 $(SRC_DIR)/lang.o \
                 $(SRC_DIR)/hsh_lang_builtin.o

# standalone interpreter uses hsh_lang_main.o (with main)
OBJS_LANG_BIN := $(SRC_DIR)/lang.o \
                 $(SRC_DIR)/hsh_lang_main.o

SETUP_SRCS    := $(SRC_DIR)/setup.c
SETUP_OBJS    := $(SETUP_SRCS:.c=.o)

HSH_BIN       := $(BIN_DIR)/hsh
HSH_LANG_BIN  := $(BIN_DIR)/hsh-lang
HSH_SETUPBIN  := $(BIN_DIR)/hsh-setup

all: $(HSH_BIN) $(HSH_SETUPBIN) $(HSH_LANG_BIN)

$(HSH_BIN): $(OBJS_HSH) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS_HSH) -lreadline

$(HSH_LANG_BIN): $(OBJS_LANG_BIN) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS_LANG_BIN)

$(HSH_SETUPBIN): $(SETUP_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(SETUP_OBJS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# special builds for hsh_lang.c
$(SRC_DIR)/hsh_lang_builtin.o: $(SRC_DIR)/hsh_lang.c
	$(CC) $(CFLAGS) -DBUILD_HSH_BUILTIN -c $< -o $@

$(SRC_DIR)/hsh_lang_main.o: $(SRC_DIR)/hsh_lang.c
	$(CC) $(CFLAGS) -DBUILD_HSH_MAIN -c $< -o $@

.PHONY: clean
clean:
	rm -f $(SRC_DIR)/*.o
	rm -f $(HSH_BIN) $(HSH_SETUPBIN) $(HSH_LANG_BIN)
	rm -f $(HOME)/.config/hsh/config $(HOME)/.config/hsh/aliases

.PHONY: install
install: $(HSH_BIN) $(HSH_SETUPBIN) $(HSH_LANG_BIN)
	mkdir -p "$(DESTDIR)$(BINDIR)"
	install -m 0755 $(HSH_BIN)      "$(DESTDIR)$(BINDIR)/hsh"
	install -m 0755 $(HSH_SETUPBIN) "$(DESTDIR)$(BINDIR)/hsh-setup"
	install -m 0755 $(HSH_LANG_BIN) "$(DESTDIR)$(BINDIR)/hsh-lang"

.PHONY: uninstall
uninstall:
	rm -f "$(BINDIR)/hsh" "$(BINDIR)/hsh-setup" "$(BINDIR)/hsh-lang"
