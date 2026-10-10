CFLAGS+=-Wall
LDLIBS=-lm
BFLAGS+=--language=C

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

%.tab.h: %.y
	bison $(BFLAGS) --header="$@" -o /dev/null "$<"

%.tab.c: %.y
	bison $(BFLAGS) -o "$@" "$<"

%.tab.o: CFLAGS+=-Wno-switch-enum -Wno-unused-value
%.tab.o: %.tab.c


