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

TARGETS	= $(BINDIR)/fspipe

.PHONY: all clean cleanall run_client run_server debug_client debug_server test checkmount umnount umount

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

clean:
	rm -f $(TARGETS)

cleanall: clean
	\rm -f $(OBJDIR)/*.o *~ *.a *.sock

PORT 				= 12345
ENDPOINT 			= 127.0.0.1
CLIENT_MOUNTPOINT 	= ./tmp/client
SERVER_MOUNTPOINT 	= ./tmp/server

run_client: all
	$(BINDIR)/fspipe --port=$(PORT) --endpoint=$(ENDPOINT) -s $(CLIENT_MOUNTPOINT)

run_server: all
	$(BINDIR)/fspipe --port=$(PORT) -s $(SERVER_MOUNTPOINT)

debug_client: all
	$(BINDIR)/fspipe --port=$(PORT) --endpoint=$(ENDPOINT) -d -s $(CLIENT_MOUNTPOINT) &

debug_server: all
	$(BINDIR)/fspipe --port=$(PORT) -d -s $(SERVER_MOUNTPOINT) &

checkmount:
	mount | grep fspipe

test: checkmount
	@printf "[TEST] client writes into $(CLIENT_MOUNTPOINT)/testfile.txt "
	@echo "Testing client write callback" > $(CLIENT_MOUNTPOINT)/testfile.txt
	@printf " -> done\n"
	@printf "[TEST] server reads from $(SERVER_MOUNTPOINT)/testfile.txt"
	@cat $(SERVER_MOUNTPOINT)/testfile.txt
	@printf " -> done\n"

unmount:
	fusermount -u ./tmp/client
	fusermount -u ./tmp/server

forceunmount:
	sudo umount -l ./tmp/client
	sudo umount -l ./tmp/server
