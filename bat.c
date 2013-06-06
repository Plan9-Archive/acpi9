#include <u.h>
#include <libc.h>
#include <aml.h>
#include "acpi.h"

/* ACPI battery driver */

enum { BatteryPresent = (1L << 4) }; 
enum { BifUnknown = 0xffffffff }; 

int 
acpibat_match(char *id) {
	return (id && (!strcmp(id, "PNP0C0A")));
}

static void
acpibat_status(struct acpidev *dev) {
	void *dot = dev->node;
	void *m, **a, *p;

	if((m = amlwalk(dot, "_BIF")) == 0) { 
         print("bat: no _BIF\n"); 
    } 
    if(amleval(m, "", &p) < 0) { 
         print("bat: _BIF not working\n"); 
    } 
    a = amlval(p); 
    print("\tpower unit: %ulld\n", amlint(a[0])); 
    print("\tcapacity: %ulld\n", amlint(a[1])); 
    print("\tlast capacity: %ulld\n", amlint(a[2])); 
    print("\tvoltage: %ulld\n", amlint(a[4])); 
    print("\tmodel: %s\n", (char*)amlval(a[9])); 
    print("\ttype: %s\n", (char*)amlval(a[11])); 
    print("\toem: %s\n", (char*)amlval(a[12])); 

    if((m = amlwalk(dot, "_BST")) == 0) { 
    	print("bat: no _BST\n"); 
		return;
    } 
    if(amleval(m, "", &p) < 0) { 
    	print("bat: _BST not working\n"); 
    	return; 
    }        
    a = amlval(p); 
    print("\tcur.state: %ulld\n", amlint(a[0])); 
    print("\tcur.rate: %ulld\n", amlint(a[1])); 
    print("\tcur.capacity: %ulld\n", amlint(a[2])); 
    print("\tcur.voltage: %ulld\n", amlint(a[3])); 
}

int 
acpibat_attach(struct acpidev *dev) {       
	uvlong sta; 
    void *m, *dot, *p; 

	dot = dev->node;
    if((m = amlwalk(dot, "_STA")) == 0) { 
		print("bat: no _STA\n"); 
		return -1; 
    } 
    if(amleval(m, "", &p) < 0) { 
    	print("bat: _STA not working\n"); 
        return -1; 
    } 
    sta = amlint(p); 
    if((sta & BatteryPresent) != 0) { 
             print("bat:\n"); 
			 acpibat_status(dev);
    } 
	return 0;
}
