#include <u.h>
#include <libc.h>
#include <aml.h>
#include "acpidev.h"

typedef struct Tbl Tbl;

struct Tbl {
	uchar	sig[4];
	uchar	len[4];
	uchar	rev;
	uchar	csum;
	uchar	oemid[6];
	uchar	oemtid[8];
	uchar	oemrev[4];
	uchar	cid[4];
	uchar	crev[4];
	uchar	data[];
};

/* for each defined space */
Amlio* acpiio[8];

static Tbl *tbltab[64];

Tree *tree;

ushort
get16(uchar *p){
	return p[1]<<8 | p[0];
}

uint
get32(uchar *p){
	return p[3]<<24 | p[2]<<16 | p[1]<<8 | p[0];
}

uvlong
get64(uchar *p){
	uvlong u;

	u = get32(p+4);
	return u<<32 | get32(p);
}

int
amlio(uchar space, char op, void *buf, int off, int len){
	if(acpiio[space] == nil){
		/*print("space[%d] io not implemented\n", space);*/
		return -1;
	}
	if(op == 'R'){
		return acpiio[space]->read(acpiio[space], buf, off, len);
		/*print("amlio[R]: space=%d off=%ulld len=%d %ulld\n", space, off, len, buf[0]);*/
	}else
		return acpiio[space]->write(acpiio[space], buf, off, len);
}

void 
amldelay(int n) 
{
	sleep(n);
}

void*
amlalloc(int n){
	void *p;

	if((p = malloc(n)) == nil) 
		sysfatal("amlalloc: no memory");
	memset(p, 0, n);
	return p;
}

void
amlfree(void *p){
	free(p);
}

void*
gettables(int *len) {
	int fd, n, size;
	uvlong off;
	void *tables;
	uchar *p;

	size = 1024*1024;
	tables = malloc(size);
	p = (uchar*)tables;
	if((fd = open("/dev/acpitbls", OREAD)) != -1) {
		off = 0;
		while((n = read(fd, p, 1024)) > 0) {
			off += n;
			p += n;
			if(off >= size - 1024) {
				size *= 2;
				tables = realloc(tables, size);
				p = (uchar*)tables + off;
			}
		}
	} else {
		sysfatal("acpi tables not available");
	}
	*len = off;
	close(fd);
	return tables;
}

Tbl*
findtable(uchar *tp, char name[4], int len) {
	Tbl *t;
	uchar *ti;
	
	ti = tp;
	for(; tp < ti + len; tp += get32(t->len)) {
		t = (Tbl*)tp;
		if(memcmp(t->sig, name, 4) == 0)
			return t;
	}
	return nil;
}

static uint
tbldlen(Tbl *t){
	return get32(t->len) - sizeof(Tbl);
}

static int
enumini(void *dot, void *)
{
	void *m;
	void **p;
	uvlong sta;

	enum { 
		StaPresent = 1 << 0, 
		StaEnabled = 1 << 1,
		StaDevOk = 1 << 3 
	};

	if((m = amlwalk(dot, "^_STA")) == 0) {
		/* no _STA, call _INI */
		amleval(dot, "", nil);
		return 0;
	} else if(amleval(m, "", &p) < 0) {
		print("ini: _STA not working\n");
		return -1;
	}
	sta = amlint(p);
	if (sta & StaPresent) {
		/* _STA says present, call _INI */
		amleval(dot, "", nil);
	}
		
	return 0;
}

static char*
eisaid(void *v)
{
	static char id[8];
	ulong b, l;
	int i;

	if(amltag(v) == 's')
		return v;
	b = amlint(v);
	for(l = 0, i=24; i>=0; i -= 8, b >>= 8)
		l |= (b & 0xFF) << i;
	id[7] = 0;
	for(i=6; i>=3; i--, l >>= 4)
		id[i] = "0123456789ABCDEF"[l & 0xF];
	for(i=2; i>=0; i--, l >>= 5)
		id[i] = '@' + (l & 0x1F);
	return id;
}

static int
foundpdc(void *dot, void *)
{
	struct acpidev *dev;
	int i;
	char buf[15];

	for(i = 0; i < nelem(acpidev); i++) {
		if(!strcmp(acpidev[i].name, "cpu")) {
			if((dev = calloc(1, sizeof(*dev))) == nil)
				sysfatal("%r");
			memcpy((void*)dev, (void*)&acpidev[i], sizeof(*dev));
			dev->node = dot;
			memset(buf, 0, sizeof(buf));
			sprint(buf, "%s%d", dev->name, dev->unit);
			dev->dir = mkdir(tree->root, buf);
			if(!acpidev[i].attach(dev)) {
				print("fail %s\n", buf);
				removefile(dev->dir);
				free(dev);
			} else {
				acpidev[i].unit++;
				mkfile(dev->dir, "status", dev);
				if(dev->control)
					createfile(dev->dir, "ctl", "sys", 0222, dev);
				break;
			}
		}
	}
	return 1;
}

