CC		= gcc -std=c99
CFLAGS	= -g -Wall -pedantic -D_POSIX_C_SOURCE=200809L -Wextra 		\
		-Wwrite-strings -Wstrict-prototypes -Wold-style-definition 	\
		-Wformat=2 -Wno-unused-parameter -Wshadow 					\
		-Wredundant-decls -Wnested-externs -Wmissing-include-dirs 	\
		-D_FILE_OFFSET_BITS=64 `pkg-config fuse --cflags` # required by FUSE

SRCDIR  	= src
INCDIR		= include
OBJDIR   	= obj
BINDIR   	= bin
TSTDIR   	= test
LIBDIR      = libs

INCLUDES 	= -I $(INCDIR)
LDFLAGS 	= `pkg-config fuse --libs` -L $(LIBDIR) # required by FUSE
LIBS		= -lpthread

# dependencies for netpipefs executable
OBJS_NETPIPEFS =$(OBJDIR)/main.o		\
				$(OBJDIR)/scfiles.o		\
				$(OBJDIR)/socketconn.o	\
				$(OBJDIR)/netpipefs_socket.o\
				$(OBJDIR)/dispatcher.o	\
				$(OBJDIR)/options.o		\
				$(OBJDIR)/netpipefs_file.o	\
				$(OBJDIR)/cbuf.o		\
				$(OBJDIR)/openfiles.o	\
				$(OBJDIR)/icl_hash.o	\
				$(OBJDIR)/utils.o

TARGETS	= $(BINDIR)/netpipefs
TESTS	= $(BINDIR)/utils.test $(BINDIR)/cbuf.test $(BINDIR)/openfiles.test

.PHONY: all test clean cleanall usage run_test checkmount unmount forceunmount mount_prod mount_cons debug_prod debug_cons

all: $(BINDIR) $(OBJDIR) $(INCDIR) $(TARGETS)

test: $(BINDIR) $(OBJDIR) $(TESTS)

$(BINDIR):
	mkdir $(BINDIR)

$(OBJDIR):
	mkdir $(OBJDIR)

$(INCDIR):
	mkdir $(INCDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(INCDIR)/%.h
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(OBJDIR)/%.test.o: $(TSTDIR)/%.test.c $(TSTDIR)/testutilities.h
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BINDIR)/netpipefs: $(OBJS_NETPIPEFS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS) $(LIBS)

$(BINDIR)/%.test: $(OBJDIR)/%.test.o $(OBJDIR)/%.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS) $(LIBS)

$(BINDIR)/openfiles.test: $(OBJDIR)/openfiles.test.o $(OBJDIR)/openfiles.o $(OBJDIR)/netpipefs_file.o $(OBJDIR)/cbuf.o $(OBJDIR)/icl_hash.o $(OBJDIR)/scfiles.o $(OBJDIR)/netpipefs_socket.o $(OBJDIR)/socketconn.o $(OBJDIR)/utils.o $(OBJDIR)/options.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	rm -f $(TARGETS) $(TESTS)

cleanall: clean
	\rm -f $(OBJDIR)/*.o *~ *.a *.sock

# run with help flag
usage: all
	$(BINDIR)/netpipefs -h

# run the test suite
run_test: test
	@$(foreach src, $(TESTS), $(src); )

checkmount:
	mount | grep netpipefs

unmount:
	fusermount -u $(PROD_MOUNTPOINT)
	fusermount -u $(CONS_MOUNTPOINT)

forceunmount:
	sudo umount -l $(PROD_MOUNTPOINT)
	sudo umount -l $(CONS_MOUNTPOINT)

PROD_PORT 			= 12345
CONS_PORT 			= 6789
PROD_HOST 			= localhost
CONS_HOST 			= 127.0.0.1
PROD_MOUNTPOINT 	= ./tmp/prod
CONS_MOUNTPOINT 	= ./tmp/cons

mount_prod: all
	$(BINDIR)/netpipefs -p $(PROD_PORT) --hostip=$(CONS_HOST) --hostport=$(CONS_PORT) --pipecapacity=2048 --timeout=6000 $(PROD_MOUNTPOINT)

mount_cons: all
	$(BINDIR)/netpipefs --port=$(CONS_PORT) --hostip=$(PROD_HOST) --hostport=$(PROD_PORT) --timeout=10000 --pipecapacity=4096 $(CONS_MOUNTPOINT)

debug_prod: all
	$(BINDIR)/netpipefs -p $(PROD_PORT) --hostip=$(CONS_HOST) --hostport=$(CONS_PORT) --pipecapacity=2048 --timeout=6000 --debug -s $(PROD_MOUNTPOINT)

debug_cons: all
	$(BINDIR)/netpipefs --port=$(CONS_PORT) --hostip=$(PROD_HOST) --hostport=$(PROD_PORT) --timeout=10000 --pipecapacity=4096 -d -s $(CONS_MOUNTPOINT)
