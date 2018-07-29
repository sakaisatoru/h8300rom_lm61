PKG = main
#~ OBJ = mystartup.o main.o lm61.o lcd.o monitorsub.o sci.o data.o
OBJ = mystartup.o monitor.o main.o lcd.o monitorsub.o sci.o data.o
#OBJ =  main.o

SCRIPT_PREFIX = ./
TOOLS_PREFIX = /usr/local/h8300-hms/bin/
LIBPATH= /usr/local/h8300-hms/lib/gcc/h8300-hms/4.4.6/h8300h/normal/
CC = $(TOOLS_PREFIX)h8300-hms-gcc
AS = $(TOOLS_PREFIX)h8300-hms-as
OBJCOPY = $(TOOLS_PREFIX)h8300-hms-objcopy
OBJDUMP = $(TOOLS_PREFIX)h8300-hms-objdump

all: $(PKG).mot Makefile

$(PKG).mot: $(PKG)
	$(OBJCOPY) -O srec $< $@
	$(OBJDUMP) -D -S -s -mh8300hn $< > $<.ref

$(PKG): $(OBJ)
	$(CC)  -o $@  -T $(SCRIPT_PREFIX)3694f.x -nostartfiles -nostdlib $(OBJ) $(LIBPATH)libgcc.a

.s.o:
	$(AS) -o $@ $<

.c.o:
	$(CC) -Os -w -mrelax -g -o $@ -c -mh -mn $<

clean:
	rm -f $(OBJ) $(PKG)

write:
	h8write -3664 $(PKG).mot /dev/ttyUSB0

.PHONY: clean all

