CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -O1

SRCS = src/main.c src/util.c src/lex.c src/types.c src/symbols.c src/expr.c \
       src/typedecl.c src/stmt.c src/decl.c src/module.c src/deps.c src/md5.c src/lib.c
HDRS = src/arinm2c.h src/util.h src/lex.h src/types.h src/symbols.h \
       src/expr.h src/typedecl.h src/stmt.h src/decl.h src/module.h src/deps.h src/md5.h src/lib.h

arinm2c: $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

check: arinm2c
	sh tests/run.sh

clean:
	rm -f arinm2c
