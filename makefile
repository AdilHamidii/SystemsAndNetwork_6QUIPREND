CC=gcc
CFLAGS=-Wall -Wextra -std=c11 -Isrc/headers

SRCDIR=src
OBJDIR=bin

OBJS_COMMON=$(OBJDIR)/net.o $(OBJDIR)/util.o $(OBJDIR)/game.o

all: server client robot robot_grok

server: $(OBJDIR)/server.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o server $^

client: $(OBJDIR)/client.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o client $^

robot: $(OBJDIR)/robot.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o robot $^

robot_grok: $(OBJDIR)/robot_grok.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o robot_grok $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) server client robot robot_grok
