include config.mk

PROG = smc
SRC = main.c http.c
OBJ = $(addprefix obj/,$(SRC:c=o))
OBJDIR = obj

INCS = -I.
LIBS = -ljson-c

CFLAGS = -g $(INCS)
LDFLAGS = $(LIBS)

$(PROG): $(OBJDIR) $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

$(OBJ): $(OBJDIR)/%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -f $(PROG)
	rm -f $(OBJ)
	rmdir obj

install: $(PROG)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(PROG) $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/$(PROG)
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(PROG)

.PHONY: clean install uninstall
