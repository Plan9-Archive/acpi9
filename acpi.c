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
static struct acpiio* acpiio[8];

static Tbl *tbltab[64];

static ushort
get16(uchar *p){
	return p[1]<<8 | p[0];
}

static uint
get32(uchar *p){
	return p[3]<<24 | p[2]<<16 | p[1]<<8 | p[0];
}

static uvlong
get64(uchar *p){
	uvlong u;

	u = get32(p+4);
	return u<<32 | get32(p);
}

void 
acpi_read(uchar space, int off, int, uvlong *buf)
{
	//print("acpi_read: off 0x%x len %d\n", off, len);

	if (acpiio[space] != nil)
		*buf = acpiio[space]->read(acpiio[space], off);
}

void 
acpi_write(uchar space, int off, int, uvlong val)
{
	//print("acpiec_write: off 0x%x len %d\n", off, len);

	if (acpiio[space] != nil)
		acpiio[space]->write(acpiio[space], off, val);
}

void 
acpi_delay(int n) 
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
		else 
			print("[%c%c%c%c]\n", t->sig[0], t->sig[1], t->sig[2], t->sig[3]);
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
enumhid(void *dot, void *)
{
	void *p;
	char *id;
	struct acpidev *dev;
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
			acpidev[i].attach(dev);
			break;
		}
	}

	return -1;
}

/*
static int 
foundpss(void *dot, void *) {
	print("pss found\n");
	return 0;
}


static int
foundtss(void *dot, void *) {
	print("tss found\n");
	return 0;
}
*/
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
main() {
	uchar *p, *tp;
	int len, l, i;
	Tbl *t;

	p = gettables(&len);
	print("tables @ [0x%p, 0x%p], len %d\n", p, p+len, len);

	acpiio[EbctlSpace] = ioinit("/dev/acpiec");
	acpiio[MemSpace] = ioinit("/dev/acpimem");
	amlinit();

	tp = p;

	if(t = findtable(tp, "DSDT", len - (tp - p))) {
		amlload(t->data, tbldlen(t));
		print("found dsdt\n");
	}
	tp = p;
	i = 0;
	while(t = findtable(tp, "SSDT", len - (tp - p))) {
			amlload(t->data, tbldlen(t));
			print("found ssdt @ %p\n", t);
			i++;
			tp += 4;
	} 
	if (!t) {
		while((tp < p + len)) {
			if(memcmp(tp, "SSDT", 4)) {
				tp += 4;
			} else {
				t = (Tbl*)tp;
				l = get32(t->len);
				if (l < len && !checksum(tp, l)) {
					print("SSDT.%d@%p\n", i, t);
					/*if (i == 1 || i == 4)
						amldebug = 1;*/
					amlload(t->data, tbldlen(t));
					amldebug = 0;
					tp += 4; //get32(t->len);
					i++;
				} else {
					tp += 4;
				}
			}
		}
	}
	/* 
	 * Bad things happen when doing actual writes
	 */
	acpiio[EbctlSpace]->dummy = 1;
	amlenum(amlroot, "_INI", enumini, nil);
 	acpiio[EbctlSpace]->dummy = 0;

	/* look for other ACPI devices */

	amlenum(amlroot, "_HID", enumhid, nil);

	/*
	amlenum(amlroot, "_PSS", foundpss, nil);
	amlenum(amlroot, "_TSS", foundtss, nil);
	*/
	exits("");
} 