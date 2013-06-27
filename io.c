#include <u.h>
#include <libc.h>
#include <aml.h>

#include "acpi.h"

static void
io_read(struct acpiio *io, uvlong addr, uvlong len, uchar *p)
{
	if(pread(io->fd, p, len, addr) == -1)
		sysfatal("read: %r");
}

static void
io_write(struct acpiio *io, uvlong addr, uvlong val)
{
	if(io->dummy)
		return;
	if(pwrite(io->fd, &val, sizeof(uvlong), addr) == -1)
		sysfatal("write: %r");
}

struct acpiio*
ioinit(char *name)
{
	int fd;
	struct acpiio *io;

	if((fd = open(name, ORDWR)) != -1) {
		if((io = calloc(1, sizeof(*io))) == nil)
			sysfatal("ioinit: %r");
		io->fd = fd;
		io->read = io_read;
		io->write = io_write;
		return io;
	} 
	return nil;
}


