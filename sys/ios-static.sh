#!/bin/sh

if [ "$1" = "-h" ]; then
	echo "Usage: sys/ios-static.sh [armv7|arm64]"
	exit 0
fi

if [ -n "$1"]; then
	export CPU="$1"
fi

if [ -z "${CPU}" ]; then
	export CPU=arm64
#	export CPU=armv7
fi

CAPSTONE_ARCHS="arm aarch64"
#export CAPSTONE_MAKEFLAGS="CAPSTONE_ARCHS=\"arm aarch64\""
# Build all archs for capstone, not just ARM/ARM64
# export CAPSTONE_MAKEFLAGS=""

[ -z "${MAKE}" ] && MAKE=make
[ -z "${MAKE_JOBS}" ] && MAKE_JOBS=12

# if set to 1 build without fork or debugger support
if [ -z "${APPSTORE_FRIENDLY}" ]; then
	APPSTORE_FRIENDLY=0
fi

export BUILD=1
PREFIX="/usr"
# PREFIX=/var/mobile

if [ ! -f sys/ios-include/mach/mach_vm.h ]; then
(
	cd sys && \
	wget http://lolcathost.org/b/ios-include.tar.gz && \
	tar xzvf ios-include.tar.gz
)
fi

export PATH=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin:$PATH
export PATH=$(pwd)/sys:${PATH}
export CC=$(pwd)/sys/ios-sdk-gcc
# set only for arm64, otherwise it is armv7
# select ios sdk version
export IOSVER=9.3
export IOSINC=$(pwd)/sys/ios-include
#export CFLAGS=-O2
export CFLAGS="-Os -flto"
export LDFLAGS="-flto"
export USE_SIMULATOR=0

if [ "${APPSTORE_FRIENDLY}" = 1 ]; then
	CFGFLAGS="--without-fork --without-debugger"
else
	CFGFLAGS=""
fi

if true ; then
${MAKE} clean
cp -f plugins.tiny.cfg plugins.cfg
cp -f plugins.ios.cfg plugins.cfg
./configure --prefix="${PREFIX}" \
	${CFGFLAGS} \
	--with-ostype=darwin \
	--without-pic --with-nonpic \
	--with-compiler=ios-sdk \
	--target=arm-unknown-darwin
fi

if [ $? = 0 ]; then
	time ${MAKE} -j${MAKE_JOBS} CAPSTONE_ARCHS="${CAPSTONE_ARCHS}"
	#time ${MAKE} -j${MAKE_JOBS}
	if [ $? = 0 ]; then
		( cd binr/radare2 ; ${MAKE} ios_sdk_sign )
		rm -rf /tmp/r2ios
		${MAKE} install DESTDIR=/tmp/r2ios
		rm -rf /tmp/r2ios/usr/share/radare2/*/www/enyo/node_modules
		( cd /tmp/r2ios && tar czvf ../r2ios-static-${CPU}.tar.gz ./* )
		rm -rf sys/cydia/radare2/root
		mkdir -p sys/cydia/radare2/root
		sudo tar xpzvf /tmp/r2ios-static-${CPU}.tar.gz -C sys/cydia/radare2/root
#		( cd sys/cydia/radare2 ; sudo ${MAKE} clean ; sudo ${MAKE} )

		# Creating tarball
		export D=radare2-ios-${CPU}
		rm -rf $D
		mkdir -p $D/bin
		for a in radare2 rabin2 rasm2 r2pm r2agent radiff2 rafind2 ragg2 rahash2 rarun2 rasm2 rax2 ; do
			cp -f binr/$a/$a "$D/bin"
		done
		mkdir -p "$D/include"
		cp -rf sys/cydia/radare2/root/usr/include/* $D/include
		mkdir -p $D/lib
		cp -f libr/libr.a $D/lib
		cp -f binr/preload/libr2.dylib $D/lib
		for a in $D/bin/* ; do
			strip $a
		done
		tar czvf $D.tar.gz $D
	fi
fi
