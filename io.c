#include <u.h>
#include <libc.h>
#include <aml.h>

#include "acpi.h"

static int
memread(struct acpiio *io, void *p, int addr, int len) {
	print("memread: 0x%x\n", addr);
	return pread(io->fd, p, len, addr);
}

static int
memwrite(struct acpiio *io, void *p, int addr, int len) {
	if(io->dummy)
		return -1;
	uvlong val = *(uvlong*)p;
	print("memwrite: 0x%x\n", addr);
	return pwrite(io->fd, &val, len, addr);
}

static int
portread(struct acpiio *io, void *p, int addr, int) {
	return pread(io->fd, p, 1, addr);
}

static int
portwrite(struct acpiio *io, void *p, int addr, int) {
	if(io->dummy)
		return -1;
	uvlong val = *(uvlong*)p;
	return pwrite(io->fd, &val, 1, addr);
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