CC=i486-mingw32-gcc
RANLIB=i486-mingw32-ranlib
OSTYPE=windows
ONELIB=0
LINK=-shared
CC_AR=echo ar -r ${LIBAR}
PICFLAGS=
CFLAGS+=${PICFLAGS} -MD -D__WINDOWS__=1
CC_LIB=${CC} -shared -o
CFLAGS_INCLUDE=-I
LDFLAGS_LINK=-l
LDFLAGS_LINKPATH=-L
CFLAGS_OPT0=-O0
CFLAGS_OPT1=-O1
CFLAGS_OPT2=-O2
CFLAGS_OPT3=-O3
CFLAGS_DEBUG=-g
