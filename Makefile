# SPDX-License-Identifier: LGPL-2.1-or-later

CC ?= cc
PKG_CONFIG ?= pkg-config
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
LIBEXECDIR ?= $(PREFIX)/libexec
DESTDIR ?=

CPPFLAGS ?=
CFLAGS ?= -O2 -g
CFLAGS += -std=gnu11 -Wall -Wextra
LDFLAGS ?=

GPOD_CFLAGS = $(shell $(PKG_CONFIG) --cflags libgpod-1.0 glib-2.0)
GPOD_LIBS = $(shell $(PKG_CONFIG) --libs libgpod-1.0 glib-2.0)
BUILD_DIR := build
PROGRAMS := \
	$(BUILD_DIR)/ipod-add-core \
	$(BUILD_DIR)/ipod-time-sync-safe

.PHONY: all check clean install enable-clock-sync uninstall

all: $(PROGRAMS)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/ipod-add-core: src/ipod-add.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GPOD_CFLAGS) $< \
		$(LDFLAGS) $(GPOD_LIBS) -o $@

$(BUILD_DIR)/ipod-time-sync-safe: src/ipod-time-sync-safe.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDFLAGS) -o $@

check:
	bash -n bin/ipod-add scripts/install-ubuntu.sh tests/wrapper.sh tests/fake-backend.sh
	bash tests/wrapper.sh

install: all
	install -d "$(DESTDIR)$(BINDIR)" "$(DESTDIR)$(LIBEXECDIR)"
	install -m 0755 bin/ipod-add "$(DESTDIR)$(BINDIR)/ipod-add"
	install -m 0755 $(BUILD_DIR)/ipod-add-core "$(DESTDIR)$(LIBEXECDIR)/ipod-add-core"
	install -m 0755 $(BUILD_DIR)/ipod-time-sync-safe \
		"$(DESTDIR)$(LIBEXECDIR)/ipod-time-sync-safe"

# The narrow helper validates that it found an Apple iPod before elevating.
# Run this target as root after install if automatic clock sync is wanted.
enable-clock-sync:
	test -x "$(DESTDIR)$(LIBEXECDIR)/ipod-time-sync-safe"
	chown root:root "$(DESTDIR)$(LIBEXECDIR)/ipod-time-sync-safe"
	chmod 4755 "$(DESTDIR)$(LIBEXECDIR)/ipod-time-sync-safe"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/ipod-add" \
		"$(DESTDIR)$(LIBEXECDIR)/ipod-add-core" \
		"$(DESTDIR)$(LIBEXECDIR)/ipod-time-sync-safe"

clean:
	rm -rf -- "$(BUILD_DIR)"
