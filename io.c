#include <u.h>
#include <libc.h>
#include <aml.h>

#include "acpi.h"

extern int ecread(Amlio*, void*, int, int);
extern int ecwrite(Amlio*, void*, int, int);

static int
memread(Amlio *io, void *p, int addr, int len) {
	print("memread: 0x%x\n", addr);
	return pread(((Fileio*)io->aux)->fd, p, len, addr);
}

static int
memwrite(Amlio *io, void *p, int addr, int len) {
	if(((Fileio*)io->aux)->dummy)
		return -1;
	uvlong val = *(uvlong*)p;
	print("memwrite: 0x%x\n", addr);
	return pwrite(((Fileio*)io->aux)->fd, &val, len, addr);
}

static int
portread(Amlio *io, void *p, int addr, int) {
	return pread(((Fileio*)io->aux)->fd, p, 1, addr);
}

static int
portwrite(Amlio *io, void *p, int addr, int) {
	if(((Fileio*)io->aux)->dummy)
		return -1;
	uvlong val = *(uvlong*)p;
	return pwrite(((Fileio*)io->aux)->fd, &val, 1, addr);
}

static int
ioinit(Amlio *io, char *name) {
	int fd;
	Fileio *fio;

	if((fd = open(name, ORDWR)) != -1) {
		if((fio = calloc(1, sizeof(*fio))) == nil)
			sysfatal("ioinit: %r");
		fio->fd = fd;
		io->aux = fio;
		return 0;
	} 
	return -1;
}

static void
registerio(Amlio *io) {
	extern Amlio* acpiio[8];

	acpiio[io->space] = io;
}

int amlmapio(Amlio *io) {
	switch(io->space) {
		case MemSpace:
			if(ioinit(io, "/dev/acpimem") != -1) {
				io->read = memread;
				io->write = memwrite;
				registerio(io);
				return 0;
			}
			break;
		case IoSpace:
			if(ioinit(io, "/dev/iob") != -1) {
				io->read = portread;
				io->write = portwrite;
				registerio(io);
				return 0;
			}
			break;
		case EbctlSpace:
			/* prey this shit is ready */
			io->read = ecread;
			io->write = ecwrite;
			((Fileio*)io->aux)->dummy = 1;
			registerio(io);
			return 0;
		default:
			print("unknown I/O space %d", io->space);
	}
	return -1;
}

void amlunmapio(Amlio *io) {
	Fileio *fio;

	fio = (Fileio*)io->aux;
	if (fio != nil) {
		close(fio->fd);
		free(fio);
	}
}

void setdummy(Amlio *io, int val) {
	((Fileio*)io->aux)->dummy = val;
}