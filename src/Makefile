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
		$(CC) $(CFLAGS) $(SRC) -g -O4 -Wl,-Ttext -Wl,90000000 -Wl,-Tbss -Wl,A0000000 -Wl,-Tdata -Wl,B0000000 -Wl,-Map -Wl,arm_rel.map -o arm
		$(OBJDUMP) -dD ./arm > ArmX86.dis

debug:		$(SRC) $(INC)
		$(CC) $(CFLAGS) -DDEBUG -g -O4 $(SRC) -Wl,-Ttext -Wl,0x90000000 -Wl,-Tbss -Wl,0xA0000000 -Wl,-Tdata -Wl,0xB0000000 -Wl,-Map -Wl,arm_dbg.map -o arm
		$(OBJDUMP) -dD ./arm > ArmX86.dis

clean:
		rm -rf *.o *.map arm
