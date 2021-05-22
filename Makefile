VERSION = 1.0
PREFIX = /usr/local

VPATH = src

MTXROOMS_PROGNAME = mtxrooms
BUILDDIR = build
OBJDIR = $(BUILDDIR)/obj

MTXROOMS_PROG = $(BUILDDIR)/$(MTXROOMS_PROGNAME)
MTXROOMS_SRC = mtxrooms.c api.c parse.c list.c
MTXROOMS_OBJ = $(addprefix $(OBJDIR)/,$(MTXROOMS_SRC:.c=.o))

INCS = 
LIBS = -ljson-c -lcurl
CFLAGS = -g $(INCS)
LDFLAGS = $(LIBS)

GDB_INIT = init.gdb

$(MTXROOMS_PROG): $(OBJDIR) $(MTXROOMS_OBJ)
	$(CC) -o $@ $(MTXROOMS_OBJ) $(LDFLAGS)

$(MTXROOMS_OBJ): $(OBJDIR)/%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(BUILDDIR)

#install: $(PROG)
#	mkdir -p $(DESTDIR)$(PREFIX)/bin
#	cp -f $(PROG) $(DESTDIR)$(PREFIX)/bin
#	chmod 755 $(DESTDIR)$(PREFIX)/bin/$(PROG)
#uninstall:
#	rm -f $(DESTDIR)$(PREFIX)/bin/$(PROG)

drun: $(MTXROOMS_PROG)
	gdb -x $(GDB_INIT) $(BUILDDIR)/$(MTXROOMS_PROGNAME)

.PHONY: clean install uninstall drun
