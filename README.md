# SENTINEL

**Sensor Network for Tracking, Integration, and Event Logging**

SENTINEL is the software of a surveillance operations centre. Four sensors watch the northern
and Pacific approaches of Canada. Each sensor reports the contacts that it sees. SENTINEL
collects the reports through the operating system and records them. It decides which reports
matter most, and it keeps an accurate picture for the watch officers.

You build SENTINEL in this repository, for ECE 476 Engineering System Software (Fall 2026) at
the University of Victoria. You write it in C, for Linux, one library in each lab.

---

## The four sensors

| Sensor | Location | What it reports | Rate | Strength |
|---|---|---|---|---|
| North Warning System radar (`NWS`) | Cambridge Bay, NU | Aircraft position, altitude, speed | ~20 / s | Constant watch |
| CP-140 Aurora patrol aircraft (`AUR`) | Beaufort Sea | Contacts it finds and identifies up close | ~5 / s | Identifies |
| Coastal radar and AIS receiver (`AIS`) | Juan de Fuca Strait | Vessel positions, and the identity each ship broadcasts | ~50 / s | Names ships |
| RADARSAT satellite (`SAT`) | Polar orbit | A full contact list for a wide area, once per pass | ~2000 / burst | Wide area |

No single sensor is enough. The software that combines them is the point.

---

## The four labs

SENTINEL is four C libraries. Each library is a layer on the library before it.

The date below is the lab session. **The deliverable of each lab is due later, and the README
of that lab states the date.** Lab 1 has its session on October 9 and its deliverable due on
October 23.

**Each lab has a manual, and it is the lab document.** Lab 1's is
[`docs/lab1/lab1_manual.pdf`](docs/lab1/lab1_manual.pdf). It is complete on its own: read it
before you start. This file describes the repository the labs are built in.

| Lab | Lab session | Library | Directory | What it adds |
|---|---|---|---|---|
| 1 | Oct 9 | `libsyscall` | [`src/libsyscall`](src/libsyscall/README.md) | Talks to the operating system: feeds, the mission log, precise timestamps |
| 2 | Oct 30 | `libproc` | [`src/libproc`](src/libproc/README.md) | One process for each sensor, and the channels that bring their reports to the centre |
| 3 | Nov 20 | `libipc` | [`src/libipc`](src/libipc/README.md) | Worker threads that share one processor and keep the track picture |
| 4 | Dec 4 | `libsync` | [`src/libsync`](src/libsync/README.md) | Protection for everything the workers share |

In Lab 4, [`src/sentinel`](src/sentinel/README.md) also starts all four layers with one
command.

The README in each lab directory holds the deliverable of that lab. It is released before the
lab.

---

## The six requirements

The final system must meet all six. The page shows them by their IDs.

| ID | Requirement | Lab |
|---|---|---|
| R1 | Four sensors run at very different rates, and any sensor can stop with no warning | 2 |
| R2 | An urgent contact must overtake routine traffic | 2 |
| R3 | Thousands of contacts can arrive in one burst | 3 |
| R4 | Many workers write the same shared record, and must not corrupt it | 4 |
| R5 | The centre must never freeze while it still looks alive | 4 |
| R6 | Every action is timestamped and written to the mission log | 1 |

---

## The operations centre runs on ONE processor

This is a design decision of the system, and it changes what Part 3 means.

A **thread** is a separate path through your program, and the operating system decides which
thread runs. With one processor, **exactly one thread runs at a time.** The operating system
swaps them every few milliseconds, and each swap is a context switch.

So worker threads do **not** add capacity on one processor. They add overlap:

| Term | Meaning | Processors needed |
|---|---|---|
| Concurrency | Many tasks are in progress together | One is enough |
| Parallelism | Many tasks execute at the same instant | Many |

Eight workers on one processor still give you one processor of work each second. Eighty give
the same. That is the result Part 3 exists to show, and the page prints the number that proves
it: **the share of a processor the whole pool was given, which on one processor cannot pass
1.00.**

Then why use threads at all? Two reasons, and both survive on one processor.

1. A worker is not always computing. It waits for a lock, for a file, for the queue. While one
   worker waits, another can use the processor. Part 4 is about making that wait cheap.
2. An urgent contact can be picked up while routine work is still in progress. It does not have
   to wait for the routine report to finish.

**The two calls this rests on are given to you**, in `common/cpu.h`: `cpu_thread_ns`, which
counts the processor time a thread was actually given, and `cpu_pin`, which restricts a
program to a set of processors. Neither is a lab deliverable, because neither is something
SENTINEL needs in order to work: the course uses them to create the one-processor condition
and to measure what a run cost. Read them when you reach Lab 3, which is where the difference
between elapsed time and processor time starts to mean something.

---

## The contact report

One report is one line of text:

```
<time> <sensor> <contact> <lat> <lon> <speed> <flag>
1755534061.482913 NWS ACCF-4587 68.7100 -105.7000 420.0 P
1755534061.501244 AIS VESL-0912 48.3000 -123.4000 12.0 R
```

