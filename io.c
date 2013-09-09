#include <u.h>
#include <libc.h>
#include <aml.h>

#include "acpi.h"

extern int ecread(Amlio*, void*, int, int);
extern int ecwrite(Amlio*, void*, int, int);

static int
memread(Amlio *io, void *p, int len, int addr) {
	print("memread: 0x%x\n", addr);
	return pread(((Fileio*)io->aux)->fd, p, len, addr);
}

static int
memwrite(Amlio *io, void *p, int len, int addr) {
	if(((Fileio*)io->aux)->dummy)
		return -1;
	return pwrite(((Fileio*)io->aux)->fd, p, len, addr);
}

static int
portread(Amlio *io, void *p, int, int addr) {
	return pread(((Fileio*)io->aux)->fd, p, 1, addr);
}

static int
portwrite(Amlio *io, void *p, int, int addr) {
	if(((Fileio*)io->aux)->dummy)
		return -1;
	return pwrite(((Fileio*)io->aux)->fd, p, 1, addr);
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
	Fileio *fio;

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
			print("amlmapio ebctl\n!");
			io->read = ecread;
			io->write = ecwrite;
			if((fio = calloc(1, sizeof(*fio))) == nil)
				sysfatal("ioinit: %r");
			io->aux = fio;
			fio->dummy = 1;
			registerio(io);
			print("amlmalio ebctl exit\n");
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