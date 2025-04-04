CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g

SRCDIR = src
OBJDIR = obj
EXEC = ctf

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

all: $(EXEC)

# Link target
$(EXEC): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/ctf.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	@mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(EXEC)

.PHONY: all clean