| Field | Format |
|---|---|
| time | seconds and microseconds since the epoch, with exactly 6 digits after the point |
| sensor | `NWS`, `AUR`, `AIS` or `SAT` |
| contact | the identity of one aircraft or one vessel, up to 15 characters |
| lat, lon | degrees. A `-` is south or west |
| speed | knots |
| flag | `P` for priority, `R` for routine |

A line is at most 128 bytes, including its `\n`. Do not change this limit.

---

## What is in this repository

```
sentinel-lab/
├── Makefile        builds everything; see "Build"
├── src/            YOUR CODE: one directory for each lab
│   ├── libsyscall/     Lab 1
│   ├── libproc/        Lab 2
│   ├── libipc/         Lab 3
│   ├── libsync/        Lab 4
│   └── sentinel/       Lab 4: the complete system
├── docs/           YOUR RECORDS: the defects log, and the files you hand in
├── common/         given: the contact record (common/contact.h)
├── tools/          given: contactfeed and the bench programs, as binaries
├── web/            given: the page
└── build/          what make builds (git ignores it)
```

- Everything that you write goes into `src/`. Your records go into `docs/`.
- `common/`, `tools/` and `web/` are given. Do not change them: an update replaces these files.
- The given programs are binaries for Linux, in `tools/bin/aarch64/` and `tools/bin/x86_64/`.
  `make` copies the ones for your computer into `build/`.
- `src/libsyscall/include/sentinel/syscall.h` is also given. Each lab README names its given
  files.

---

## Before you start

You need a Linux system on an aarch64 or x86_64 processor. A virtual machine is fine: a Mac
with Apple silicon runs aarch64 Linux, and most PCs and WSL2 run x86_64 Linux. `uname -m`
tells you which one you have. Install the tools:

```sh
sudo apt install build-essential python3 valgrind git
```

---

## Your own copy of this repository

You do not work in the course repository. You make your own copy, you keep it **private**, and
you keep it for all four labs. Each lab then arrives in your copy with one command.

You keep it private because it holds your own solutions. **You do not hand in the repository.**
You hand in a zip file on Brightspace, and `make submit` builds that zip for you.

### 1. Make your copy, once

| Your host | What you do |
|---|---|
| GitHub | Open the course repository. Press **Use this template**, then **Create a new repository**. Set it to **Private**. |
| GitLab | Open the course repository. Press **Fork**. Set the visibility to **Private**. |

On GitHub, do **not** press Fork. A fork of a public repository is public, and GitHub cannot
make a fork private. Your solution must not be public.

### 2. Point your copy at the course, once

```sh
git clone <your repository>
cd sentinel-lab
git remote add upstream <the course repository>
git remote -v
```

`origin` is your copy. `upstream` is the course.

### 3. Take each lab when the course releases it

```sh
git pull upstream main
```

This brings the README of the new lab, the files that the lab gives you, and every correction
to the given code. Do this before you start each lab, and again if the course announces a fix.

### Who owns which files

| Files | Who changes them |
|---|---|
| `src/<lab>/src`, `src/<lab>/include`, `src/<lab>/tests` | you |
| `docs/` | you |
| `common/`, `tools/`, `web/`, the top `Makefile`, every `README.md` | the course |

Keep to this table, and `git pull upstream main` is always clean. If you change a file that
the course owns, the pull stops and reports a conflict. Take the course's copy of that file:

```sh
git checkout --theirs -- <the file>
git add <the file>
git commit
```

The given programs in `tools/bin/` are binaries. A new release replaces them, and Git cannot
merge a binary. Take the course's copy in the same way.

---

## Build

```sh
make          # build the given code and each lab in src/ that has a Makefile, and copy the given programs
make test     # run the tests of each lab that has a Makefile
make accept1  # check your Lab 1 against its contract: see src/libsyscall/README.md
make submit   # build, test, and write the zip file you upload to Brightspace
make clean    # remove build/
```

- `make` puts everything into `build/`.
- Your code compiles with `-Wall -Wextra -Werror`, so a warning stops the build.
- The given code compiles without `-Werror`.

---

## The given programs

`make` copies four given programs into `build/`: `contactfeed`, `benchfeed`, `benchsim` and
`benchd`. Run each one with `--help` to see its usage.

### contactfeed

`contactfeed` stands in for the real sensors. It writes contact reports on its standard output.

```sh
build/contactfeed --sensor NWS --rate 20              # 20 reports each second, until Ctrl-C
build/contactfeed --sensor SAT --burst 2000           # 2000 reports at once, then stop
build/contactfeed --sensor AIS --count 50             # 50 reports at the rate in Table 1
build/contactfeed --sensor AUR --burst 20 --seed 42   # the same 20 contacts each time
build/contactfeed --sensor NWS --burst 500 > feed.txt # a feed file
```

