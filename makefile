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
OBJS_FSPIPE	=	$(OBJDIR)/main.o		\
				$(OBJDIR)/scfiles.o		\
				$(OBJDIR)/socketconn.o

OBJS_PROD 	= 	$(OBJDIR)/fsprod.o		\
				$(OBJDIR)/scfiles.o

OBJS_CONS 	= 	$(OBJDIR)/fscons.o		\
				$(OBJDIR)/scfiles.o

TARGETS	= $(BINDIR)/fspipe $(BINDIR)/fsprod $(BINDIR)/fscons

.PHONY: all clean cleanall mount_prod mount_cons debug_prod debug_cons usage checkmount unmount

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
$(BINDIR)/fspipe: $(OBJS_FSPIPE)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

$(BINDIR)/fsprod: $(OBJS_PROD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

$(BINDIR)/fscons: $(OBJS_CONS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGETS)

cleanall: clean
	\rm -f $(OBJDIR)/*.o *~ *.a *.sock

PROD_PORT 			= 12345
CONS_PORT 			= 6789
PROD_HOST 			= 127.0.0.1
CONS_HOST 			= 127.0.0.1
PROD_MOUNTPOINT 	= ./tmp/prod
CONS_MOUNTPOINT 	= ./tmp/cons

mount_prod: all
	$(BINDIR)/fspipe --port=$(PROD_PORT) --host=$(CONS_HOST) --remote_port=$(CONS_PORT) --timeout=6000 -s $(PROD_MOUNTPOINT)

mount_cons: all
	$(BINDIR)/fspipe --port=$(CONS_PORT) --host=$(PROD_HOST) --remote_port=$(PROD_PORT) --timeout=10000 -s $(CONS_MOUNTPOINT)

debug_prod: all
	$(BINDIR)/fspipe --port=$(PROD_PORT) --host=$(CONS_HOST) --remote_port=$(CONS_PORT) --timeout=6000 -o debug -s $(PROD_MOUNTPOINT)

debug_cons: all
	$(BINDIR)/fspipe --port=$(CONS_PORT) --host=$(PROD_HOST) --remote_port=$(PROD_PORT) --timeout=10000 -d -s $(CONS_MOUNTPOINT)

# run with help flag
usage: all
	$(BINDIR)/fspipe -h

checkmount:
	mount | grep fspipe

unmount:
	fusermount -u $(PROD_MOUNTPOINT)
	fusermount -u $(CONS_MOUNTPOINT)

forceunmount:
	sudo umount -l $(PROD_MOUNTPOINT)
	sudo umount -l $(CONS_MOUNTPOINT)
