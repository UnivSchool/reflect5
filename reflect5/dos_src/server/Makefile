EXECNAME=REFLECT5.EXE

BCC_FORCE_CPP_COMPILE=-P
BCC_FORCE_C_COMPILE=-P-

REFLECT_OPT=-DKNOW_HOSTBYNAME
#REFLECT_OPT=

COMPDEF=$(REFLECT_OPT)
COMPINCL=-Id:\bc5\include
CC=bcc $(COMPINCL)
COMPILER_OPT=$(BCC_FORCE_C_COMPILE)
COMPILE=$(CC) $(COMPILER_OPT) $(COMPDEF)

LINK=bcc

target: goal

goal: reflect5.obj
	$(LINK) reflect5.o -e$(EXECNAME)

.c.obj:
	@echo "$* depends on $*.c/h"
	$(COMPILE) $*.c
#	bcc $(LINK_OPT) reflect5.c -eREFLECT5.EXE $(LINK_DEF)

.cc.obj:
	@echo "$* depends on $*.c/h"
	invalid.exe
