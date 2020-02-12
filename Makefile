EXE=notify
CC	:= gcc

INC_DIR := /usr/include
INC_PATH = -I. \
           -I$(INC_DIR)

CFLAGS = -c -g  -Wall -Werror -fPIC -std=gnu11 -fms-extensions $(INC_PATH)
CEXTRA = -nostdlib

OBJS =  notify.o
CLEAN = rm -f $(EXE) *.o *.a *.so  *.map *.asm

.PHONY: all
all: notify mapfile

notify.o: notify.c
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: $(EXE)
notify: clean  $(OBJS)
	$(CC) -static -static-libgcc -o $(EXE) $(OBJS)
	$(shell ./disasm.sh *.o)

.PHONY: mapfile
mapfile: $(EXE)
	$(shell ld -M=notify.map -o /dev/null $(EXE) &> /dev/null)

.PHONY: clean
clean:
	$(shell $(CLEAN) &> /dev/null)
	@echo "repo is clean"

.PHONY: superclean
superclean: clean
	$(shell rm *~ &> /dev/null)
	@echo "cleaned unwanted backup files"
	$(shell rm vgcore* &> /dev/null)
	@echo "cleaned core files"
