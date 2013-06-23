#include <u.h>
#include <libc.h>
#include <aml.h>
#include "acpi.h"

#define ACPI_DEV_IBM	"IBM0068"
#define ACPI_DEV_LENOVO	"LEN0068"

#define	THINKPAD_HKEY_VERSION		0x0100

#define	THINKPAD_CMOS_VOLUME_DOWN	0x00
#define	THINKPAD_CMOS_VOLUME_UP		0x01
#define	THINKPAD_CMOS_VOLUME_MUTE	0x02
#define	THINKPAD_CMOS_BRIGHTNESS_UP	0x04
#define	THINKPAD_CMOS_BRIGHTNESS_DOWN	0x05

/* ACPI thinkpad driver */
int 
acpitp_match(char *id) {
	return (id && (!strcmp(id, ACPI_DEV_IBM) || !strcmp(id, ACPI_DEV_LENOVO)));
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
		tp_cmos(dev, THINKPAD_CMOS_BRIGHTNESS_DOWN);
		err[0] = '\0';
		return;
	}
	if(!strncmp(data, "up", len)) {
		tp_cmos(dev, THINKPAD_CMOS_BRIGHTNESS_UP);
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
