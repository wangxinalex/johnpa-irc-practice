# This makefile does NOT create the sircd for you.
# You'll want to compile debug.o into sircd to get
# the debugging functions.
# We suggest adding three targets:  all, clean, test

CC=gcc
CFLAGS=-Wall -DDEBUG -g -ggdb
OBJDIR=obj
OBJS=$(addprefix $(OBJDIR)/,debug.o rtgrading.o rtlib.o sircd.o arraylist.o common.o irc_proto.o message.o) # 
DEPS=debug-text.h common.h arraylist.h

all: sircd

$(OBJDIR)/irc_proto.o: irc_proto.c irc_proto.h $(DEPS) message.h
	$(CC) -c -o $@ $< $(CFLAGS)

$(OBJDIR)/%.o: %.c %.h $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

debug-text.h: debug.h
	./dbparse.pl < debug.h > debug-text.h

sircd: $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

#minid: minid.c $(OBJDIR)/debug.o $(OBJDIR)/common.o
#	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -rf $(OBJS) debug-text.h sircd minid

