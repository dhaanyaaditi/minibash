# minibash

A Unix shell implemented from scratch in C++17 — featuring pipes, I/O redirection, job control, readline-style history, and variable expansion.

Compatible with standard shell scripts; passes `shellcheck` on most constructs.

```
$ ./minibash
minibash 1.0.0  (type 'exit' to quit)
aditi@iitd:~ $ echo "Hello $USER"
Hello aditi
aditi@iitd:~ $ ls src/ | grep -v ".o" | wc -l
9
aditi@iitd:~ $ sleep 10 &
[1] 12345
aditi@iitd:~ $ jobs
[1]  Running    sleep 10
aditi@iitd:~ $ fg 1
sleep 10
^C
```

---

## Why I built this

Shells are one of the oldest and most fundamental programs in Unix — yet most CS courses treat them as magic. Building one forced me to understand:

- How `fork()` + `exec()` actually work at the syscall level
- Why pipes need to be created *before* forking
- What "process group" means and why terminals care about it
- How job control (Ctrl-Z, `fg`, `bg`) is implemented using `SIGTSTP`, `SIGCONT`, and `tcsetpgrp`
- Why `cd` and `export` must be builtins (they can't run in a child process)

---

## Architecture

```
┌──────────────────────────────────────────────────────┐
│                     Shell (REPL)                     │
│    readline prompt → signal handling → history       │
└──────────────┬───────────────────────────────────────┘
               │  raw input string
┌──────────────▼───────────────┐
│           Lexer               │  char stream → Token[]
│   quotes, escapes, operators  │
└──────────────┬────────────────┘
               │  Token[]
┌──────────────▼───────────────┐
│           Parser              │  Token[] → CmdList AST
│   pipelines, && || ; &        │
└──────────────┬────────────────┘
               │  CmdList
┌──────────────▼───────────────┐
│          Executor             │  walk AST, run commands
│   fork/exec, pipes, redirects │
└───┬──────────┬────────────────┘
    │          │
┌───▼──┐  ┌───▼──────┐
│Builtin│  │ External │
│ (no  │  │ command  │
│ fork)│  │ fork+exec│
└───────┘  └──────────┘
```

### Key design decisions

| Component | Decision | Why |
|---|---|---|
| Pipe setup | Create pipe **before** `fork()` | Both ends exist before child inherits them |
| Builtins | Run in shell process (no fork) | `cd`, `export`, `exit` must affect shell state |
| Job control | `setpgid(0,0)` in child | Each job gets its own process group for `SIGTSTP` scoping |
| Terminal | `tcsetpgrp()` to transfer | Foreground job gets keyboard input; shell reclaims on exit |
| SIGCHLD | `waitpid(WNOHANG)` in handler | Non-blocking reap prevents zombie accumulation |
| History | Ring buffer (deque) | O(1) add/remove; saved to `~/.minibash_history` on exit |

---

## Features

### Commands & pipelines
```bash
ls -la                         # simple command
ls | grep .cpp | wc -l         # pipeline
echo hello; echo world         # sequential (;)
make && ./run_tests            # conditional: run if previous succeeded
make || echo "build failed"    # conditional: run if previous failed
! test -f file.txt             # negate exit status
```

### I/O redirection
```bash
echo "hello" > output.txt      # redirect stdout (truncate)
echo "more"  >> output.txt     # redirect stdout (append)
sort < unsorted.txt            # redirect stdin
cmd 2> errors.txt              # redirect stderr
cat << EOF                     # here-doc
  hello world
EOF
```

### Variables & expansion
```bash
NAME="aditi"
echo $NAME                     # → aditi
echo ${NAME}!                  # → aditi!
echo "exit: $?"                # last exit status
echo "PID: $$"                 # shell PID
echo "bg: $!"                  # last background PID
ls ~/projects                  # tilde expansion
ls *.cpp                       # glob expansion
```

### Job control
```bash
sleep 60 &                     # run in background → [1] 12345
jobs                           # list jobs
fg 1                           # bring job 1 to foreground
bg 1                           # resume stopped job in background
# Ctrl-Z suspends foreground job → Stopped
# Ctrl-C sends SIGINT to foreground job
```

### Builtins
```bash
cd ~/projects                  # change directory (OLDPWD tracked)
cd -                           # go back to previous directory
pwd                            # print working directory
echo -e "tab:\there"          # echo with escape sequences
export PATH="$PATH:/my/bin"    # export variable
unset VARIABLE                 # remove variable
type ls                        # show how command is resolved
alias ll='ls -la'              # define alias
history 20                     # show last 20 commands
!! ; !git ; !42                # history expansion
source ~/.minibashrc           # execute script in current shell
```

---

## Build & run

**Requirements:** C++17, CMake ≥ 3.16, libreadline

```bash
# Install readline (Ubuntu/Debian)
sudo apt-get install libreadline-dev

# Build
git clone https://github.com/YOUR_USERNAME/minibash
cd minibash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run interactive
./minibash

# Run a script
./minibash scripts/minibashrc.example

# Run a single command
./minibash -c 'echo "Hello from minibash"'
```

---

## Tests

```bash
cd build && ctest --output-on-failure
# or directly:
./run_tests
```

34 unit tests covering: lexer (10), parser (5), environment/expansion (9), history (6), integration (4).

Run with AddressSanitizer:
```bash
cmake -B build_debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build_debug --parallel
./build_debug/run_tests
```

---

## File structure

```
minibash/
├── include/
│   ├── common.h        # Token, SimpleCmd, Pipeline, CmdList AST types
│   ├── lexer.h         # Tokeniser interface
│   ├── parser.h        # Parser interface
│   ├── environment.h   # Variable store, $VAR expansion, PATH lookup, glob
│   ├── history.h       # Command history, !!, !N, !string expansion
│   ├── jobs.h          # Job table, fg/bg, SIGCHLD reaping
│   ├── builtins.h      # All builtin command declarations
│   ├── executor.h      # fork/exec engine, pipe chains, redirections
│   └── shell.h         # REPL, signal setup, prompt
├── src/
│   ├── main.cpp        # Entry point (-c / script / interactive)
│   ├── lexer.cpp       # Quote handling, escape sequences, operator recognition
│   ├── parser.cpp      # Recursive descent: pipeline → simplecmd → redirect
│   ├── environment.cpp # Variable expansion, glob (glob.h), Haversine PATH search
│   ├── history.cpp     # Ring buffer, save/load, ! expansion
│   ├── jobs.cpp        # waitpid loop, SIGCONT, tcsetpgrp, job list
│   ├── builtins.cpp    # cd, echo, export, alias, source, fg, bg, type, ...
│   ├── executor.cpp    # fork/exec, pipe setup, redirection, process groups
│   └── shell.cpp       # readline integration, SIGINT/SIGCHLD, .minibashrc
├── tests/
│   └── test_main.cpp   # 34 unit tests (no process spawning — pure logic tests)
├── scripts/
│   └── minibashrc.example  # Sample startup file with aliases
├── .github/workflows/
│   └── ci.yml          # Build + test + ASan + script smoke test
└── CMakeLists.txt
```

---

## What I learned

**`fork()` + `exec()` separation exists for a reason.** Between `fork()` and `exec()`, the child can set up file descriptors, process groups, and signal handlers — things that would be impossible if `fork` and `exec` were one call. This is where pipe setup happens: `dup2(pipe[1], STDOUT_FILENO)` replaces the child's stdout with the write end of the pipe.

**Pipe creation order is critical.** You must call `pipe()` *before* `fork()`. If you fork first and then try to create a pipe, the two processes can't share it — pipes are kernel objects inherited at fork time.

**Terminal job control is subtle.** When you Ctrl-Z a process, the kernel sends `SIGTSTP` to the *foreground process group* — not just the process. `tcsetpgrp()` is what decides which process group is "foreground". Getting this wrong means the shell itself gets suspended or the child never stops.

**`SIGCHLD` is asynchronous.** If you call `waitpid()` in the main loop and a child dies between checks, you get a zombie. If you do blocking `wait()` in the signal handler, you can deadlock. The correct pattern: set a flag or call `waitpid(WNOHANG)` in the handler.

**Builtins can't be forked.** `cd` changes the current directory of the *calling process*. If you fork before calling `chdir()`, the parent's directory is unchanged. Every shell developer hits this bug once.

---

## References

- W. Richard Stevens & Stephen Rago, *Advanced Programming in the Unix Environment* (APUE) — the definitive reference for everything in this project
- Michael Kerrisk, *The Linux Programming Interface* — Ch. 24 (process creation), Ch. 44 (pipes), Ch. 34 (job control)
- Bash source code — especially `jobs.c`, `execute_cmd.c`
- [Writing a Unix Shell](https://indradhanush.github.io/blog/writing-a-unix-shell-part-1/) — excellent 3-part walkthrough
- POSIX shell spec — [pubs.opengroup.org](https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html)
