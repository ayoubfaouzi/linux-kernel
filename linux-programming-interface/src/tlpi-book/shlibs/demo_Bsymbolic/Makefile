LIBS = libfoo1.so libfoo2.so libfoo3.so
TARGETS = prog ${LIBS}

all: ${TARGETS}

run: all
	LD_LIBRARY_PATH=. ./prog

prog: prog.o libfoo1.so libfoo2.so
	${CC} -g -o $@ $^ -Wl,--rpath-link,.

# libfoo3.so is not built with -Bsymbolic, so the local reference to
# xyz() can be interposed from another object.

${LIBS}: CFLAGS := ${CFLAGS} -fPIC

lib%.so: %.o
	${CC} ${LDFLAGS} -shared -o $@ $^

libfoo3.so: foo3.o

# libfoo2.so is built with -Bsymbolic, so the local reference to
# xyz() will bind to the local definition inside the library.

# By making libfoo3.so a dependency of libfoo2.so, we can observe that
# -Bsymbolic in one library (libfoo2.so) does not have an effect for
# a dependency of that library (libfoo3.so). (And thus, the reference
# in libfoo3.so still binds to the definition in the main program.)

libfoo2.so: foo2.o libfoo3.so
	${CC} -g -Wl,-Bsymbolic -shared -o $@ $^

# libfoo1.so is not built with -Bsymbolic, so the local reference to
# xyz() can be interposed from another object.

libfoo1.so: foo1.o

clean:
	${RM} ${TARGETS} *.o
