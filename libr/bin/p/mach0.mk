OBJ_MACH0=bin_mach0.o ../format/mach0/mach0.o

STATIC_OBJ+=${OBJ_MACH0}
TARGET_MACH0=bin_mach0.${EXT_SO}

ALL_TARGETS+=${TARGET_MACH0}

${TARGET_MACH0}: ${OBJ_MACH0}
	${CC} -shared ${CFLAGS} -o ${TARGET_MACH0} ${OBJ_MACH0}
	@#strip -s ${TARGET_MACH0}
