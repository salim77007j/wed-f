# WED browser — Linux GTK3/WebKitGTK build
# Requires: gtk+-3.0, webkit2gtk-4.1, and the Rust core (built via cargo).

CFLAGS ?= -O2 -g
WED_CFLAGS = $(CFLAGS) -std=gnu11 -Wall -Wextra -Wno-unused-parameter \
	$(shell pkg-config --cflags webkit2gtk-4.0 2>/dev/null || pkg-config --cflags webkit2gtk-4.1 gtk+-3.0)

SHELL_SRC = shell/main.c shell/browser.c shell/tabstrip.c shell/toolbar.c \
	shell/omnibox.c shell/webview.c shell/findbar.c shell/panels.c \
	shell/downloads.c shell/privacydash.c shell/settingsui.c \
	shell/startpage.c shell/theme.c shell/icons.c shell/menus.c shell/ai.c

RUST_LIB = core/target/release/libwed_core.a
BIN = wed-browser

all: $(BIN)

rust:
	cd core && cargo build --release

$(RUST_LIB): core/src/*.rs core/Cargo.toml
	cd core && cargo build --release

LIBS := $(shell tools/resolvelibs.sh webkit2gtk-4.1 gtk+-3.0)

$(BIN): $(SHELL_SRC) $(RUST_LIB) include/wed_core.h shell/wed.h
	gcc $(WED_CFLAGS) -Iinclude -Ishell $(SHELL_SRC) $(RUST_LIB) \
	        $(LIBS) -L$(LIBDIR) -L/usr/lib/x86_64-linux-gnu -lm -ldl -lpthread \
	        -Wl,-rpath,$(LIBDIR) -o $(BIN)

test: $(BIN)
	./tests/run_ui_tests.sh

clean:
	rm -f $(BIN)
	cd core && cargo clean

.PHONY: rust test clean
