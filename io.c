#include <u.h>
#include <libc.h>
#include <aml.h>

#include "acpi.h"

extern int ecread(Amlio*, void*, int, int);
extern int ecwrite(Amlio*, void*, int, int);

static int
portread(Amlio *io, void *p, int, int addr) {
	if(((Fileio*)io->aux)->dummy)
		return -1;
	//print("io->off = %ullx, addr = %d\n", io->off, addr);
	return pread(((Fileio*)io->aux)->fd, p, 1, addr);
}

static int
portwrite(Amlio *io, void *p, int, int addr) {
	if(((Fileio*)io->aux)->dummy)
		return -1;
	//print("io->off = %ullx, addr = %d\n", io->off, addr);
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

static void
unregister(Amlio *io) {
	extern Amlio* acpiio[8];

	acpiio[io->space] = nil;

}

static int
registered(Amlio *io) {
	extern Amlio* acpiio[8];

	return acpiio[io->space] != nil;
}

int memmap(Amlio *io) {
	int fd;
	uchar *buf;
	Fileio *fio;

	if (io->aux) {
		fio = (Fileio*)io->aux;
		fd = fio->fd;
	} else {
		if((fio = calloc(1, sizeof(*fio))) == nil)
			sysfatal("ioinit: %r");
		io->aux = fio;
		if ((fio->fd = fd = open("/dev/acpimem", ORDWR)) == -1)
			return -1;
	}

	if((buf = calloc(1, io->len)) == nil)
		sysfatal("ioinit: %r");
	io->va = buf;
	pread(fd, buf, io->len, io->off);
	return 0;
}

int amlmapio(Amlio *io) {
	Fileio *fio;

	switch(io->space) {
		case MemSpace:
			if (registered(io)) {
				unregister(io);
			}
			return memmap(io);
		case IoSpace:
			if (registered(io)) {
				unregister(io);
			}
			if(ioinit(io, "/dev/iob") != -1) {
				io->read = portread;
				io->write = portwrite;
				((Fileio*)io->aux)->dummy = 1;
				registerio(io);
				return 0;
			}
			break;
		case EbctlSpace:
			if (registered(io)) {
				unregister(io);
			}
			/* prey this shit is ready */
			io->read = ecread;
			io->write = ecwrite;
			if((fio = calloc(1, sizeof(*fio))) == nil)
				sysfatal("ioinit: %r");
			io->aux = fio;
			fio->dummy = 1;
			registerio(io);
			return 0;
		default:
			print("Not supported I/O space %d\n", io->space);
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