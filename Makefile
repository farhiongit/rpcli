# Using GNU Make >= 4.3
CFLAGS+=-Wall
BFLAGS+=--language=C -Werror

#RUNNER=valgrind --leak-check=full
#CFLAGS+=-g
# Enable run-time traces (%define parse.trace, RPN_DEBUG set to 1, RPN_debug can be set to activate tracing)
# Generate the parser description file (%verbose)
#BFLAGS+=--debug --verbose

all: rpcli
	$(RUNNER) ./"$<"

rpcli: LDLIBS=-lm
rpcli: rpgrammar.tab.o rpcli.o rp_lexer.o streamreader.o
################################################
.INTERMEDIATE: rpcli.o
rpcli.o: rpcli.c rpgrammar.tab.h rp_types.h rp_parser.h streamreader.h
################################################
.INTERMEDIATE: rp_lexer.o
rp_lexer.o: rp_lexer.c rpgrammar.tab.h rp_types.h rp_parser.h streamreader.h
################################################
.INTERMEDIATE: rpgrammar.tab.c rpgrammar.tab.o
rpgrammar.tab.o: rpgrammar.tab.c rpgrammar.tab.h rp_types.h rp_parser.h rp_parser.c

rpgrammar.tab.h: rp_types.h rp_parser.h streamreader.h
rpgrammar.tab.c: rp_types.h rp_parser.h rp_parser.c streamreader.h

# Group of multiple targets (with "&:" in GNU make 4.3 and later).
# https://www.gnu.org/software/make/manual/html_node/Multiple-Targets.html
# Grouped-target rules address a longstanding issue of make's design: how to describe targets that are unavoidably created together.
# The canonical example would probably be the outputs of yacc or bison: a C source file and corresponding header file, both generated together by one run of the same program.
# https://stackoverflow.com/questions/63656568/how-to-specify-multiple-targets-in-makefile
%.tab.c %.tab.h &: %.y
	bison $(BFLAGS) --header="${@:.c=.h}" -o "${@:.h=.c}" "$<"

%.tab.o: CFLAGS+=-Wno-switch-enum -Wno-switch-default
%.tab.o: %.tab.c
################################################
.INTERMEDIATE: streamreader.o
streamreader.o: streamreader.c streamreader.h
################################################
test_streamreader: streamreader.o test_streamreader.c
################################################
.INTERMEDIATE: mongoose.o rphttpserver.o
mongoose.o: CFLAGS=-Wno-cast-align -Wno-cast-qual -Wno-float-equal -Wno-arith-conversion -Wno-switch-default -Wno-redundant-decls

rphttpserver: LDLIBS=-lm
rphttpserver: rpgrammar.tab.o rphttpserver.o rp_lexer.o streamreader.o mongoose.o
rphttpserver.o: rphttpserver.c rpgrammar.tab.h rp_types.h rp_parser.h streamreader.h
