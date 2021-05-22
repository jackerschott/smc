BUILDDIR = build
MTXMSG_NAME = mtxmsg

GDB_INIT = init.gdb

compile: $(BUILDDIR)
	ninja -C $(BUILDDIR)

$(BUILDDIR):
	meson $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)

drun: $(MTXROOMS_PROG)
	gdb -x $(GDB_INIT) $(BUILDDIR)/$(MTXMSG_NAME)

.PHONY: clean drun compile
