CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g

SRCDIR = src
OBJDIR = obj
EXEC = ctf

# Find all .c files in SRCDIR
SOURCES = $(wildcard $(SRCDIR)/*.c)
# Create object file names in OBJDIR
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

# Default target
all: $(EXEC)

# Link target
$(EXEC): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

# Compile target
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/ctf.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create object directory if it doesn't exist
$(OBJDIR):
	@mkdir -p $(OBJDIR)

# Clean target
clean:
	rm -rf $(OBJDIR) $(EXEC)

.PHONY: all clean
