CC = cc
EXENAME = tel2com
SRCDIR = ../src
SRCS = $(SRCDIR)/tel2com.c $(SRCDIR)/uart.c $(SRCDIR)/telnet.c
OBJS = $(SRCS:%.c=%.o)
CFLAGS = -DPC_LINUX -Wall
LDLIBS = 

%.o: %.c 
	$(CC) -c $(CFLAGS) -o $@ $<

$(EXENAME): $(OBJS) 
	$(CC) $(CFLAGS) -o $(EXENAME) $(OBJS) $(LDLIBS)

$(OBJS): $(SRCDIR)/t2c_def.h

clean:
	rm -f $(OBJS)
