#include <auth.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

/* taken from aml.c, should be exported by aml.h, but well... */
enum {
	MemSpace	= 0x00,
	IoSpace		= 0x01,
	PcicfgSpace	= 0x02,
	EbctlSpace	= 0x03,
	SmbusSpace	= 0x04,
	CmosSpace	= 0x05,
	PcibarSpace	= 0x06,
	IpmiSpace	= 0x07,
};

/* drivers */
struct acpidev;

struct acpidev {
	char 	name[10];
	int 	unit;
	int		(*match)(char *);
	int	 	(*attach)(struct acpidev *);
	char* 	(*status)(struct acpidev *, File *);
	void 	(*control)(struct acpidev *, char *, u32int, char *);
	File 	*dir;
	void 	*node;
	void 	*data;
};

/* talk to the kernel */
struct acpiio;

struct acpiio {
	int fd, dummy;
	uchar (*read)(struct acpiio *, uint);
	void  (*write)(struct acpiio *, uint, uvlong);
};

extern struct acpiio* ioinit(char *);

File* mkdir(File *, char *);
File* mkfile(File *, char *, void *);

/* utilities */
ushort
get16(uchar *p);
uint
get32(uchar *p);
uvlong
get64(uchar *p);