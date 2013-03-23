include config.mk
VALADIR=bindings/vala

PWD=$(shell pwd)
PFX=${DESTDIR}${PREFIX}
HGFILES=`find sdb-${VERSION} -type f | grep -v hg | grep -v swp`

all: src/sdb-version.h
	cd src && ${MAKE}
	cd memcache && ${MAKE}
ifneq (${HAVE_VALA},)
	cd ${VALADIR} && ${MAKE}
	cd ${VALADIR}/types && ${MAKE}
endif

src/sdb-version.h:
	echo '#define SDB_VERSION "${VERSION}"' > src/sdb-version.h

clean:
	rm -f src/sdb-version.h
	cd src && ${MAKE} clean
	cd memcache && ${MAKE} clean
	cd test && ${MAKE} clean
	cd ${VALADIR} && ${MAKE} clean

dist:
	rm -f sdb-${VERSION}.tar.gz
	rm -rf sdb-${VERSION}
	git clone . sdb-${VERSION}
	rm -rf sdb-${VERSION}/.git*
	tar czvf sdb-${VERSION}.tar.gz sdb-${VERSION}
	pub sdb-${VERSION}.tar.gz
	rm -rf sdb-${VERSION}

install-dirs:
	mkdir -p ${PFX}/lib/pkgconfig ${PFX}/bin 
	mkdir -p ${PFX}/share/vala/vapi ${PFX}/include/sdb

install: install-dirs
	cp -f src/libsdb.* ${PFX}/lib
	cp -f src/sdb.h ${PFX}/include/sdb
	cp -f src/sdb-version.h ${PFX}/include/sdb
	cp -f src/cdb.h ${PFX}/include/sdb
	cp -f src/ht.h ${PFX}/include/sdb
	cp -f src/types.h ${PFX}/include/sdb
	cp -f src/ls.h ${PFX}/include/sdb
	cp -f src/cdb_make.h ${PFX}/include/sdb
	cp -f src/buffer.h ${PFX}/include/sdb
	cp -f src/config.h ${PFX}/include/sdb
	cp -f src/sdb ${PFX}/bin
	cp -f memcache/libmcsdb.a ${PFX}/lib
	cp -f memcache/mcsdb.h ${PFX}/include/sdb
	cp -f memcache/mcsdbd ${PFX}/bin
	cp -f memcache/mcsdbc ${PFX}/bin
	cp -f ${VALADIR}/sdb.pc ${PFX}/lib/pkgconfig
	cp -f ${VALADIR}/mcsdb.pc ${PFX}/lib/pkgconfig
ifneq (${HAVE_VALA},)
	cp -f ${VALADIR}/sdb.vapi ${PFX}/share/vala/vapi
	cp -f ${VALADIR}/mcsdb.vapi ${PFX}/share/vala/vapi
	cd ${VALADIR}/types && ${MAKE} install PFX=${PFX}
endif

deinstall uninstall:
	rm -rf ${PFX}/include/sdb
	rm -f ${PFX}/bin/sdb
	rm -f ${PFX}/bin/mcsdbc
	rm -f ${PFX}/bin/mcsdbd
	rm -f ${PFX}/lib/libsdb.*
	rm -f ${PFX}/lib/libmcsdb.a
	rm -f ${PFX}/lib/pkgconfig/sdb.pc
	rm -f ${PFX}/lib/pkgconfig/mcsdb.pc
ifneq (${HAVE_VALA},)
	rm -f ${PFX}/share/vala/vapi/sdb.vapi 
	rm -f ${PFX}/share/vala/vapi/mcsdb.vapi 
	cd ${VALADIR}/types && ${MAKE} uninstall PFX=${PFX}
endif

symstall: install-dirs
	cd src ; for a in libsdb.* ; do \
		ln -fs ${PWD}/src/$$a ${PFX}/lib/$$a ; done
	ln -fs ${PWD}/src/sdb ${PFX}/bin
	ln -fs ${PWD}/src/sdb.h ${PFX}/include/sdb
	ln -fs ${PWD}/src/sdb-version.h ${PFX}/include/sdb
	ln -fs ${PWD}/src/cdb.h ${PFX}/include/sdb
	ln -fs ${PWD}/src/ht.h ${PFX}/include/sdb
	ln -fs ${PWD}/src/types.h ${PFX}/include/sdb
	ln -fs ${PWD}/src/ls.h ${PFX}/include/sdb
	ln -fs ${PWD}/src/cdb_make.h ${PFX}/include/sdb
	ln -fs ${PWD}/src/buffer.h ${PFX}/include/sdb
	ln -fs ${PWD}/src/config.h ${PFX}/include/sdb
	ln -fs ${PWD}/bindings/vala/sdb.pc ${PFX}/lib/pkgconfig
	ln -fs ${PWD}/bindings/vala/mcsdb.pc ${PFX}/lib/pkgconfig
ifneq (${HAVE_VALA},)
	ln -fs ${PWD}/vala/sdb.vapi ${PFX}/share/vala/vapi
	ln -fs ${PWD}/vala/mcsdb.vapi ${PFX}/share/vala/vapi
	ln -fs ${PWD}/vala/sdb.vapi ${PFX}/share/vala/vapi
	ln -fs ${PWD}/vala/mcsdb.vapi ${PFX}/share/vala/vapi
	cd ${VALADIR}/types && ${MAKE} symstall PFX=${PFX}
endif

.PHONY: all ${VALADIR} clean dist install uninstall deinstall
