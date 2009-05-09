ifneq (,$(findstring _debug,$(FLAV)))
CFLAGS += -g -DDEBUG
endif

ifneq (,$(findstring _noindex,$(FLAV)))
CFLAGS += -DNOINDEX
endif

ifneq (,$(findstring _nochain,$(FLAV)))
CFLAGS += -DNOCHAINING
endif

CC = gcc
CFLAGS += -Wall -O4
LOPTS = -Wl,-Ttext -Wl,90000000 -Wl,-Tbss -Wl,A0000000 -Wl,-Tdata -Wl,B0000000 -Wl,-Map -Wl,arm$(FLAV).map

OBJDUMP= objdump

all: 	exec

INC=	ArmX86Types.h	\
	ArmX86ElfLoad.h	\
	ArmX86Decode.h	\
	codegen.h	\
	ArmX86CodeGen.h	\

OBJ= 	ArmX86$(FLAV).o	\
	ArmX86ElfLoad$(FLAV).o	\
	alu$(FLAV).o	\
	ArmX86Decode$(FLAV).o	\
	ArmX86CodeGen$(FLAV).o	\

ArmX86$(FLAV).o:	ArmX86.c $(INC)
			$(CC) $(CFLAGS) ArmX86.c -c -o $@
ArmX86Decode$(FLAV).o:	ArmX86Decode.c ArmX86DecodePrivate.h $(INC)
			$(CC) $(CFLAGS) ArmX86Decode.c -c -o $@
alu$(FLAV).o:		alu.c ArmX86DecodePrivate.h $(INC)
			$(CC) $(CFLAGS) alu.c -c -o $@
ArmX86ElfLoad$(FLAV).o:ArmX86ElfLoad.c $(INC)
			$(CC) $(CFLAGS) ArmX86ElfLoad.c -c -o $@
ArmX86CodeGen$(FLAV).o:ArmX86CodeGen.c $(INC)
			$(CC) $(CFLAGS) ArmX86CodeGen.c -c -o $@

exec:	 	$(OBJ)
		$(CC) $(OBJ) $(LOPTS) -o arm$(FLAV)
		$(OBJDUMP) -dD ./arm$(FLAV) > ArmX86$(FLAV).dis

clean:
		rm -rf *.o *.map *.dis arm*
