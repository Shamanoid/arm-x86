CC = arm-elf-gcc
CCFLAGS += -g
OBJDUMP = arm-elf-objdump

main: main.c
	$(CC) $(CCFLAGS) main.c -nostdlib -o $@
	$(OBJDUMP) -dD $@ > $@.dis
clean:
	rm -rf main main.dis