| Option | What it does |
|---|---|
| `--sensor NWS\|AUR\|AIS\|SAT` | the sensor. This option is necessary |
| `--rate R` | R reports each second. With no `--count`, it does not stop |
| `--burst N` | N reports as fast as possible, then stop |
| `--count N` | stop after N reports |
| `--priority-every K` | every Kth report has the flag `P`. The default is 10 |
| `--seed S` | the same seed gives the same contacts |
| `--profile NAME` | `ideal`, `dirty`, `field` or `threat`. See the next table |
| `--damage-every N` | with `dirty`, every Nth line is damaged. The default is 50 |

With no `--rate` and no `--burst`, the sensor uses its rate from the table of sensors.

| Profile | The feed |
|---|---|
| `ideal` | The default. Positions are random inside the area of the sensor. Every line is valid |
| `dirty` | The same, but some lines are damaged: cut short, too long, with a bad number, or empty |
| `field` | Contacts that move: the next report about a contact is where the contact has moved to |
| `threat` | `field`, flown by traffic that filed a flight plan, plus a few contacts that filed nothing. The feed never says which is which: a line from one of them is an ordinary line. Your centre decides, against `common/traffic.txt` |

| Environment variable | What it does |
|---|---|
| `SENTINEL_PROFILE` | the same as `--profile`. The command-line option wins |
| `SENTINEL_SEED` | any text. It selects your own repeatable set of contacts |

A seed fixes the contacts, not the time. Each line gets its timestamp from the clock.

### The bench programs

The bench is a map box with four real sensors, on one board that the TA runs. The bench
programs bring its reports to your computer, or simulate the board when there is no board.

| Program | What it does |
|---|---|
| `benchfeed --sensor NWS\|AUR\|AIS\|SAT [--burst N]` | writes the bench contact reports of one sensor on its standard output, like `contactfeed`. It accepts the other options of `contactfeed` and ignores them |
| `benchfeed --health PATH` | writes the state of the bench to `PATH` once each second, for the Bench view |
| `benchfeed --pty PATH` | makes a pseudo-terminal at `PATH` that carries every line of the bench |
| `benchsim --link PATH [--scenario NAME]` | a simulated board behind a pseudo-terminal at `PATH`. The scenarios are `empty`, `target`, `strait`, `flood` and `demo` |
| `benchd --device PATH [--status PATH] [--listen PORT]` | reads a board, writes its state for the Bench view, and sends its lines to each `benchfeed` that connects (port 4760 by default) |

`benchfeed` gets its source from `--source`, or from the environment variable
`SENTINEL_BENCH`:

| Source | Where the lines come from |
|---|---|
| `tcp:HOST:PORT` | a `benchd`: the TA's bench, or your own `make bench-sim` at `tcp:127.0.0.1:4760` |
| `file:PATH` | a recorded capture, replayed at the speed it was recorded |

`benchfeed --sensor` ends with status 0 when its reader stops, 2 when the source is wrong,
and 3 when no heartbeat arrives for 3 seconds, because the board is gone.

---

## The contact record: `common/contact.h`

| Name | What it does |
|---|---|
| `contact_t` | one report: `t_sec`, `t_usec`, `sensor`, `id`, `lat`, `lon`, `speed`, `flag` |
| `contact_parse(line, len, &c)` | parses one line, with or without its `\n`. Returns `CONTACT_OK`, or the error that it found. It changes `c` only when it returns `CONTACT_OK` |
| `contact_strerror(e)` | the text for an error. It never returns `NULL` |
| `contact_format(&c, buf, buflen)` | writes a report as one line with its `\n`. Returns the length, or -1 |
| `sensor_name(id)` | `"NWS"` for `SENSOR_NWS`, or `NULL` for an unknown id |
| `sensor_rate(id)` | the rate of the sensor, or the burst size for `SAT`. -1 for an unknown id |
| `CONTACT_LINE_MAX` | 128: the maximum length of a line, including its `\n` |

---

## The page

The page shows the operations centre from the files that your programs write. It only reads
files, and it changes nothing. Section 5 of the lab document says that a display is not a
course requirement.

```sh
make lab1     # run your Lab 1 demo on a new feed
make watch    # serve the page
```

Open <http://localhost:8080/>. Press Ctrl-C to stop the server. For the Bench view, run
`make bench-sim` (a simulated board) or `make bench-health BENCH=tcp:<address>:4760` (the TA's
bench) in another terminal. When you work over SSH,
`make watch` tells you what to do. [`web/README.md`](web/README.md) has more.

---

## How you work

- Commit often. A commit you can go back to is worth more than an editor's undo.
- Run `git pull upstream main` before each lab, and keep the given files as the course sent
  them. The marker compares your tree with the released one, and a changed given file is a
  question you will be asked to answer.
- Record each error in `docs/DEFECTS.md` when you meet it: what you saw, the cause, and the
  fix. It is not marked. It is the fastest way to answer a question about your own code.
- At each check, you say what a command will print before you run it. You also say what one
  changed line of your own code will do.
- You hand in a zip file on Brightspace. `make submit` builds it. The README of each lab gives
  the steps.

---

## Credit

The project brief and the scenario come from the ECE 476 lab documentation.
