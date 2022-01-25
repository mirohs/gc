CFLAGS = -std=c99 -Wall -Wno-unused-function -Wno-unused-variable -Werror -Wpointer-arith -Wfatal-errors
DEBUG = -g

# dependency chain:
# exe <-- o*       link
# o <-- c + h*     compile
# c <-- c + h*     preprocessor
# c + h <-- h.c    headify         create header and implementation files
# h.c <-- d.c      embrace         create C code with braces {...}

OBJECTS = gc.o util.o trie.o gc_test.o
SOURCES = $(OBJECTS:.o=.c)
HEADERS = $(OBJECTS:.o=.h)
DEPENDS = $(OBJECTS:.o=.d)

# disable default suffixes
.SUFFIXES:

gc: $(OBJECTS)
	gcc $(CFLAGS) $(DEBUG) $^ -lm -o $@

trie: trie.o trie_test.o util.o
	gcc $(CFLAGS) $(DEBUG) $^ -lm -o $@

# create C code file with braces {...}
%.h.c: %.d.c
	../embrace/embrace $< > $@

# create header and implementation files
%.c %.h: %.h.c
	../headify/headify $<

# pattern rule for compiling .c-file to executable
#%: %.o util.o
#	gcc $(CFLAGS) $(DEBUG) $^ -lm -o $@

# create object file from C file
%.o: %.c
	gcc -c $(CFLAGS) $(DEBUG) $<

# create dependency files (listing the .c-file and the .h-files that a .o file depends on)
%.d: %.c $(SOURCES) $(HEADERS)
	@echo "$@ \\" >$@; \
	gcc -MM $< >>$@

-include $(DEPENDS)

# do not treat "clean" as a file name
.PHONY: clean 

# remove produced files, invoke as "make clean"
clean: 
	rm -f *.o
	rm -f *.d
	rm -rf .DS_Store
	rm -rf *.dSYM
	rm -f gc.[ch] gc.h.c gc
	rm -f trie.[ch] trie.h.c trie
	rm -f gc_test.[ch] gc_test.h.c gc_test
	rm -f trie_test.[ch] trie_test.h.c trie_test
