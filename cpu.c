#include <u.h>
#include <libc.h>
#include <aml.h>
#include "acpi.h"

struct acpicpu_tss {
	uint	tss_core_perc;
	uint	tss_power;
	uint	tss_trans_latency;
	uint	tss_ctrl;
	uint	tss_status;
};

struct acpi_gas {
	uchar space;
	uchar bitwidth;
	uchar bitoff;
	uchar size;
	uchar addr[8];
};

struct acpi_grd {
	uchar	grd_descriptor;
	uchar	grd_length[2];
};

struct acpicpu_pct {
	struct acpi_grd ctrl;
	struct acpi_grd status;
};

enum { PerfCtl = 0x199, MiscEnable = 0x1a0 };

static void
writemsr(void) {
	int fd;
	uchar msr[8];
	int i;

	if((fd = open("/dev/msr", ORDWR)) == -1)
		sysfatal("%r");
	if(pread(fd, msr, sizeof(msr), MiscEnable) != 8)
		sysfatal("%r");
	for(i = 0; i < sizeof(msr); i++) {
		print("0x%x, ", msr[i]);
	}
	if(msr[2] & 0x1) {
		print("Speedstep enabled!\n");
	}
	//return;
	if(pread(fd, msr, sizeof(msr), PerfCtl) != 8)
		sysfatal("%r");
	print("Before:\n");
	for(i = 0; i < sizeof(msr); i++) {
		print("0x%x, ", msr[i]);
	}
	memset(msr, 0, 2);
	msr[0] |= 0x7;
	if(pwrite(fd, msr, sizeof(msr), PerfCtl) != 8)
		sysfatal("%r");
	print("After:\n");
	for(i = 0; i < sizeof(msr); i++) {
		print("0x%x, ", msr[i]);
	}
}

int
foundtss(void *dot, void *)
{
	void *m;
	void *p, **a, **pkg;
	uchar *buf;
	int i;
	struct acpicpu_tss tss;
	struct acpicpu_pct pct;
	struct acpi_gas *gas;
	
	//amldebug = 1;
	print("found tss\n");
	if(amleval(dot, "", &p) < 0) {
		print("tss: _TSS not working\n");
		return 1;
	}
	//amldebug = 0;
	print("len(p) = %d\n", amllen(p));
	pkg = amlval(p);
	print("got package len %d\n", amllen(p));
	for(i = 0; i < amllen(p); i++) {
		print("package len = %d\n", amllen(pkg[i]));
		a = amlval(pkg[i]);
		tss.tss_core_perc = amlint(a[0]);
		tss.tss_power = amlint(a[1]);
		tss.tss_trans_latency = amlint(a[2]);
		tss.tss_ctrl = amlint(a[3]);
		tss.tss_status = amlint(a[4]);
		print("core perc: %d%% ctrl: 0x%x status: 0x%x\n", tss.tss_core_perc, tss.tss_ctrl, tss.tss_status);
		//tss.tss_power 
	}
	if((m = amlwalk(dot, "^_PTC")) == 0) {
		print("cpu: no _PTC\n");
		return 1;
	}
	//amldebug = 1;
	if(amleval(m, "", &p) < 0) {
		print("tss: _PTC not working\n");
		return 1;
	}
	amldebug =0;
	if (amllen(p) != 2) {
		print("invalid _PTC len\n");
		return 1;
	}
	a = amlval(p);
	memcpy(&pct.ctrl, a[0], sizeof(pct.ctrl));
	memcpy(&pct.status, a[1], sizeof(pct.status));

	buf = a[0];
	for(i = 0; i < 17; i++) {
		print("%x, ", buf[i]);
	}
	gas = (struct acpi_gas *)(buf + 3);
	print("_PTC(ctrl)  : %02x %04x %02x %02x %02x %02x %016ullx\n",
	    pct.ctrl.grd_descriptor,
	    get16(pct.ctrl.grd_length),
	    gas->space,
	    gas->bitwidth,
	    gas->bitoff,
	    gas->size,
	    get64(buf + 7));
	print("space = %d\n", gas->space);
	print("0x%x\n", 0x410);

	buf = a[1];
	for(i = 0; i < 17; i++) {
		print("%x, ", buf[i]);
	}
	gas = (struct acpi_gas *)(buf + 3);
	print("_PTC(status)  : %02x %04x %02x %02x %02x %02x %016ullx\n",
	    pct.ctrl.grd_descriptor,
	    get16(pct.ctrl.grd_length),
	    gas->space,
	    gas->bitwidth,
	    gas->bitoff,
	    gas->size,
	    get64(buf + 7));
	print("space = %d\n", gas->space);
	writemsr();
	return 1;
}