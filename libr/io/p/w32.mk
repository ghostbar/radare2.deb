OBJ_W32=io_w32.o

STATIC_OBJ+=${OBJ_W32}
TARGET_W32=io_w32.${EXT_SO}
ALL_TARGETS+=${TARGET_W32}

${TARGET_W32}: ${OBJ_W32}
	${CC} -shared ${CFLAGS} -o ${TARGET_W32} ${OBJ_W32} -lr_io
