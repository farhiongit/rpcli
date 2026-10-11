# Using GNU Make >= 4.3
CFLAGS+=-Wall
LDLIBS=-lm
BFLAGS+=--language=C -Werror

#RUNNER=valgrind --leak-check=full
#CFLAGS+=-g
# Enable run-time traces (%define parse.trace, RPN_DEBUG set to 1, RPN_debug can be set to activate tracing)
# Generate the parser description file (%verbose)
#BFLAGS+=--debug --verbose

all: rpcli
	$(RUNNER) ./"$<"

rpcli: rpgrammar.tab.o rpcli.o

rpcli.o: CFLAGS+=-Wno-switch-enum
rpcli.o: rpcli.c rpgrammar.tab.h

.INTERMEDIATE: rpgrammar.tab.c

# Group of multiple targets (with "&:" in GNU make 4.3 and later).
# https://www.gnu.org/software/make/manual/html_node/Multiple-Targets.html
# Grouped-target rules address a longstanding issue of make's design: how to describe targets that are unavoidably created together.
# The canonical example would probably be the outputs of yacc or bison: a C source file and corresponding header file, both generated together by one run of the same program.
# https://stackoverflow.com/questions/63656568/how-to-specify-multiple-targets-in-makefile
%.tab.c %.tab.h &: %.y
	bison $(BFLAGS) --header="${@:.c=.h}" -o "${@:.h=.c}" "$<"

%.tab.o: CFLAGS+=-Wno-switch-enum -Wno-unused-value
%.tab.o: %.tab.c


