# HW4: Whack-a-Mole and Race Conditions

This program uses the HW1 deque to pass moles between producer and consumer
threads. It creates one producer and one consumer thread per mole. The default
simulation creates ten moles and uses a queue capacity of four.

## Build and run

Run these commands from `hw4`:

```sh
make
./wam                  # Graphical simulation with 10 moles, capacity 4
./wam 20 1             # 20 moles, capacity 1
DISPLAY= ./wam 20 0    # Text mode; capacity 0 is unbounded
```

Building requires GCC/G++, GNU make, pthreads, FLTK development files, and X11.
Text mode still links against FLTK, as required by the assignment. On Ubuntu
or Debian, the system development packages can be installed with:

```sh
sudo apt update
sudo apt install libfltk1.3-dev libx11-dev
```

`FL/Fl.H: No such file or directory` means those headers are missing or their
include path has not been supplied. Clearing `DISPLAY` does not fix a missing
build dependency. On onyx, use its installed FLTK development environment.

The build first uses `fltk-config` when available. This local machine also has
a persistent user-local FLTK installation at
`~/.local/opt/cs453-hw4-fltk`, which the makefile detects automatically when
`fltk-config` is absent. It does not depend on files in `/tmp` or require sudo.
`LOCAL_FLTK` can override that directory. With neither installation detected,
the build uses the standard header paths and `-lfltk -lX11`.

`FLTK_CXXFLAGS` and `FLTK_LIBS` can be overridden for any custom installation.
Use `-isystem /path/to/include` for external FLTK headers if an older FLTK
release produces warnings with a newer compiler.

The optional command-line arguments are nonnegative integer mole count and
queue capacity. A count of zero is supported and shuts down cleanly. With a
display, the window closes automatically after every worker has finished.
Closing it early is ignored because this simulation is noninteractive and
workers may still be using its widgets.

Each normal simulation tick lasts 1000 milliseconds. For quick tests:

```sh
DISPLAY= WAM_TICK_MS=1 ./wam 30 1
```

`WAM_TICK_MS` accepts integers from 0 through 60000. Normal runs retain the
starter's random lifetimes of one through five ticks for creation, whacking,
and expiration. Timing does not enforce queue synchronization.

## Modules and synchronization

- `deq.c` and `deq.h` are exact copies of your completed HW1 files. The existing
  deque test is also copied into `tests/test_deq.c`. The deque itself remains
  unaware of threads.
- `mtq.c` wraps one deque with one mutex and two condition variables.
  `not_empty` wakes waiting consumers; `not_full` wakes waiting producers.
  Every read or modification of the underlying deque takes that queue's lock.
- `threads.c` provides `threads_start(n, function, arg)` and
  `threads_join(group)`. Each of the n new threads calls the supplied function
  once. Joining waits for all threads and frees the group. An argument must
  remain valid until its group is joined.
- `main.c` starts a coordinator that launches both worker groups before joining
  either. A producer creates one mole and puts it at the queue's tail. A consumer
  gets one mole from its head and whacks it. Producers and consumers exchange
  moles only through `Mtq`; mole work happens outside the queue lock.
- `pthread_check.h` reports pthread failures using their returned error numbers,
  rather than `errno`.

