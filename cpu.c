#include <u.h>
#include <libc.h>
#include <stdio.h>
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

#define MSR_LEN 8
#define TSS_LEN 200
#define STATUS_LEN 50

int
acpicpu_match(char *) {
	return 0;
}

static void
readmsr(uchar *msr) {
	int fd;

	if((fd = open("/dev/msr", OREAD)) == -1)
		sysfatal("readmsr: %r");
	if(pread(fd, msr, MSR_LEN, PerfCtl) != MSR_LEN)
		sysfatal("readmsr: %r");
	close(fd);
}

static void
writemsr(uchar val) {
	int fd;
	uchar msr[MSR_LEN];

	if((fd = open("/dev/msr", OWRITE)) == -1)
		sysfatal("writemsr: %r");
	readmsr(msr);	
	memset(msr, 0, 2);
	msr[0] |= val;
	if(pwrite(fd, msr, MSR_LEN, PerfCtl) != MSR_LEN)
		sysfatal("writemsr2: %r");
	close(fd);
}

struct acpicpu {
	struct acpicpu_tss **tss;
	int ntss;
	void (*set_tss)(uchar);
	void (*get_tss)(uchar*);
	File *ts;
};

/* we don't use _PTC, but keep it here 
 * if sometime we want to 
 */
/*
void
getptc(void *dot) {
	void *m;
	struct acpicpu_pct pct;
	struct acpi_gas *gas;
	uchar *buf;

	if((m = amlwalk(dot, "^_PTC")) == 0) {
		print("cpu: no _PTC\n");
		return 1;
	}
	if(amleval(m, "", &p) < 0) {
		print("tss: _PTC not working\n");
		return 1;
	}
	if (amllen(p) != 2) {
		print("invalid _PTC len\n");
		return 1;
	}
	a = amlval(p);
	memcpy(&pct.ctrl, a[0], sizeof(pct.ctrl));
	memcpy(&pct.status, a[1], sizeof(pct.status));

	buf = a[0];
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
}
*/

static char *
status(struct acpidev *dev, File *f) {
	struct acpicpu *cpu;
	uchar msr[8];
	char *buf;
	int i, n;

	cpu = (struct acpicpu*)dev->data;
	if(f == cpu->ts) {
		if((buf = malloc(TSS_LEN)) == 0)
			sysfatal("%r");
		n = 0;
		n += snprint(buf, TSS_LEN, "%%\t\t power\tctl\n");
		for(i = 0; i < cpu->ntss; i++) {
			n += snprint(buf+n, TSS_LEN - n, "%3d%%\t%5d\t0x%x\n",
					cpu->tss[i]->tss_core_perc, 
					cpu->tss[i]->tss_power,
					cpu->tss[i]->tss_ctrl);
		}		
		return buf;
	}
	cpu->get_tss(msr);
	for(i = 0; i < cpu->ntss; i++) {
		if(cpu->tss[i]->tss_ctrl == msr[0])
			break;
	}
	if((buf = malloc(STATUS_LEN)) == 0)
		sysfatal("%r");
	if(i == cpu->ntss) {
		snprint(buf, STATUS_LEN, "unknown state\n");
	} else {
		snprint(buf, STATUS_LEN, "throttling: %d%%, power: %d\n",
				cpu->tss[i]->tss_core_perc, cpu->tss[i]->tss_power);
	}
	return buf;
}

static void 
control(struct acpidev *dev, char *data, u32int len, char *err)
{
	int i;
	struct acpicpu *cpu;
	uint val;

	cpu = (struct acpicpu*)dev->data;
	if (len < 3)
		goto bad;
	if(sscanf(data, "0x%x", &val) != 1)
		goto bad;
	for(i = 0; i < cpu->ntss; i++) {
		if (val == cpu->tss[i]->tss_ctrl)
			break;
	}
	if(i == cpu->ntss) {
bad:
		snprint(err, ERRMAX, "bad value");
		return;
	}
	print("whiteyz!\n");
	cpu->set_tss(val);
	err[0] = '\0';
	print("niggerz!\n");
}

int
acpicpu_attach(struct acpidev *dev)
{
	void *p, **a, **pkg, *dot;
	int i;
	struct acpicpu *cpu;

	dot = dev->node;
	if(amleval(dot, "", &p) < 0) {
		print("cpu: _TSS not working\n");
		return 0;
	}
	if((cpu = malloc(sizeof *cpu)) == 0)
		sysfatal("%r");
	pkg = amlval(p);
	cpu->ntss = amllen(p);
	if ((cpu->tss = malloc(cpu->ntss * sizeof(struct acpicpu_tss*))) == 0)
		sysfatal("%r");
	for(i = 0; i < amllen(p); i++) {
		a = amlval(pkg[i]);
		if ((cpu->tss[i] = malloc(sizeof(struct acpicpu_tss))) == 0)
			sysfatal("%r");
		cpu->tss[i]->tss_core_perc = amlint(a[0]);
		cpu->tss[i]->tss_power = amlint(a[1]);
		cpu->tss[i]->tss_trans_latency = amlint(a[2]);
		cpu->tss[i]->tss_ctrl = amlint(a[3]);
		cpu->tss[i]->tss_status = amlint(a[4]);
	}
	cpu->set_tss = writemsr;
	cpu->get_tss = readmsr;
	dev->data = cpu;
	dev->status = status;
	dev->control = control;	
	cpu->ts = createfile(dev->dir, "throttling", "sys", 0444, dev);
	return 1;
}