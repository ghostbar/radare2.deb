#!/bin/sh

MAKE_JOBS=8
PREFIX=/usr

[ ! "${PREFIX}" = /usr ] && \
	CFGARG=--with-rpath

MAKE=make
gmake --help >/dev/null 2>&1
[ $? = 0 ] && MAKE=gmake

# find root
cd `dirname $PWD/$0` ; cd ..

ccache --help > /dev/null 2>&1
if [ $? = 0 ]; then
	[ -z "${CC}" ] && CC=gcc
	CC="ccache ${CC}"
	export CC
fi

# build
${MAKE} mrproper > /dev/null 2>&1
[ "`uname`" = Linux ] && export LDFLAGS="-Wl,--as-needed"
rm -f plugins.cfg
./configure ${CFGARG} --prefix=${PREFIX} || exit 1
exec ${MAKE} -s -j ${MAKE_JOBS}