Queue put/get waits use **while loops** to recheck the predicate after waking.
`pthread_cond_wait` releases the queue mutex while blocked and reacquires it
before returning. Checking the predicate and changing the queue under the same
mutex prevents lost wakeups and protects against spurious wakeups. See the
[POSIX condition-variable documentation](https://man7.org/linux/man-pages/man3/pthread_cond_wait.3p.html).

The wrapper exposes the deque's head/tail put, get, ith, rem, map, str, len, and
delete operations. Puts wait for available capacity and gets wait for an item.
`ith` and `rem` remain nonblocking when no match exists. Capacity zero removes
the upper bound. NULL data pointers are supported. Callbacks for map/str execute
under the lock and must not call back into the same queue. Length/index lookups
are snapshots; callers must coordinate the lifetime of data observed by `ith`.

The main thread owns the `Simulation` argument, queue, and lawn until the
coordinator finishes. After both worker groups are joined, the coordinator asks
the graphics loop to stop. Main then joins the coordinator, deletes the empty
queue, and frees the lawn. No mutex, condition variable, or shared argument is
destroyed while a worker can still use it. Deleting a queue with remaining data
can use a callback, such as `mtq_del(q, free)`, once all its users have finished.
The wrapper does not provide cancellation or an end-of-stream operation; this
simulation uses equal, known producer and consumer counts.

## Graphics changes

The supplied files ran FLTK in a background thread, unlike the main-thread
graphics model shown in the PDF. `lawn_new` now initializes the window without
starting a graphics thread. New `lawn_run` and `lawn_stop` functions let main
run the event loop and the coordinator request shutdown.

FLTK locking is initialized before showing the window. Workers briefly lock
FLTK while changing widgets, then wake the main thread to redraw. Event handling
and window creation/destruction stay on the main thread. Shutdown uses an awake
callback to hide the window after workers finish; main subsequently deletes it.
Widget removal/deletion is protected by the same FLTK lock as other updates.
The delays use `nanosleep`, with retry on interruption, rather than allocating
a temporary mutex and condition variable for each delay. Text output uses a
separate short-lived critical section so entire event lines are serialized;
the logging mutex is never held across a delay or a queue operation. See
[FLTK's threading documentation](https://www.fltk.org/doc-1.3/advanced.html).

## Verification

```sh
make test         # Original deque, wrapper/thread tests, and text simulation
make valgrind     # Leak and memory checks on tests and the real simulation
make racecheck    # Valgrind DRD race checks on wrapper tests and simulation
make sanitize     # Address/undefined-behavior/leak sanitizers on unit tests
make clean        # Remove the executable and generated build files
```

`make unit` builds/runs the deque and MTQ/thread tests without FLTK. Full
simulation tests additionally require Python 3. Tests exercise:

- All deque-wrapper operations, NULL payloads, callbacks, and remaining-data
  cleanup.
- Empty-queue blocking at both ends and full-queue blocking/wakeup after gets
  and removals, including removal of a stored NULL.
- Four producers and four consumers transferring 12,000 items across queues
  with capacities 1, 4, and 0. Every item must arrive exactly once.
- Concurrent execution of all threads in a group, correct argument delivery,
  joining, and zero-thread groups.
- End-to-end creation, whacking, and expiration counts for zero/one/many moles,
  bounded/unbounded queues, and invalid command-line arguments.

The tests have timeouts so a deadlock fails the run. Text simulations use short
ticks; no test depends on a random number of seconds passing for correctness.

`./vg [moles [capacity]]` runs the application under Valgrind using the course's
`fltk.supp`, copied from `CS453-resources/.../pub/hw4.vg`. These suppressions apply
to reachable library allocations, not lost application allocations. For example:

```sh
DISPLAY= WAM_TICK_MS=1 ./vg 30 1
```

Local validation passed for unit/integration tests, ASan/UBSan with leak checking,
Valgrind memory checking, and DRD. Graphical smoke tests passed with zero and ten
moles, including capacities one and four. Graphical Valgrind checking also
reported zero memory errors and zero lost blocks; some FLTK/dynamic-loader global
allocations remain reachable at process exit. FLTK is supplied by a persistent
user-local installation on this machine; no system packages were installed.
Onyx verification is left to you. Rebuild on onyx with `make clean && make`.

This environment's Valgrind 3.22 Helgrind reports unlocked internal signals from
glibc 2.39's `__pthread_cond_wait_common`, even though the wrapper's own signals
hold the mutex. `racecheck` therefore uses DRD. Its checks pass without custom
race suppressions; the unsuppressed Helgrind report is not counted as a passing
check. LeakSanitizer also requires running outside a debugger/tracing sandbox.
