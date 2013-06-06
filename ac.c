#include <u.h>
#include <libc.h>
#include <aml.h>
#include "acpi.h"

/* ACPI AC driver */

struct ac {
	void *psr_node;
};

int 
acpiac_match(char *id) {
	return (id && (!strcmp(id, "ACPI0003")));
}

static int
acpiacstatus(struct acpidev *dev) {
	void **p;
	void *m;

	m = ((struct ac*)dev->data)->psr_node;

	if(m == nil) {
		print("must not happen\n");
		return 0;
	}
	if(amleval(m, "", &p) < 0) {
		print("ac: _PSR not working\n");
		return 0;
	}
	return amlint(p);
}

int
acpiac_attach(struct acpidev *dev) {
	uvlong sta;
	void *m, *dot;
	enum { Offline, Online };
	struct ac *ac;

	dot = dev->node;
	if((m = amlwalk(dot, "_PSR")) == 0) {
		print("ac: no _PSR\n");
		return -1;
	}
	if((ac = malloc(sizeof(*ac))) == 0)
		sysfatal("%r");
	ac->psr_node = m;
	dev->data = ac;
	sta = acpiacstatus(dev);
	if(sta == Online)
		print("ac: AC online\n");
	else if(sta == Offline)
		print("ac: AC offline\n");
	else
		print("ac: status unknown\n");
	return 0;
}
