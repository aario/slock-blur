# slock-blur - simple screen locker
# See LICENSE file for copyright and license details.

include config.mk

SRC = slock.c stackblur.c
OBJ = ${SRC:.c=.o}

all: options slock

options:
	@echo slock build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

config.h:
	@echo creating $@ from config.def.h
	@cp config.def.h $@

slock: ${OBJ}
	@echo CC -o $@
	@${CC} -pthread -o $@ ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f slock ${OBJ} slock-blur-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p slock-blur-${VERSION}
	@cp -R LICENSE Makefile README config.def.h config.mk ${SRC} slock-blur-${VERSION}
	@tar -cf slock-blur-${VERSION}.tar slock-blur-${VERSION}
	@gzip slock-blur-${VERSION}.tar
	@rm -rf slock-blur-${VERSION}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f slock ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/slock
	@chmod u+s ${DESTDIR}${PREFIX}/bin/slock

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/slock

.PHONY: all options clean dist install uninstall
