CC		= gcc -std=c99
CFLAGS	= -g -Wall -pedantic -D_POSIX_C_SOURCE=200809L -Wextra \
		-Wwrite-strings -Wstrict-prototypes -Wold-style-definition \
		-Wformat=2 -Wno-unused-parameter -Wshadow \
		-Wredundant-decls -Wnested-externs -Wmissing-include-dirs \
		-D_FILE_OFFSET_BITS=64 `pkg-config fuse --cflags` # richiesto da FUSE

SRCDIR  	= ./src
INCDIR		= ./include
OBJDIR   	= ./obj
BINDIR   	= ./bin
LIBDIR      = ./libs

INCLUDES 	= -I $(INCDIR)
LDFLAGS 	= `pkg-config fuse --libs` -L $(LIBDIR)

# dipendenze per l'eseguibile
OBJS_MAIN	=	$(OBJDIR)/main.o

TARGETS	= $(BINDIR)/learningfuse

.PHONY: all clean cleanall run umnount umount

all: $(BINDIR) $(OBJDIR) $(INCDIR) $(TARGETS)

$(BINDIR):
	mkdir $(BINDIR)

$(OBJDIR):
	mkdir $(OBJDIR)

$(INCDIR):
	mkdir $(INCDIR)

# generazione di un .o da un .c con il relativo .h come dipendenza
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(INCDIR)/%.h
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# generazione di un .o da un .c senza relativo .h
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# da .c ad eseguibile learningfuse
$(BINDIR)/learningfuse: $(OBJS_MAIN)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGETS)

cleanall: clean
	\rm -f $(OBJDIR)/*.o *~ *.a *.sock

run: all
	$(BINDIR)/learningfuse -d -s ./tmp/example &

unmount:
	$(BINDIR)/learningfuse -u ./tmp/example

umount:
	sudo umount -l ./tmp/example

test:
	@echo "Testing write callback" > ./tmp/example/testfile.txt
