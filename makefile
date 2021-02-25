CC		= gcc -std=c99
CFLAGS	= -g -Wall -pedantic -D_POSIX_C_SOURCE=200809L -Wextra 		\
		-Wwrite-strings -Wstrict-prototypes -Wold-style-definition 	\
		-Wformat=2 -Wno-unused-parameter -Wshadow 					\
		-Wredundant-decls -Wnested-externs -Wmissing-include-dirs 	\
		-D_FILE_OFFSET_BITS=64 `pkg-config fuse --cflags` # richiesto da FUSE

SRCDIR  	= ./src
INCDIR		= ./include
OBJDIR   	= ./obj
BINDIR   	= ./bin
LIBDIR      = ./libs

INCLUDES 	= -I $(INCDIR)
LDFLAGS 	= `pkg-config fuse --libs` -L $(LIBDIR)	# richiesto da FUSE

# dipendenze per l'eseguibile
OBJS_MAIN	=	$(OBJDIR)/main.o		\
				$(OBJDIR)/scfiles.o		\
				$(OBJDIR)/socketconn.o

OBJS_PROD 	= 	$(OBJDIR)/fsprod.o		\
				$(OBJDIR)/scfiles.o

OBJS_CONS 	= 	$(OBJDIR)/fscons.o		\
				$(OBJDIR)/scfiles.o

TARGETS	= $(BINDIR)/fspipe $(BINDIR)/fsprod $(BINDIR)/fscons

.PHONY: all clean cleanall run_client run_server debug_client debug_server checkmount unmount

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

# da .c ad eseguibile fspipe
$(BINDIR)/fspipe: $(OBJS_MAIN)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

$(BINDIR)/fsprod: $(OBJS_PROD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

$(BINDIR)/fscons: $(OBJS_CONS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGETS)

cleanall: clean
	\rm -f $(OBJDIR)/*.o *~ *.a *.sock

PORT 				= 12345
ENDPOINT 			= 127.0.0.1
PROD_MOUNTPOINT 	= ./tmp/prod
CONS_MOUNTPOINT 	= ./tmp/cons

run_client: all
	$(BINDIR)/fspipe --port=$(PORT) --endpoint=$(ENDPOINT) -s $(PROD_MOUNTPOINT)

run_server: all
	$(BINDIR)/fspipe --port=$(PORT) -s $(CONS_MOUNTPOINT)

# run the client in debugging mode
debug_client: all
	$(BINDIR)/fspipe --port=$(PORT) --endpoint=$(ENDPOINT) --timeout=2000 -d -s $(PROD_MOUNTPOINT)

# run the server in debugging mode
debug_server: all
	$(BINDIR)/fspipe --port=$(PORT) --timeout=6000 -d -s $(CONS_MOUNTPOINT)

checkmount:
	mount | grep fspipe

unmount:
	fusermount -u $(PROD_MOUNTPOINT)
	fusermount -u $(CONS_MOUNTPOINT)

forceunmount:
	sudo umount -l $(PROD_MOUNTPOINT)
	sudo umount -l $(CONS_MOUNTPOINT)
