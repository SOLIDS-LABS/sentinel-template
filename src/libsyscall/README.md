# Lab 1 — `libsyscall`: the operating system interface

**Lab session: October 9, 2026. Deliverable due: October 23, 2026.**

## Read the Lab Manual first

**`docs/lab1/lab1_manual.pdf` is the lab document.** It is complete on its own: the operations
centre you are building for, what each function is for, where every given file is, how to
build and run everything, what the display shows, how the lab is marked, and how to submit.

This file is the quick reference you keep open while you work. Everything in it is explained
in the manual.

---

## What you write

Four source files and a `Makefile`, in this directory. `src/` and `tests/` are empty now, and
filling them is the lab.

| Path | What it holds |
|---|---|
| `Makefile` | builds `libsyscall.a`, `demo_lab1`, and a `test` target |
| `src/io.c` | `sc_open_read` `sc_open_write` `sc_open_write_mode` `sc_close` `sc_write_all` `sc_read_full` |
| `src/clock.c` | `sc_now_ns` `sc_now_wall` `sc_stamp` |
| `src/log.c` | `log_open` `log_writef` `log_close` `log_level_name` |
| `src/proc.c` | `proc_report` `proc_report_write` |
| `tests/test_*.c` | one test program for each source file |
| `tests/demo_lab1.c` | the demonstration program |

Fifteen functions. `include/sentinel/syscall.h` declares all of them and states the contract
of each one. **It is given. Do not change a declaration in it**, because tests that are not in
this repository call your functions exactly as it declares them.

## What you are given

| Path | What it gives you |
|---|---|
| `include/sentinel/syscall.h` | the interface you implement |
| `../../common/contact.h` | the contact record, and `contact_parse` |
| `../../common/linereader.h` | `lr_open` `lr_open_follow` `lr_next_line` `lr_close` |
| `../../common/cpu.h` | the processor instruments |
| `../../common/device.h` | `device_open`, for a serial device |
| `../../build/contactfeed` | the feed generator, which `make` copies into `build/` |

Read `../../common/linereader.c` before you write the `--follow` loop in your demo. The reader
is given, but your demo is its caller, and the caller is the difficult part. Section 6.5 of
the manual explains it.

## Commands

From the top of the repository:

```sh
make          # build the given code and your library
make test     # run your tests
make accept1  # 34 checks against your library, and your demo as a black box
make lab1     # run your demo on a new feed, for the display
make watch    # serve the display at http://localhost:8080/
make submit   # build, test, and write the zip you upload to Brightspace
make clean    # remove build/
```

## Build rules

- Compile with `-Iinclude -I../../common`.
- Link with `$(BUILD)/libsyscall.a $(BUILD)/libcommon.a -lm`, in that order.
- Take `CC`, `CFLAGS` and `BUILD` from the top-level `Makefile`, each with `?=`.
- A warning stops the build. Do not remove `-Werror`.

## Done when

- [ ] `make` builds your library and your demo with no warning.
- [ ] `make test` passes.
- [ ] `make accept1` passes all 34 checks.
- [ ] `make lab1` and `make lab1 PROFILE=dirty` both complete.
- [ ] The demo prints `(balanced)`.
- [ ] `--follow` ends because the feed went quiet, and not because the reader reported
      `EAGAIN`.
- [ ] `valgrind --leak-check=full --error-exitcode=1 build/demo_lab1 build/live/feed.txt /tmp/check.log`
      reports no leak and no error.

## What you hand in

One zip file on Brightspace. `make submit` builds it, after you have produced the three files
of one run in `docs/lab1/`. **Section 9 of the manual gives the exact commands.**
