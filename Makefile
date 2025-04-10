CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -g
LIBS = -lcurl -ljansson

SRCDIR = src
OBJDIR = obj
EXEC = recap

SOURCES = $(wildcard $(SRCDIR)/*.c) 
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

all: $(EXEC)

# Link target
$(EXEC): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/recap.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	@mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(EXEC)

.PHONY: all clean
