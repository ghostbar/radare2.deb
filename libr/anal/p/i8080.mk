OBJ_I8080=anal_i8080.o

STATIC_OBJ+=${OBJ_I8080}
TARGET_I8080=anal_i8080.${EXT_SO}

ALL_TARGETS+=${TARGET_I8080}
#LDFLAGS+=-L../../lib -lr_lib
#LDFLAGS+=-L../../syscall -lr_syscall
#LDFLAGS+=-L../../diff -lr_diff

${TARGET_I8080}: ${OBJ_I8080}
	${CC} $(call libname,anal_z80) ${LDFLAGS} ${CFLAGS} -o anal_z80.${EXT_SO} ${OBJ_I8080}
