CC = arm-elf-gcc
CCFLAGS += -g
OBJDUMP = arm-elf-objdump

main: main.c
	$(CC) $(CCFLAGS) main.c -static -o $@
	$(OBJDUMP) -d $@ > $@.dis
clean:
	rm -rf main main.dis
