CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -g

PREFIX  ?= /usr
BINDIR  ?= $(PREFIX)/bin

SRC_DIR := src
BIN_DIR := bin

SRCS       := $(SRC_DIR)/main.c $(SRC_DIR)/extras.c $(SRC_DIR)/parser.c
SETUP_SRCS := $(SRC_DIR)/setup.c

OBJS       := $(SRCS:.c=.o)
SETUP_OBJS := $(SETUP_SRCS:.c=.o)

HSH_BIN      := $(BIN_DIR)/hsh
HSH_SETUPBIN := $(BIN_DIR)/hsh-setup

all: $(HSH_BIN) $(HSH_SETUPBIN)

$(HSH_BIN): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -lreadline


$(HSH_SETUPBIN): $(SETUP_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(SETUP_OBJS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(SRC_DIR)/*.o
	rm -f $(HSH_BIN) $(HSH_SETUPBIN)
	rm -f $(HOME)/.config/hsh/config $(HOME)/.config/hsh/aliases

.PHONY: install
install: $(HSH_BIN) $(HSH_SETUPBIN)
	mkdir -p "$(DESTDIR)$(BINDIR)"
	install -m 0755 $(HSH_BIN) "$(DESTDIR)$(BINDIR)/hsh"
	install -m 0755 $(HSH_SETUPBIN) "$(DESTDIR)$(BINDIR)/hsh-setup"

.PHONY: uninstall
uninstall:
	rm -f "$(BINDIR)/hsh" "$(BINDIR)/hsh-setup"
