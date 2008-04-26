CC = arm-linux-gcc
CCFLAGS += -g
OBJDUMP = arm-linux-objdump

main: main.c
	$(CC) $(CCFLAGS) main.c -static -o $@
	$(OBJDUMP) -dD $@ > $@.dis
clean:
	rm -rf main main.dis
