#include <u.h>
#include <libc.h>
#include <aml.h>
#include "acpi.h"

/* ACPI battery driver */

enum { 
	BatteryPresent = (1L << 4),
	BifUnknown = 0xffffffff
}; 

#define STATUSLEN 512
enum { StringStart = 9 };

char *bif_names[] = {
	"power unit",
	"capacity", 
	"last capacity",
	"technology",
	"voltage",
	"warning capacity",
	"low capacity",
	"cap. granularity",
	"cap. granularity",
	"model",
	"serial",
	"type",
	"oem"
};

char *bst_names[] = {
	"cur.state",
	"cur.rate",
	"cur.capacity",
	"cur.voltage"
};

int 
acpibat_match(char *id)
{
	return (id && (!strcmp(id, "PNP0C0A")));
}

static char *
status(struct acpidev *dev, File *f)
{
	void *dot;
	uvlong sta;
	void *m, **a, *p;
	char *buf, *bp;
	int i, cap;

	dot = dev->node;
    if((m = amlwalk(dot, "_STA")) == 0) { 
		fprint(2, "bat: no _STA\n"); 
		return 0; 
    } 
    if(amleval(m, "", &p) < 0) { 
    	fprint(2, "bat: _STA not working\n"); 
        return 0; 
    }
    sta = amlint(p); 
    if((sta & BatteryPresent) == 0) {
			 if(dev->data == f) {
				buf = calloc(1, sizeof("0 0"));
				sprint(buf, "0 0 not present");
				return buf;
			 }
			 buf = calloc(1, sizeof("not present")+1);
			 sprint(buf, "not present");
			 return buf;
    }
	if((m = amlwalk(dot, "_BIF")) == 0) { 
         fprint(2, "bat: no _BIF\n"); 
		 return nil;
    }
    if(amleval(m, "", &p) < 0) { 
         fprint(2, "bat: _BIF not working\n"); 
		 return nil;
    }
    a = amlval(p); 
	bp = buf = calloc(1, STATUSLEN);
	if (dev->data == f) {
		/* save capacity */
		cap = amlint(a[2]);
		goto bst;
	}
	for(i = 0; i < nelem(bif_names); i++) {
		if(i < StringStart)
			bp += sprint(bp, "%s:\t%ulld\n", bif_names[i], amlint(a[i]));
		else
			bp += sprint(bp, "%s:\t%s\n", bif_names[i], (char*)amlval(a[i]));
	}
bst:
    if((m = amlwalk(dot, "_BST")) == 0) { 
    	fprint(2, "bat: no _BST\n"); 
		goto end;
    } 
    if(amleval(m, "", &p) < 0) { 
    	fprint(2, "bat: _BST not working\n"); 
    	goto end; 
    }        
    a = amlval(p); 
	if(dev->data == f)
		goto apmemu;
	for(i = 0; i < nelem(bst_names); i++)
		bp += sprint(bp, "%s:\t%ulld\n", bst_names[i], amlint(a[i]));
end:
	return buf;
apmemu:
	sprint(bp, "%ulld%% %ulld", 100*amlint(a[2])/cap, 
			amlint(a[1]) ? amlint(a[2])/amlint(a[1]) : 0);
	return buf;
}

int 
acpibat_attach(struct acpidev *dev)
{
    void *m, *dot, *p; 

	dot = dev->node;
    if((m = amlwalk(dot, "_STA")) == 0) { 
		fprint(2, "bat: no _STA\n"); 
		return 0; 
    } 
    if(amleval(m, "", &p) < 0) { 
    	fprint(2, "bat: _STA not working\n"); 
        return 0; 
    } 
	dev->status = status;
	/* this file is used to emulate apm battery file, mount it 
	 * at /mnt/apm
	 */
	dev->data = createfile(dev->dir, "battery", "sys", 0444, dev);
	return 1;
}
