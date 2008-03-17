CC=	gcc
CFLAGS += -Wall
OBJDUMP= objdump

all: 	release debug

INC=	ArmX86Types.h	\
	ArmX86ElfLoad.h	\
	ArmX86Decode.h	\
	ArmX86CodeGen.h	\

SRC= 	ArmX86.c	\
	ArmX86ElfLoad.c	\
	ArmX86Decode.c	\
	ArmX86CodeGen.c	\

release: 	$(SRC) $(INC)
		$(CC) $(CFLAGS) $(SRC) -o arm
		$(OBJDUMP) -dD ./arm > ArmX86.dis

debug:		$(SRC) $(INC)
		$(CC) $(CFLAGS) -DDEBUG $(SRC) -o arm
		$(OBJDUMP) -dD ./arm > ArmX86.dis

clean:
		rm -rf *.o arm
