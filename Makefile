EXE=notify
CC	:= gcc

INC_DIR := /home/mdday/src/linux/include/linux/uapi
INC_PATH = -I. \
           -I$(INC_DIR)/linux \
           -I$(INC_DIR)/asm-generic \
           -I/usr/include


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
	$(CC) -static -o $(EXE) $(OBJS)
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

#OPTIONS = --addrs 0xfff --delete  --pre-alloc 0xfff --timing --threads 0x0ff --hash 8

#.PHONY: test
#test: phys-virt FORCE
#	valgrind --leak-check=full --show-leak-kinds=all ./phys-virt $(OPTIONS)
