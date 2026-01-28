# HorizonShell

HorizonShell (`hsh`) is a simple, hybrid Linux shell with a status bar, system-aware builtins, and lightweight alias support. It is written in C and designed as a learning-friendly, hackable shell you can extend.

## Features

- Status bar with time, CPU, and RAM usage.
- Colored prompt, configurable via `~/.config/hsh/config`.
- Builtin namespaces:
  - `sys` (info, resources, config)
  - `fs` (tree, ls)
  - `net` (ip, ping)
  - `ps` (top, find)
- Simple alias system (`alias ll ls -al --color=auto`).
- Basic pipelines: `cmd1 | cmd2 | cmd3`.
- `config` builtin to open the config file in your editor of choice.

## Project layout

```text
.
├── Makefile
├── README.md
├── src
│   ├── main.c       # entry point, REPL, builtins
│   ├── parser.c     # parsing + execution (pipes, external cmds, builtins)
│   ├── extras.c     # status bar, config loader, alias support
│   ├── setup.c      # first-run interactive config wizard
│   ├── extras.h
│   └── parser.h
└── bin
    ├── hsh          # built shell binary
    └── hsh-setup    # setup helper (installed alongside hsh)
```

## Building

Requirements:

- GCC or compatible C compiler
- A POSIX-like environment (Linux, WSL, etc.)

From the repo root:

```bash
make
```

This will:

- Compile sources in `src/`
- Produce binaries in `bin/`:
  - `bin/hsh`
  - `bin/hsh-setup`

You can run the shell directly from the build tree:

```bash
./bin/hsh
```

## Installation

By default, `make install` installs into `/usr/local/bin`:

```bash
sudo make install
```

This installs:

- `/usr/local/bin/hsh`
- `/usr/local/bin/hsh-setup`

You can change the prefix:

```bash
sudo make PREFIX=/usr install
```

To uninstall:

```bash
sudo make uninstall
```

## First run and configuration

On first run, if `~/.config/hsh/config` does not exist, `hsh` automatically launches `hsh-setup`:

```bash
hsh
# hsh: first run, launching setup...
```

The setup wizard lets you choose:

- Prompt foreground color
- Prompt background color
- Whether to show a status bar
- Which fields to show in the status bar (time, CPU, RAM)

After that, you can adjust settings any time from inside `hsh`:

```bash
config
# choose an editor (nano, vim, code, or $EDITOR)
```

This opens `~/.config/hsh/config` for manual editing.

### Config file

The config lives at:

```text
~/.config/hsh/config
```

Example options (actual parser expects simple `key = value` lines):

```text
fg = 32          # foreground color code
bg = 40          # background color code
enabled = 1      # status bar on/off
show_time = 1
show_cpu = 1
show_ram = 1
```

Changes take effect on the next `hsh` start.

## Aliases

Aliases are stored in:

```text
~/.config/hsh/aliases
```

Each line is:

```text
name value...
```

Inside `hsh`:

```bash
alias
# prints alias file path and usage

alias ll ls -al --color=auto
# appends "ll ls -al --color=auto" to the aliases file

exit
hsh         # restart to load new aliases
ll          # runs ls -al --color=auto
```

Notes:

- Only the **first word** of a command line is expanded as an alias.
- Aliases are loaded at shell startup.

## Builtins and examples

### General

```bash
help                # overview of builtins and namespaces
help sys            # detailed help for "sys"
help fs
help net
help ps
```

### System (sys)

```bash
sys info            # OS, kernel, user, host, uptime
sys resources       # CPU, memory, disk summary (using lscpu, free, df)
sys config          # choose editor and open ~/.config/hsh/config
```

### Filesystem (fs)

```bash
fs tree             # directory tree of current directory
fs tree /var/log    # tree of a specific path

fs ls               # colored ls -al of current directory
fs ls /tmp
```

### Network (net)

```bash
net ip              # show IP configuration (ip addr show or ifconfig)
net ping example.com
net ping 1.1.1.1
```

### Processes (ps)

```bash
ps top              # top CPU processes (ps -eo ... | head)
ps find hsh         # grep-like search over ps aux
```

## Pipelines

HorizonShell supports simple pipelines with `|`:

```bash
ls -l | grep '\.c'
ps aux | grep hsh | grep -v grep
cat main.c | wc -l
```

Limitations:

- No redirection (`>`, `<`, `>>`) yet.
- Builtins inside pipelines (e.g. `help | less`) are not handled; pipelines are for external commands.

## Scripting (future work)

Current state:

- `hsh` is interactive.
- The parser supports single-line commands and pipelines.

Planned / easy extensions:

- `hsh -c "command"` style batch mode.
- `hsh script.hsh` to run a file line-by-line through the existing parser.

Full control flow (`if`, `while`, variables) is intentionally out of scope for the initial version.

## Contributing

This project is intentionally small and hackable. Ideas to explore:

- Add history and better line editing (e.g. via GNU readline or linenoise).
- Implement redirection (`>`, `<`, `>>`) and chaining (`&&`, `||`, `;`).
- Improve alias handling (arguments, expansion beyond first word).
- Add `-c` and script file execution.

Pull requests and forks are welcome.

## License

HorizonShell is licensed under the MIT License.

See the license header in `src/main.c` and other source files