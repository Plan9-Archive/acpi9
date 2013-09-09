#include <auth.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

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

typedef struct fileio Fileio;
struct fileio {
	int fd, dummy;
};

extern void setdummy(Amlio*, int);

File* mkdir(File *, char *);
File* mkfile(File *, char *, void *);

/* utilities */
ushort get16(uchar *p);
uint get32(uchar *p);
uvlong get64(uchar *p);

int amlio(uchar, char, void*, int, int);