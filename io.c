#include <u.h>
#include <libc.h>
#include <aml.h>

#include "acpi.h"

static void
memread(struct acpiio *io, uvlong addr, uvlong len, uchar *p) {
	if(pread(io->fd, p, len, addr) == -1)
		sysfatal("read: %r");
}

static void
memwrite(struct acpiio *io, uvlong addr, uvlong len, uvlong val) {
	if(io->dummy)
		return;
	if(pwrite(io->fd, &val, len, addr) == -1)
		sysfatal("write: %r");
}

static void
portread(struct acpiio *io, uvlong addr, uvlong, uchar *p) {
	if(pread(io->fd, p, 1, addr) != 1)
		sysfatal("read: %r");
}

static void
portwrite(struct acpiio *io, uvlong addr, uvlong, uvlong val) {
	if(io->dummy)
		return;
	if(pwrite(io->fd, &val, 1, addr) != 1)
		sysfatal("write: %r");
}

static struct acpiio*
ioinit(char *name) {
	int fd;
	struct acpiio *io;

	if((fd = open(name, ORDWR)) != -1) {
		if((io = calloc(1, sizeof(*io))) == nil)
			sysfatal("ioinit: %r");
		io->fd = fd;
		return io;
	} 
	return nil;
}

AcpiIo*
meminit(char *file){
	AcpiIo *io;

	if((io = ioinit(file)) == nil)
		return nil;
	io->write = memwrite;
	io->read = memread;
	return io;
}

AcpiIo*
portinit(char *file){
	AcpiIo *io;

	if((io = ioinit(file)) == nil)
		return nil;
	io->write = portwrite;
	io->read = portread;
	return io;
}