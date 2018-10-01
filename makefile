PREFIX  = /usr/local
BIN = wm
SRC = wm.c
OBJ ?= ${SRC:.c=.o}
LDFLAGS += -lxcb

CC ?= cc
LD  = ${CC}

CFLAGS += -Wall -I/opt/X11/include -I/usr/X11R6/include
LDFLAGS += -L/opt/X11/lib -L/usr/X11R6/lib

.POSIX:
all: ${BIN}

.c.o: ${SRC}
	@${CC} -c $< -o $@ ${CFLAGS}
	@echo "wm has been built c:"
	
${OBJ}:

${BIN}: ${OBJ}
	@${LD} -o $@ ${OBJ} ${LDFLAGS}

install: ${BIN}
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@install ${BIN} ${DESTDIR}${PREFIX}/bin/${BIN}
	@chmod 755 ${DESTDIR}${PREFIX}/bin/${BIN}
	@echo "wm has been installed c:"


uninstall: ${DESTDIR}${PREFIX}/bin/${BIN}
	@rm -f ${DESTDIR}${PREFIX}/bin/${BIN}
	@echo "wm has been uninstalled :c"

clean:
	@rm -f ${BIN} ${OBJ}
	@echo "bin + obj files have been cleaned"