static int 
enumhid(void *dot, void *)
{
	void *p;
	char *id;
	struct acpidev *dev;
	char buf[15];
	int i;

	id = nil;
	p = nil;
	if(amleval(dot, "", &p) == 0)
		id = eisaid(p);

	for(i = 0; i < nelem(acpidev); i++) {
		if(id && acpidev[i].match(id)) {
			if((dev = calloc(1, sizeof(*dev))) == nil)
				sysfatal("%r");
			memcpy((void*)dev, (void*)&acpidev[i], sizeof(*dev));
			dev->node = dot;
			memset(buf, 0, sizeof(buf));
			sprint(buf, "%s%d", dev->name, dev->unit);
			dev->dir = mkdir(tree->root, buf);
			if(!acpidev[i].attach(dev)) {
				print("fail %s\n", buf);
				removefile(dev->dir);
				free(dev);
			} else {
				acpidev[i].unit++;
				if(dev->status)
					mkfile(dev->dir, "status", dev);
				if(dev->control)
					createfile(dev->dir, "ctl", "sys", 0222, dev);
				break;
			}
		}
	}

	return -1;
}

int
checksum(void *v, int n)
{
	uchar *p, s;

	s = 0;
	p = v;
	while(n-- > 0)
		s += *p++;
	return s;
}

void
run(void) {
	uchar *p, *tp;
	int len, l, i;
	Tbl *t;

	p = gettables(&len);
	amlinit();

	tp = p;

	if(t = findtable(tp, "ECDT", len - (tp - p))) {
			acpiec_init(nil, t->data);
	}
	tp = p;

	if(t = findtable(tp, "DSDT", len - (tp - p))) {
		amlload(t->data, tbldlen(t));
	} else {
		fprint(2, "DSDT not found\n");
	}

	tp = p;
	i = 0;
	while(t = findtable(tp, "SSDT", len - (tp - p))) {
			amlload(t->data, tbldlen(t));
			i++;
			tp += 4;
	} 
	/* we can't trust t->len, bruteforce search */
	if (!t) {
		while((tp < p + len)) {
			if(memcmp(tp, "SSDT", 4)) {
				tp += 1;
			} else {
				t = (Tbl*)tp;
				l = get32(t->len);
				if (l < len && !checksum(tp, l)) {
					amlload(t->data, tbldlen(t));
					tp += 4;
					i++;
				} else {
					tp += 1;
				}
			}
		}
	}
	if(i == 0)
		fprint(2, "SSDT not found\n");
	/* 
	 * Bad things happen when doing actual writes
	 */
	//setdummy(acpiio[EbctlSpace], 1);
	amlenum(amlroot, "_INI", enumini, nil);
	setdummy(acpiio[EbctlSpace], 0);

	/* look for other ACPI devices */
	amlenum(amlroot, "_HID", enumhid, nil);

	/* cpu power and throttling states */
	amlenum(amlroot, "_PDC", foundpdc, nil);
} 

void
usage(void)
{
	fprint(2, "usage: bla");
}

static void
fsread(Req *r) 
{
	struct acpidev *dev;
	char err[ERRMAX];
	char *s;

	dev = r->fid->file->aux;

	if(dev && dev->status) {
		s = dev->status(dev, r->fid->file);
		readstr(r, s);
		respond(r, nil);
		free(s);
	} else {
		err[0] = '\0';
		errstr(err, sizeof(err));
		respond(r, err);
	}	
}

static void
fswrite(Req *r) 
{
	struct acpidev *dev;
	char err[ERRMAX];

	dev = r->fid->file->aux;

	if(dev && dev->control) {
		r->ofcall.count = r->ifcall.count;
		dev->control(dev, r->ifcall.data, r->ifcall.count, err);
		if(err[0] != '\0')
			respond(r, err);
		else
			respond(r, nil);
	} else {
		err[0] = '\0';
		errstr(err, sizeof(err));
		respond(r, err);
	}
}

Srv fs = {
	.read=	fsread,
	.write= fswrite,
};

File *
mkdir(File *parent, char *name) {
	return createfile(parent, name, "sys", DMDIR|0555, nil);
}

File *
mkfile(File *parent, char *name, void *aux) {
	return createfile(parent, name, "sys", 0444, aux);
}

void
main(int argc, char **argv)
{
	ulong flag;
	char *mtpt;
	char err[ERRMAX];

	flag = 0;
	mtpt = "/mnt/acpi";
	ARGBEGIN{
	case 'D':
		chatty9p++;
		break;
	case 'm':
		mtpt = EARGF(usage());
		break;
	default:
		usage();
		break;
	}ARGEND;

	tree = fs.tree = alloctree("sys", "sys", DMDIR|0555, nil);

	err[0] = '\0';
	errstr(err, sizeof err);
	if(err[0])
		sysfatal("acpi: %s", err);

	run();
	postmountsrv(&fs, nil, mtpt, flag);
	exits(0);
}