
DESTDIR=""
PREFIX="/usr/local"
VERSION="1.2"
INSTALLDIR=$(DESTDIR)$(PREFIX)/bin/
SDL=$(shell sdl2-config --cflags --libs)
UNAME=$(shell uname)

ifeq ($(UNAME),Darwin)
	COMPILER=clang
	FLAGS=-Wall -Werror -Wextra -Wpedantic
endif
ifeq ($(UNAME),Linux)
	COMPILER=gcc
	FLAGS=-std=c99 -lm -Wall -Werror -Wextra -Wno-format-truncation
endif
ifeq ($(findstring MINGW,$(UNAME)),MINGW)
	COMPILER=gcc
	FLAGS=-Wall -Werror -Wextra -Wno-format-truncation
	WINDOWS_SDL_PATH=C:/mingw_dev_lib
	SDL=-I$(WINDOWS_SDL_PATH)/include/SDL2 -L$(WINDOWS_SDL_PATH)/lib -lmingw32 -lSDL2main -lSDL2
endif
ifndef COMPILER
	$(error No configuration for host OS...)
endif

all: cli run ide

clean:
	@rm -rf build/

cli:
	@mkdir -p build
	@$(COMPILER) src/octo_cli.c -o build/octo-cli $(FLAGS) -DVERSION="\"$(VERSION)\""

run:
	@mkdir -p build
	@$(COMPILER) src/octo_run.c -o build/octo-run $(SDL) $(FLAGS) -DVERSION="\"$(VERSION)\""

ide:
	@mkdir -p build
	@$(COMPILER) src/octo_de.c -o build/octo-de $(SDL) $(FLAGS) -DVERSION="\"$(VERSION)\""

install:
	@cp build/octo-cli $(INSTALLDIR)octo-cli
	@cp build/octo-run $(INSTALLDIR)octo-run
	@cp build/octo-de  $(INSTALLDIR)octo-de
	@test -f ~/.octo.rc && echo "~/.octo.rc already exists." || true
	@test -f ~/.octo.rc || ( echo "~/.octo.rc does not exist, creating." && cp -p octo.rc ~/.octo.rc )
	@echo "installed successfully in $(INSTALLDIR)"

uninstall:
	@rm -f $(INSTALLDIR)octo-cli
	@rm -f $(INSTALLDIR)octo-run
	@rm -f $(INSTALLDIR)octo-de
	@echo "uninstalled successfully."

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

testregress: cli
	@./scripts/test_compiler.sh ../Octo/octo
	@rm -rf temp.ch8
	@rm -rf temp.err
