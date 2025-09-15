PREFIX ?= /usr/local
BINDIR := $(PREFIX)/bin
MANDIR := $(PREFIX)/share/man/man1

CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -g -D_POSIX_C_SOURCE=200809L
LIBS = -lcurl -ljansson -lpcre2-8

SRCDIR = src
OBJDIR = obj
EXEC = recap
MANPAGE = doc/recap.1

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

all: $(EXEC)

.PHONY: test
test: all
	@bash test/run-integration-tests.sh


$(EXEC): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/recap.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	@mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(EXEC)

install: $(EXEC) $(MANPAGE)
	install -Dm755 $(EXEC) $(BINDIR)/$(EXEC)
	install -Dm644 $(MANPAGE) $(MANDIR)/$(EXEC).1
	@echo "$(EXEC): installed to $(BINDIR), manpage to $(MANDIR)"

uninstall:
	rm -f $(BINDIR)/$(EXEC)
	rm -f $(MANDIR)/$(MANPAGE).1
	@echo "$(EXEC): uninstalled"

.PHONY: all clean install uninstall