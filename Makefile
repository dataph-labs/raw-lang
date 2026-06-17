PROGS=            raw-runtime rawcc

SRCS.raw-runtime= runtime.c
SRCS.rawcc=       cc.c

MKMAN=no

.include <bsd.prog.mk>
