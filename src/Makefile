CC=	gcc
CFLAGS += -Wall

all: 	release debug

INC=	ArmX86Types.h	\
	ArmX86ElfLoad.h	\
	ArmX86Decode.h	\

SRC= 	ArmX86.c	\
	ArmX86ElfLoad.c	\
	ArmX86Decode.c	\

release: 	$(SRC) $(INC)
		$(CC) $(CFLAGS) $(SRC) -o arm

debug:		$(SRC) $(INC)
		$(CC) $(CFLAGS) -DDEBUG $(SRC) -o arm

clean:
		rm -rf *.o arm
