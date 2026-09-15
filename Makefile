# SENTINEL — the top-level Makefile.
#
#   make               build the given code, and each lab in src/ that has a Makefile
#   make test          run the tests of each lab in src/ that has a Makefile
#   make lab1          run your Lab 1 demo on a new feed, for the page
#   make accept1       check your Lab 1 library and demo against their contract
#   make watch         serve the page at http://localhost:8080/
#   make submit        build, test, and write the zip file you upload to Brightspace
#   make bench-sim     a simulated bench board on this computer, for the Bench view
#   make bench-health  the TA's bench board, for the Bench view: BENCH=tcp:<address>:4760
#   make clean         remove build/
#
# Each lab Makefile gets CC, CFLAGS and BUILD from here: see src/libsyscall/README.md.

CC      := gcc
CFLAGS  := -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -g -O1
LDFLAGS :=
BUILD   := $(abspath build)

export CC CFLAGS LDFLAGS BUILD

# The given code in common/ compiles without -Werror, so that a warning from a
# newer compiler cannot stop your build in code you did not write.
GIVEN_CFLAGS := -std=c11 -D_GNU_SOURCE -Wall -Wextra -g -O1

# The given programs come as binaries, one directory for each processor type.
# make copies the ones for this computer into build/.
ARCH := $(shell uname -m)
ifeq ($(ARCH),arm64)
ARCH := aarch64
endif
ifeq ($(ARCH),amd64)
ARCH := x86_64
endif
PROGRAMS := contactfeed benchd benchsim benchfeed
INSTALLED := $(addprefix $(BUILD)/,$(PROGRAMS))

# The labs, in build order. A lab builds only when its directory has a Makefile.
LABS    := src/libsyscall src/libproc src/libipc src/libsync src/sentinel
READY   := $(strip $(foreach d,$(LABS),$(if $(wildcard $(d)/Makefile),$(d))))
WAITING := $(filter-out $(READY),$(LABS))

# The run that the page shows, and the server for the page.
SRC     ?= $(BUILD)/live
# The sky the four sensors report. `world` is one world seen by all four, so
# coverage that overlaps reports the same object more than once. `dirty` adds
# damaged lines, and the Lab 1 manual asks you to run it.
PROFILE ?= world
BURST   ?= 250
PORT    ?= 8080
BIND    ?= 127.0.0.1
BENCH   ?= tcp:127.0.0.1:4760
SCENARIO ?= demo
WEB     := $(abspath web)
LIVE    := $(abspath $(SRC))
HOSTIP  := $(shell hostname -I 2>/dev/null | awk '{print $$1}')

.PHONY: all test lab1 accept1 watch submit bench-sim bench-health clean common programs $(LABS)

all: common programs $(READY)
	@for d in $(WAITING); do echo "  $$d: no Makefile yet"; done

common: | $(BUILD)
	$(MAKE) -C common CFLAGS="$(GIVEN_CFLAGS)"

programs: $(INSTALLED)

$(INSTALLED): $(BUILD)/%: tools/bin/$(ARCH)/% | $(BUILD)
	rm -f $@
	cp $< $@
	chmod +x $@

tools/bin/$(ARCH)/%:
	@echo "There are no given programs for this processor ($(ARCH)): tools/bin/ has aarch64 and x86_64."
	@exit 1

ifneq ($(READY),)
$(READY): common | $(BUILD)
	$(MAKE) -C $@
endif

# Each lab is built on the lab before it.
src/libproc:  $(filter src/libsyscall,$(READY))
src/libipc:   $(filter src/libproc,$(READY))
src/libsync:  $(filter src/libipc,$(READY))
src/sentinel: $(filter src/libsync,$(READY))

$(BUILD):
	mkdir -p $(BUILD)

test: all
	@if [ -z "$(READY)" ]; then echo "  no lab in src/ has a Makefile yet: nothing to test"; fi
	@for d in $(READY); do $(MAKE) -C $$d test || exit 1; done

# Lab 1: a feed from the four sensors, then your demo on it. The old mission
# log is removed first, because a new run adds its lines to the end of the file.
lab1: programs $(filter src/libsyscall,$(READY))
	@if [ ! -x "$(BUILD)/demo_lab1" ]; then \
		echo "build/demo_lab1 does not exist. Lab 1 builds it: see src/libsyscall/README.md"; \
		exit 1; \
	fi
	@mkdir -p "$(LIVE)"
	@rm -f "$(LIVE)/feed.txt" "$(LIVE)/missions.log"
	@for s in NWS AUR AIS SAT; do \
		$(BUILD)/contactfeed --sensor $$s --burst $(BURST) --profile $(PROFILE) \
			>> "$(LIVE)/feed.txt" || exit 1; \
	done
	$(BUILD)/demo_lab1 "$(LIVE)/feed.txt" "$(LIVE)/missions.log"
	@echo
	@echo "The page shows this run: make watch"

# Lab 1: the acceptance test. tools/bin/ holds it as a compiled object, and
# this links it with YOUR library, so every check calls your functions. Each
# check runs in its own process: a crash in one check does not stop the rest.
accept1: programs $(filter src/libsyscall,$(READY))
	@if [ ! -f "$(BUILD)/libsyscall.a" ]; then \
		echo "build/libsyscall.a does not exist. Lab 1 builds it: see src/libsyscall/README.md"; \
		exit 1; \
	fi
	$(CC) -o $(BUILD)/accept_lab1 tools/bin/$(ARCH)/accept_lab1.o \
		$(BUILD)/libsyscall.a $(BUILD)/libcommon.a -lm
	@$(BUILD)/accept_lab1 --demo $(BUILD)/demo_lab1

