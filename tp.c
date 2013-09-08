#include <u.h>
#include <libc.h>
#include <aml.h>
#include "acpi.h"

#define	THINKPAD_HKEY_VERSION		0x0100

enum {
	BrightnessUp = 0x04, 
	BrightnessDown
};

enum { 
	FanLowByte = 0x84,
	FanHighByte = 0x85
};

#define STATUSLEN 50

/* ACPI thinkpad driver */
int 
acpitp_match(char *id)
{
	return (id && (!strcmp(id, "IBM0068") || !strcmp(id, "LEN0068")));
}

static void
tp_cmos(struct acpidev *dev, uchar cmd)
{
	void *dot, *m, *p;

	dot = dev->node;
	if((m = amlwalk(dot, "\\UCMS")) == 0)
		return;
	amleval(m, "i", cmd, &p);
}

static void 
control(struct acpidev *dev, char *data, u32int len, char *err)
{
	if(!strncmp(data, "brighter", len-1)) {
		tp_cmos(dev, BrightnessUp);
		goto success;
	}
	if(!strncmp(data, "dimmer", len-1)) {
		tp_cmos(dev, BrightnessDown);
		goto success;
	}
	snprint(err, ERRMAX, "bad value");
	return;
success:
	err[0] = '\0';
	return;
}

static char *
status(struct acpidev *, File *)
{
	uvlong hi, lo;
	char *buf;

	if((buf = calloc(1, STATUSLEN)) == 0)
		sysfatal("%r");

	amlio(EbctlSpace, 'R', &lo, FanLowByte, 1);
	amlio(EbctlSpace, 'R', &hi, FanHighByte, 1);
	snprint(buf, STATUSLEN, "Fan RPM: %ulld\n", (hi << 8) | lo);
	return buf;
}

int 
acpitp_attach(struct acpidev *dev)
{
	void *m, *dot, *p;

	dot = dev->node;
	if((m = amlwalk(dot, "MHKV")) == 0)
		return 0;

	if(amleval(m, "", &p) < 0)
		return 0;

	if(amlint(p) != THINKPAD_HKEY_VERSION)
			return 0;
	dev->control = control;
	dev->status = status;
	return 1;
}
