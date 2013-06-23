#include <u.h>
#include <libc.h>
#include <aml.h>
#include "acpi.h"

#define	THINKPAD_HKEY_VERSION		0x0100

enum { VolumeDown, 
	   VolumeUp, 
	   VolumeMute,
	   BrightnessUp = 0x04, 
	   BrightnessDown };

/* ACPI thinkpad driver */
int 
acpitp_match(char *id) {
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
control(struct acpidev *dev, char *data, u32int len, char *err) {
	if(!strncmp(data, "down", len)) {
		tp_cmos(dev, BrightnessDown);
		err[0] = '\0';
		return;
	}
	if(!strncmp(data, "up", len)) {
		tp_cmos(dev, BrightnessUp);
		err[0] = '\0';
		return;
	}
	snprint(err, ERRMAX, "bad value");
}

int 
acpitp_attach(struct acpidev *dev) {
	void *m, *dot, *p;

	dot = dev->node;
	if((m = amlwalk(dot, "MHKV")) == 0)
		return 0;

	if(amleval(m, "", &p) < 0)
		return 0;

	if(amlint(p) != THINKPAD_HKEY_VERSION)
			return 0;
	dev->control = control;
	return 1;
}