# The page reads files over HTTP. A page opened as a file cannot read them, so
# a small server is necessary. web/live is a link to the run directory.
#
# The server is tools/gen/serve.py and not python3 -m http.server, because the
# page asks for the part of the mission log it has not read yet. That is a
# Range request. http.server does not answer one, so it sends the whole log
# again on every poll, and a long run makes the page slower as it grows.
watch:
	@mkdir -p "$(LIVE)"
	@if [ -L "$(WEB)/live" ]; then rm -f "$(WEB)/live"; \
	elif [ -e "$(WEB)/live" ]; then \
		echo "web/live exists and is not a link. Move it away, then run make watch again."; \
		exit 1; \
	fi
	@ln -s "$(LIVE)" "$(WEB)/live"
	@echo
	@echo "  SENTINEL page"
	@echo "  ----------------------------------------------------------"
ifeq ($(BIND),127.0.0.1)
	@echo "  open        http://localhost:$(PORT)/"
	@if [ -n "$$SSH_CONNECTION" ]; then \
		echo; \
		echo "  YOU ARE ON SSH, so that address is YOUR computer, not this"; \
		echo "  one, and nothing listens there. Do one of these:"; \
		echo; \
		echo "    forward the port, from your own computer:"; \
		echo "      ssh -L $(PORT):localhost:$(PORT) $$USER@$(HOSTIP)"; \
		echo "      then open  http://localhost:$(PORT)/"; \
		echo; \
		echo "    or publish the page on the network:"; \
		echo "      make watch BIND=0.0.0.0"; \
		echo "      then open  http://$(HOSTIP):$(PORT)/"; \
		echo; \
	fi
else
	@echo "  open        http://$(HOSTIP):$(PORT)/"
	@echo "  everyone on this network can read it. Press Ctrl-C when you finish."
endif
	@echo "  run data    $(LIVE)"
	@if [ ! -f "$(LIVE)/missions.log" ]; then \
		echo "  NO DATA YET, so the views are empty. In another terminal, run: make lab1"; \
	else \
		echo "  log         $$(wc -l < "$(LIVE)/missions.log") lines"; \
	fi
	@echo "  stop        Ctrl-C"
	@echo
	@python3 tools/gen/serve.py --port $(PORT) --bind $(BIND) --directory "$(WEB)"

# The bench on the page. Each target runs until you press Ctrl-C. Run make watch
# in a second terminal, and open the Bench view.
bench-health: programs
	@mkdir -p "$(LIVE)"
	$(BUILD)/benchfeed --health "$(LIVE)/bench.json" --source "$(BENCH)"

bench-sim: programs
	@mkdir -p "$(LIVE)"
	@$(BUILD)/benchsim --link "$(BUILD)/board" --scenario $(SCENARIO) & sim=$$!; \
	trap 'kill $$sim 2>/dev/null' EXIT INT TERM; \
	sleep 0.5; \
	echo "a simulated board ($(SCENARIO)), read by benchd: the Bench view reads $(LIVE)/bench.json"; \
	$(BUILD)/benchd --device "$(BUILD)/board" --status "$(LIVE)/bench.json"

# --- the hand-in -----------------------------------------------------------
#
# You hand in a zip file on Brightspace. This target builds it.
#
# It builds and tests first, on purpose. A submission that does not compile
# cannot be marked, and the most common cause is a file that was never added
# to the Makefile: it builds on your machine, where the object file is still
# in build/ from an earlier run, and nowhere else. `make clean` first is what
# finds that.
#
# The zip holds your SOURCE, not what you built. build/ is left out because
# the marker builds your code again from the files in the zip. A prebuilt
# library in a zip would be marked without ever being compiled from the source
# beside it, and the two can differ.
#
# Everything else stays in, including tools/bin. The zip is then complete on
# its own: the marker unzips it, runs make, and every command in this file
# works. Object files are excluded by the build/ rule above, so the one
# object that survives is tools/bin/<arch>/accept_lab1.o, which make accept1
# needs.
NAME ?= $(shell id -un)
ZIP  := sentinel-lab1-$(NAME).zip

submit:
	@command -v zip >/dev/null 2>&1 || { \
		echo "zip is not installed. Install it with: sudo apt install zip"; exit 1; }
	@echo "==> building from clean, so that nothing is missing from your Makefile"
	@$(MAKE) --no-print-directory clean
	@$(MAKE) --no-print-directory all
	@echo
	@echo "==> running your tests"
	@$(MAKE) --no-print-directory test
	@echo
	@echo "==> running the acceptance test"
	@$(MAKE) --no-print-directory accept1
	@echo
	@for f in demo_output.txt missions.log proc_capture.txt; do \
		if [ ! -s "docs/lab1/$$f" ]; then \
			echo "docs/lab1/$$f is missing or empty."; \
			echo "Section 9 of src/libsyscall/README.md gives the commands that make it."; \
			exit 1; \
		fi; \
	done
	@rm -f "$(ZIP)"
	@zip -q -r "$(ZIP)" . \
		-x '*/build/*' 'build/*' '.git/*' '*/.git/*' 'web/live' \
		   '*.a' '*.d' 'core' 'vgcore.*' '.vscode/*' '*.zip' \
		-i 'Makefile' 'README.md' '.gitignore' 'common/*' 'docs/*' 'src/*' \
		   'tools/*' 'web/*'
	@echo
	@echo "  $(ZIP)"
	@echo "  $$(unzip -l "$(ZIP)" | tail -1 | awk '{print $$2}') files, $$(du -h "$(ZIP)" | cut -f1)"
	@echo
	@echo "  Upload this file to Brightspace before the deadline."
	@echo "  Set your name in the file name with: make submit NAME=<your name>"


clean:
	rm -rf $(BUILD)
	@if [ -L "$(WEB)/live" ]; then rm -f "$(WEB)/live"; fi
