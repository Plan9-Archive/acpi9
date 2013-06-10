#include <u.h>
#include <libc.h>
#include <aml.h>
#include "acpi.h"

/* ACPI AC driver */

struct ac {
	void *psr_node;
};

enum { Offline, Online };

#define STATUSLEN 10

int 
acpiac_match(char *id) {
	return (id && (!strcmp(id, "ACPI0003")));
}

static char *
status(struct acpidev *dev, File *) {
	void **p;
	void *m;
	char *buf;

	m = ((struct ac*)dev->data)->psr_node;

	if(m == nil) {
		fprint(2, "must not happen\n");
		return nil;
	}
	if(amleval(m, "", &p) < 0) {
		fprint(2, "ac: _PSR not working\n");
		return nil;
	}
	if((buf = calloc(1, STATUSLEN)) == nil) {
		fprint(2, "%r");
		return nil;
	}
	switch(amlint(p)) {
		case Online:
			snprint(buf, STATUSLEN, "online");
			break;
		case Offline:
			snprint(buf, STATUSLEN, "offline");
			break;
		default:
			snprint(buf, STATUSLEN, "unknown");
	}
	return buf;
}

int
acpiac_attach(struct acpidev *dev) {
	void *m, *dot;
	struct ac *ac;

	dot = dev->node;
	if((m = amlwalk(dot, "_PSR")) == 0) {
		print("ac: no _PSR\n");
		return 0;
	}
	if((ac = malloc(sizeof(*ac))) == 0)
		sysfatal("%r");
	ac->psr_node = m;
	dev->data = ac;
	dev->status = status;
	return 1;
}
