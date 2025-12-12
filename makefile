CC=gcc
CFLAGS=-Wall -Wextra -std=c11

OBJS_COMMON=net.o util.o game.o

all: server client robot robot_grok

server: server.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o server server.o $(OBJS_COMMON)

client: client.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o client client.o $(OBJS_COMMON)

robot: robot.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o robot robot.o $(OBJS_COMMON)

robot_grok: robot_grok.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o robot_grok robot_grok.o $(OBJS_COMMON)

clean:
	rm -f *.o server client robot robot_grok
