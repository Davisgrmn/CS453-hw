# HW3 Unix shell

Implements the core assignment: foreground commands, sequences, background
commands, pipelines, redirection, and the `exit`, `pwd`, `cd`, and `history`
builtins.

## Build and run

```sh
make
./shell
```

All project sources are inside this directory. `deq.c` is an unchanged copy of
my HW1 implementation.

Requires GCC, GNU Make, and the GNU Readline/ncurses development libraries.
The makefile uses the active `CONDA_PREFIX` when available; otherwise it uses
system headers and libraries. 

## Supported input

The provided scanner splits tokens on whitespace. Put spaces around operators:

```text
cd /etc ; pwd
seq 1 100000 | cat | wc -l
cat < input > output
pwd > directory
sleep 1 & echo ready
history | cat
exit
```

Redirection follows the assignment grammar: optional `< filename`, followed by
optional `> filename`. Output files are created or truncated. Explicit
redirection takes precedence over a pipeline connection.

Builtins run inside the shell only when they are standalone foreground commands.
Inside a pipeline or in the background, they run in children. `cd -` switches to
the previous directory without printing it. Failed `cd` commands preserve that
previous directory. `history` prints numbered commands, including the current
history command. Interactive history is loaded from and saved to `.history` in
the directory where the shell started.

Background commands default to `/dev/null` for standard input, unless redirected
explicitly. Completed jobs are reaped between commands and while waiting at an
interactive prompt. Both `exit` and end of input wait for remaining jobs.

This implements the supplied grammar; it does not perform Bash quoting,
variable expansion, wildcard expansion, or additional redirection operators.

## Validation

```sh
make test             # required Test/run regression runner, 27 cases
make test-processes   # 5 interactive checks; requires Linux /proc and Python 3
make test-valgrind    # regression suite under Valgrind
./vg                 # interactive Valgrind session
```

`Test/run` creates an isolated temporary working directory for each case and
checks stdout, stderr, successful shell exit, and any expected files. Failures
produce differences and a nonzero status. `@ROOT@` and `@WORK@` placeholders
make fixtures portable. Each case has `inp` and `exp`, optionally `err`, a
`files/` directory of expected file contents, or a Bash `check` script.

Tests cover foreground waiting, all pipeline children being waited for, large
pipelines, early consumer exit, builtin process isolation, background execution
and shutdown, redirection restoration, command history, and recovery from
syntax/command errors. Interactive checks also cover signals, idle job reaping,
history persistence, and file descriptor cleanup. Valgrind uses the supplied
Readline suppressions and reports invalid memory accesses and lost allocations;
the suite silences reports from forked children before exec. Use `./vg` to
inspect those children too. Submission still needs evaluation on Onyx.

## Modules

- `Shell.c`: input loop, history persistence, signals, and final cleanup.
- `Scanner.c`: the starter whitespace token scanner.
- `Parser.c` / `Tree.*`: syntax validation and parse-tree ownership.
- `Interpreter.c`: conversion from parse trees to executable objects.
- `Sequence.c`: ordered execution and cleanup of unexecuted pipelines.
- `Pipeline.c`: pipes, child creation, descriptor setup, and process waiting.
- `Command.c`: arguments, redirection, builtins, and external command execution.
- `Jobs.c`: ownership and reaping of background pipelines.
- `deq.c`: the local HW1 deque implementation used by sequences and jobs.
