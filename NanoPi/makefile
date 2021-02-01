CC = cc
EXENAME = tel2com
SRCDIR = ../src
SRCS = $(SRCDIR)/tel2com.c $(SRCDIR)/uart.c $(SRCDIR)/telnet.c $(SRCDIR)/sshd.c
OBJS = $(SRCS:%.c=%.o)
CFLAGS = -DUSE_GPIO -DSYS_CLASS_GPIO -Wall
LDLIBS = -lssh -lcrypt

%.o: %.c 
	$(CC) -c $(CFLAGS) -o $@ $<

$(EXENAME): $(OBJS) 
	$(CC) $(CFLAGS) -o $(EXENAME) $(OBJS) $(LDLIBS)

$(OBJS): $(SRCDIR)/t2c_def.h

clean:
	rm -f $(OBJS)
