#include <u.h>
#include <libc.h>
#include <aml.h>

#include "acpi.h"

struct Gas {
	uchar 	space;
	uchar 	bw;
	uchar	bo;
	uchar 	size;
	uchar	addr[8];
};

struct Ecdt {
	struct Gas	ec_cmd;
	struct Gas 	ec_data;
	uchar 		uid[4];
	uchar 		gpe;
	uchar 		id[1];
};

struct ec {
	int cmd;
	int data;
	int ready;
} ec;

enum { 
	/* status bits */
	OutBufFull = 0x01, 
	InBufFull  = 0x02,
	DataIsCmd  = 0x08,
	Burst      = 0x10,
	SciEvt     = 0x20,
	SmiEvt     = 0x40,
	/* commands */
	EcRead 	   = 0x80,
	EcWrite    = 0x81,
	EcBurstEn  = 0x82,
	EcBurstDis = 0x83,
	EcQuery	   = 0x84,
};

static uchar 
inb(int off){
	uvlong val;

	amlio(IoSpace, 'R', &val, off, 1);
	return val;
}

static void
outb(int off, uchar v){
	uvlong val;

	val = v;
	amlio(IoSpace, 'W', &val, off, 1);
}

static void
acpiec_wait(uchar mask, uchar val)
{
	uchar stat;

	while((stat = inb(ec.cmd) & mask) != val) {
		if (stat & Burst)
			amldelay(1);
	}
}

static uchar
acpiec_read_data(void)
{
	acpiec_wait(OutBufFull, OutBufFull);
	return inb(ec.data);
}

static void
acpiec_write_data(uchar val)
{
	acpiec_wait(InBufFull, 0);
	outb(ec.data, val);
}

static void
acpiec_write_cmd(uchar val)
{
	acpiec_wait(InBufFull, 0);
	outb(ec.cmd, val);
}

static uchar
acpiec_read(uchar addr)
{
	if (!ec.ready)
		return 0;
	acpiec_write_cmd(EcRead);
	acpiec_write_data(addr);
	
	return acpiec_read_data();
}

static void
acpiec_write(uchar addr, uvlong val)
{
	int i;
	uchar *v = (uchar*)&val;

	if (!ec.ready)
		return;

	for(i = 0; i < sizeof(uvlong); i++) {
		acpiec_write_cmd(EcWrite);
		acpiec_write_data(addr + i); // ?
		acpiec_write_data(v[i]);
	}
}

int
ecread(Amlio*, void *p, int addr, int){
	*(uchar*)p = acpiec_read(addr);
	return 1;
}

int
ecwrite(Amlio *io, void *p, int addr, int){
	if(((Fileio*)io->aux)->dummy)
		return -1;
	uvlong val = *(uvlong*)p;
	acpiec_write(addr, val);
	return 1;
}
	
static int
acpiec_getreg(unsigned char *buf, int size, int *type, int *addr)
{
	enum { ResTypeMask = 0x80, ResLengthMask = 0x07,
		   ResTypeIoPort = 0x47, ResTypeEndTag = 0x79 };

	enum { SystemIoSpace = 1 };

	int len, hlen;

	if (size <= 0)
		return 0;
	if (*buf & ResTypeMask) {
		/* large resource */
		if (size < 3)
			return 1;
		len = (int)buf[1] + 256 * (int)buf[2];
		hlen = 3;
	} else {
		/* small resource */
		len = buf[0] & ResLengthMask;
		hlen = 1;
	}
	if (*buf != ResTypeIoPort)
		return 0;

	if (size < hlen + len)
		return 0;
	
	*type = SystemIoSpace;
	*addr = (int)buf[2] + 256 * (int)buf[3];

	return (hlen + len);
}

void
acpiec_init(void *dot, void *tbl)
{
	void *m;
	char *buf;
	void *p;
	int gpe;
	int rv, size, t;
	int ec_sc, ec_data;
	struct Ecdt *ecdt = tbl;

	if(ec.ready)
		return;
	if(ecdt != nil) {
		ecdt = (struct Ecdt*)tbl;
		ec_sc = get64(ecdt->ec_cmd.addr);
		ec_data = get64(ecdt->ec_data.addr);
		gpe = ecdt->gpe;
		goto done;
	}

	if((m = amlwalk(dot, "_GPE")) == 0) {
		print("ec: no _GPE\n");
		return;
	}
	if(amleval(m, "", &p) < 0) {
		print("ec: _GPE not working\n");
		return;
	}

	gpe = amlint(amlval(p));

	if((m = amlwalk(dot, "_CRS")) == 0) {
		print("ec: no _CRS\n");
		return;
	}
	if(amleval(m, "", &buf) < 0) {
		print("ec: _CRS not working\n");
		return;
	}
	if(amltag(buf) != 'b') {
		print("ec: unknown _CRS type\n");
		return;
	}
	size = amllen(buf);
	rv = acpiec_getreg(amlval(buf), size, &t, &ec_data);
	//print("getreg: rv=%d t=%d ec_data=0x%x\n", rv, t, ec_data);

	buf += rv;
	size -= rv;

	rv = acpiec_getreg(amlval(buf), size, &t, &ec_sc);
	//print("getreg: rv=%d t=%d ec_sc=0x%x\n", rv, t, ec_sc);

	buf += rv;
	size -= rv;

	if(size != 2 || *buf != 0x79) {
		print("no crs endtag\n");
	}

	/* register */
	enum { RegTypeEC = 3 };
	if((m = amlwalk(dot, "_REG")) == 0) {
		print("ec: no _REG\n");
		return;
	}
	if(amleval(m, "ii", RegTypeEC, 1, nil) < 0) {
		print("ec: _REG not working\n");
		return;
	}	
done:
/*	if(ioalloc(ec_data, 1, 0, "ec_data") < 0)
		print("ec: ec_data port space in use");
	if(ioalloc(ec_sc, 1, 0, "ec_sc") < 0)
		print("ec: ec_sc port space in use"); */
	print("ec: status: %d gpe: %d data: 0x%x s/c: 0x%x \n",
		inb(ec_sc), gpe, ec_data, ec_sc);	
	ec.cmd = ec_sc;
	ec.data = ec_data;
	ec.ready = 1;
}