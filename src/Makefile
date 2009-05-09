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

INC=	armx86types.h	\
	elfload.h	\
	decode.h	\
	codegen.h	\
	codeenv.h	\

OBJ= 	main$(FLAV).o		\
	elfload$(FLAV).o	\
	alu$(FLAV).o		\
	decode$(FLAV).o		\
	codeenv$(FLAV).o	\

main$(FLAV).o:		main.c $(INC)
			$(CC) $(CFLAGS) main.c -c -o $@
decode$(FLAV).o:	decode.c decodeprivate.h $(INC)
			$(CC) $(CFLAGS) decode.c -c -o $@
alu$(FLAV).o:		alu.c decodeprivate.h $(INC)
			$(CC) $(CFLAGS) alu.c -c -o $@
elfload$(FLAV).o:	elfload.c $(INC)
			$(CC) $(CFLAGS) elfload.c -c -o $@
codeenv$(FLAV).o:	codeenv.c $(INC)
			$(CC) $(CFLAGS) codeenv.c -c -o $@

exec:	 	$(OBJ)
		$(CC) $(OBJ) $(LOPTS) -o arm$(FLAV)
		$(OBJDUMP) -dD ./arm$(FLAV) > ArmX86$(FLAV).dis

clean:
		rm -rf *.o *.map *.dis arm*
