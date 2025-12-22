# Ref: https://danyspin97.org/blog/makefiles-best-practices/
#
# People installing the regular way, unmanaged and into /usr/local, can leave
# PREFIX and BINDIR at their default values.
#
# OS Packagers can set DESTDIR and PREFIX, and leave BINDIR alone.
#
# Folks using Stow or other link farm managers can override BINDIR alone, or
# BINDIR and PREFIX.

PREFIX ?= /usr/local
BINDIR ?= ${PREFIX}/bin
SDL_CONFIG ?= sdl2-config

VERSION="1.2"
UNAME := $(shell uname)

# Any warning/error/info lines in the ifeq...endif block must not be indented,
# otherwise GNU Make 4.4.1 will get upset (tab-newline is a rule)
ifeq ($(UNAME),Darwin)
	CC ?= clang
	CFLAGS := ${CFLAGS} -Wall -Werror -Wextra -Wpedantic
	SDL_INCLUDE := $(shell ${SDL_CONFIG} --cflags)
	SDL_LIBS := $(shell ${SDL_CONFIG} --libs)
else ifeq ($(UNAME),Linux)
	CC ?= gcc
	CFLAGS := ${CFLAGS} -std=c99 -Wall -Werror -Wextra -Wno-format-truncation
	LIBS := ${LIBS} -lm
	SDL_INCLUDE := $(shell ${SDL_CONFIG} --cflags)
	SDL_LIBS := $(shell ${SDL_CONFIG} --libs)
else ifeq ($(findstring MINGW,$(UNAME)),MINGW)
	CC ?= gcc
	CFLAGS := ${CFLAGS} -Wall -Werror -Wextra -Wno-format-truncation
	WINDOWS_SDL_PATH=C:/mingw_dev_lib
	SDL_INCLUDE := -I$(WINDOWS_SDL_PATH)/include/SDL2
	SDL_LIBS=-L$(WINDOWS_SDL_PATH)/lib -lmingw32 -lSDL2main -lSDL2
else
$(warning No defaults for host OS "${UNAME}". Using generic fallbacks.)
$(warning Set CC/SDL_INCLUDE/SDL_LIBS/CFLAGS/LIBS if the build fails.)
	CC ?= gcc
	CFLAGS := ${CFLAGS} -std=c99 -Wall -Werror -Wextra
	SDL_INCLUDE := $(shell ${SDL_CONFIG} --cflags)
	SDL_LIBS := $(shell ${SDL_CONFIG} --libs)
endif

.PHONY: all build cli run ide clean \
 install install_user_rc uninstall \
 testcli testrun testregress

all: build

build: cli run ide

cli: build/octo-cli
run: build/octo-run
ide: build/octo-de

clean:
	@rm -rf build/ temp.ch temp.err

build/octo-cli: src/octo_cli.c
	@mkdir -p $(dir $@)
	@${CC} $< -o $@ ${CFLAGS} ${LIBS} -DVERSION="\"${VERSION}\""

build/octo-run: src/octo_run.c
	@mkdir -p $(dir $@)
	@${CC} $< -o $@ ${SDL_INCLUDE} ${SDL_LIBS} ${CFLAGS} ${LIBS} \
	    -DVERSION="\"${VERSION}\""

build/octo-de: src/octo_de.c
	@mkdir -p $(dir $@)
	@${CC} $< -o $@ ${SDL_INCLUDE} ${SDL_LIBS} ${CFLAGS} ${LIBS} \
	    -DVERSION="\"${VERSION}\""


install: build
	@cp build/octo-cli ${DESTDIR}${BINDIR}/octo-cli
	@cp build/octo-run ${DESTDIR}${BINDIR}/octo-run
	@cp build/octo-de  ${DESTDIR}${BINDIR}/octo-de
	@echo 'Installed successfully in "${DESTDIR}${BINDIR}".'
	@echo 'Consider copying ./octo.rc to ~/.octo.rc too.'
	@echo 'Running "make install_user_rc" will do that for the current user.'

install_user_rc:
	@test -f ~/.octo.rc && echo "~/.octo.rc already exists." || true
	@test -f ~/.octo.rc || ( echo "~/.octo.rc does not exist, creating." && cp -p octo.rc ~/.octo.rc )

uninstall:
	@rm -f ${DESTDIR}${BINDIR}/octo-cli
	@rm -f ${DESTDIR}${BINDIR}/octo-run
	@rm -f ${DESTDIR}${BINDIR}/octo-de
	@echo "Uninstalled successfully."


testcli: cli
	@./scripts/test_compiler.sh ./build/octo-cli
	@./scripts/test_cart.sh     ./build/octo-cli
	@rm -rf temp.ch8
	@rm -rf temp.err

# odds and ends:

testrun: run
	./build/octo-run carts/superneatboy.gif

testide: ide
	./build/octo-de

testregress:
	@./scripts/test_compiler.sh ../Octo/octo
	@rm -rf temp.ch8
	@rm -rf temp.err
