CC=	gcc
CFLAGS += -Wall

all: 	release debug

SRC= 	ArmX86.c	\
	ArmX86ElfLoad.c	\
	ArmX86Decode.c	\

release: 	$(SRC)
		$(CC) $(CFLAGS) $(SRC) -o arm

debug:		$(SRC)
		$(CC) $(CFLAGS) -DDEBUG $(SRC) -o arm

clean:
		rm -rf *.o arm
