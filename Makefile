PKG = main
#~ OBJ = mystartup.o main.o lm61.o lcd.o monitorsub.o sci.o data.o
#~ OBJ = mystartup.o monitor.o main.o time_my.o lcd.o monitorsub.o sci.o data.o
OBJ = mystartup.o monitor.o main.o time_my.o myprintf.o lcd.o monitorsub.o sci.o
#OBJ =  main.o

SCRIPT_PREFIX = ./
TOOLS_PREFIX = /usr/local/h8300-elf/bin/
LIBPATH= /usr/local/h8300-elf/lib/gcc/h8300-elf/8.4.0/h8300h/normal/
#~ LIBPATH= /usr/local/h8300-elf/lib/gcc/h8300-elf/9.4.0/h8300h/normal/
#~ LIBPATH= /usr/local/h8300-elf/lib/gcc/h8300-elf/11.4.0/normal/
CC = $(TOOLS_PREFIX)h8300-elf-gcc
AS = $(TOOLS_PREFIX)h8300-elf-as
OBJCOPY = $(TOOLS_PREFIX)h8300-elf-objcopy
OBJDUMP = $(TOOLS_PREFIX)h8300-elf-objdump
NM		= $(TOOLS_PREFIX)h8300-elf-nm

all: $(PKG).mot Makefile

$(PKG).mot: $(PKG)
	$(OBJCOPY) -O srec $< $@
#~ 	$(OBJDUMP) -D -S -s -mh8300hn $< > $<.ref
	$(OBJDUMP) -d -S -h -t -mh8300hn $< > $<.ref
#~ 	$(OBJDUMP) -h  $< > $<.ref2
#~ 	$(NM) -n -S $< > $<.sym

$(PKG): $(OBJ)
	$(CC)  -o $@  -T $(SCRIPT_PREFIX)3694f.x -nostartfiles -nostdlib $(OBJ) $(LIBPATH)libgcc.a 
	# /usr/local/h8300-elf/h8300-elf/lib/h8300h/normal/libstdc++.a
#~ 	$(CC)  -o $@  -T $(SCRIPT_PREFIX)3694f.x -nostartfiles -nostdlib $(OBJ) 

.s.o:
	$(AS) -o $@ $<

.c.o:
	$(CC) -isystem /usr/local/h8300-elf/include -Os -w -mrelax -g -o $@ -c -mh -mn $< -Wl,-Map=$<.map,--cref

clean:
	rm -f $(OBJ) $(PKG)

write:
	h8write -3664 $(PKG).mot /dev/ttyUSB0

.PHONY: clean all

