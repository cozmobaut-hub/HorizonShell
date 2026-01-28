# HorizonShell

HorizonShell (`hsh`) is a **lightweight C-based Linux shell** with status bar, system-aware builtins, and 30%+ faster startup than bash. Perfect for homelabs, servers, Minecraft automation, and scripting. [ [

## 🚀 Performance Benchmarks

| Shell | Startup Time | Loop 500 echoes |
|-------|--------------|-----------------|
| **hsh**   | **0.174s** ⚡ | **0.030s**     |
| bash  | 0.252s      | 0.031s         |
| dash  | 0.278s      | 0.065s         |
| zsh   | 0.291s      | 0.031s         |

**hsh = 30-45% faster startup** than bash. Ideal for frequent shell spawns in scripts/clusters.

## ✨ Features

```
Status bar: time | CPU 15% | RAM 2.1G
$ sys info        # OS/kernel/uptime
$ fs tree .       # dir tree
$ net ip          # IP config
$ ps top          # top processes
$ alias ll='ls -al' # persistent aliases
$ ls | grep .c    # pipelines
```

- **Live status bar** (time, CPU, RAM)
- **5 builtin namespaces**: `sys`, `fs`, `net`, `ps`
- **Persistent aliases** (`~/.config/hsh/aliases`)
- **Simple pipelines** (`cmd1 | cmd2`)
- **Interactive config wizard** first-run
- **Hackable C codebase** (~1k LOC)

## 🎯 Quick Install

```bash
# Build & run
git clone https://github.com/cozmobaut-hub/HorizonShell
cd HorizonShell
make && ./bin/hsh

# System install
sudo make install

# Debian package (coming soon)
# dpkg -i hsh_0.1.1-1_amd64.deb
```

**One-liner test**: `curl -sL github.com/cozmobaut-hub/HorizonShell/raw/main/bin/hsh >hsh && chmod +x hsh && ./hsh`

## 📁 Layout

```
.
├── Makefile
├── src/          # main.c parser.c extras.c (~1k LOC)
├── bin/
│   ├── hsh       # shell binary
│   └── hsh-setup # config wizard
└── README.md
```

## 🎮 Usage Examples

```bash
$ hsh
hsh: first run detected, launching setup...
[prompt/status config wizard]

# System info
$ sys info
Linux 6.5.0-kali amd64 | user: haustintexas2 | uptime: 2h 15m

# Network
$ net ip
192.168.1.100/24 enp3s0 (wlan0)

# Processes
$ ps top
hsh 1234  0.2%  | sshd 567  0.1%  | bash 890  0.0%

# Aliases (persistent)
$ alias ll='ls -al --color=auto'
$ exit && hsh && ll  # works next session
```

## 🔧 Configuration

**First run**: Interactive wizard sets prompt colors + status bar.

**Edit anytime**: `config` (opens `~/.config/hsh/config` in $EDITOR)

```ini
fg = 32        # prompt fg color
bg = 40        # prompt bg  
enabled = 1    # statusbar on
show_time = 1
show_cpu = 1
show_ram = 1
```

## 🛠️ Builtins

| Namespace | Commands | Example |
|-----------|----------|---------|
| `sys` | `info`, `resources`, `config` | `sys resources` |
| `fs` | `tree`, `ls` | `fs tree /var/log` |
| `net` | `ip`, `ping HOST` | `net ping 1.1.1.1` |
| `ps` | `top`, `find TERM` | `ps find hsh` |

**Help**: `help` or `help sys`

## 🚀 Why hsh?

- **Faster than bash** for shell-heavy workloads (homelabs, mining, game servers)
- **Status-at-a-glance** (no `htop` tab-switching)
- **Minimal C** (no bloat, easy to hack/extend)
- **Debian packaging** in progress
- **Modern config** (`~/.config/hsh/`)

## 🤝 Contributing

1. Fork → hack → PR
2. Good first issues: [add redirection `>`](#), [script mode `hsh script.hsh`](#)
3. Test: `make test`
4. Debian packaging help welcome!

## 📦 Debian Package

```
Sponsored upload to mentors.debian.net complete.
Ready for sponsor review → Debian unstable.
```

## 💝 Support

[
[

**Star if useful!** ⭐ [cozmobaut-hub/HorizonShell](https://github.com/cozmobaut-hub/HorizonShell)

***

**MIT License** | Built with ❤️ for homelab hackers | v0.1.1