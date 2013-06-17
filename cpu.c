#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <aml.h>
#include "acpi.h"

#define ACPI_PDC_REVID		0x1
#define ACPI_PDC_SMP		0xa
#define ACPI_PDC_MSR		0x1

/* _PDC Intel capabilities flags from linux */
#define ACPI_PDC_P_FFH		0x0001
#define ACPI_PDC_C_C1_HALT	0x0002
#define ACPI_PDC_T_FFH		0x0004
#define ACPI_PDC_SMP_C1PT	0x0008
#define ACPI_PDC_SMP_C2C3	0x0010
#define ACPI_PDC_SMP_P_SWCOORD	0x0020
#define ACPI_PDC_SMP_C_SWCOORD	0x0040
#define ACPI_PDC_SMP_T_SWCOORD	0x0080
#define ACPI_PDC_C_C1_FFH	0x0100
#define ACPI_PDC_C_C2C3_FFH	0x0200

struct acpicpu_tss {
	uint	tss_core_perc;
	uint	tss_power;
	uint	tss_trans_latency;
	uint	tss_ctrl;
	uint	tss_status;
};

struct acpicpu_pss {
	uint	pss_core_freq;
	uint	pss_power;
	uint	pss_trans_latency;
	uint 	pss_bus_latency;
	uint	pss_ctrl;
	uint	pss_status;
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

struct acpicpu {
	struct acpicpu_tss **tss;
	struct acpicpu_pss **pss;
	int ntss;
	int npss;
	void (*set_tss)(uchar);
	void (*get_tss)(uchar*);
	File *throttling;
	File *powerstates;
};

enum { PerfCtl = 0x199, MiscEnable = 0x1a0 };

#define MSR_LEN 8
#define TSS_LEN 200
#define STATUS_LEN 50

int
acpicpu_match(char *) {
	/* never attach to _HID */
	return 0;
}

/* These two are for Intel, don't try it on AMD */
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
	readmsr(msr);
	if(msr[0] & val)
		print("0x%x written successfully\n", val);
	close(fd);
}

/* we don't use _PTC, but keep it here 
 * if sometime we want to.
 * Why not? It tells us to use i/o port
 * which doesn't actually work. 
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
status(struct acpidev *dev, File *file) {
	struct acpicpu *cpu;
	uchar msr[8];
	char *buf;
	int i, n, pss = 0;

	cpu = (struct acpicpu*)dev->data;
	if(file == cpu->throttling) {
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
	if(file == cpu->powerstates) {
		if((buf = malloc(TSS_LEN)) == 0)
			sysfatal("%r");
		n = 0;
		n += snprint(buf, TSS_LEN, "Mhz\t\tpower\tctl\n");
		for(i = 0; i < cpu->npss; i++) {
			n += snprint(buf+n, TSS_LEN - n, "%4d\t%5d\t0x%x\n",
					cpu->pss[i]->pss_core_freq, 
					cpu->pss[i]->pss_power,
					cpu->pss[i]->pss_ctrl);
		}		
		return buf;
	}
	cpu->get_tss(msr);
	for(i = 0; i < cpu->ntss; i++) {
		if(cpu->tss[i]->tss_ctrl == msr[0])
			break;
	}
	if(i == cpu->ntss) {
		for(i = 0; i < cpu->npss; i++) {
			if(cpu->pss[i]->pss_ctrl == msr[0])
				break;
		}
		if(i < cpu->npss)
			pss = 1;
	}
	if((buf = malloc(STATUS_LEN)) == 0)
		sysfatal("%r");
	if(pss == 0) {
		snprint(buf, STATUS_LEN, "unknown state 0x%x\n", msr[0]);
	} else {
		if(pss)
			snprint(buf, STATUS_LEN, "%dMhz, power: %d\n",
				cpu->pss[i]->pss_core_freq, cpu->pss[i]->pss_power);
		else
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
		goto notfound;
	if(sscanf(data, "0x%x", &val) != 1)
		goto notfound;
	for(i = 0; i < cpu->ntss; i++) {
		if (val == cpu->tss[i]->tss_ctrl)
			break;
	}
	if(i < cpu->ntss)
		goto found;

	for(i = 0; i < cpu->npss; i++) {
		if (val == cpu->pss[i]->pss_ctrl)
			break;
	}
	if(i == cpu->npss)
		goto notfound;
found:
	cpu->set_tss(val);
	err[0] = '\0';
	return;
notfound:
	snprint(err, ERRMAX, "bad value");
}

static int
evalpss(struct acpidev *dev) {
	void *p, *m;
	int i;
	void **a;
	void **pkg;
	struct acpicpu *cpu = dev->data;

	if((m = amlwalk(dev->node, "^_PSS")) == 0) {
		fprint(2, "no _PSS\n");
		return 0;
	}
	if(amleval(m, "", &p) < 0) {
		fprint(2, "cpu: _PSS not working\n");
		return 0;
	}
	pkg = amlval(p);
	cpu->npss = amllen(p);
	if ((cpu->pss = malloc(cpu->npss * sizeof(struct acpicpu_pss*))) == 0)
		sysfatal("%r");
	for(i = 0; i < cpu->npss; i++) {
		a = amlval(pkg[i]);
		if ((cpu->pss[i] = malloc(sizeof(struct acpicpu_pss))) == 0)
			sysfatal("%r");
		cpu->pss[i]->pss_core_freq = amlint(a[0]);
		cpu->pss[i]->pss_power = amlint(a[1]);
		cpu->pss[i]->pss_trans_latency = amlint(a[2]);
		cpu->pss[i]->pss_bus_latency = amlint(a[3]);
		cpu->pss[i]->pss_ctrl = amlint(a[4]);
		cpu->pss[i]->pss_status = amlint(a[5]);
	}
	return 1;
}

static int
evaltss(struct acpidev *dev) {
	struct acpicpu *cpu = dev->data;
	void *p, **a, **pkg, *m;
	int i;

	if((m = amlwalk(dev->node, "^_TSS")) == 0) {
		fprint(2, "cpu: no _TSS");
		return 0;
	}
	if(amleval(m, "", &p) < 0) {
		fprint(2, "cpu: _TSS not working\n");
		return 0;
	}
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
	return 1;
}

static int
evalpdc(struct acpidev *dev) {
	void *m;
	uint buf[3];
	void *b, *oscb;
	char *p;
	static uchar cpu_oscuuid[] = { 0x16, 0xA6, 0x77, 0x40, 0x0C, 0x29,
					   0xBE, 0x47, 0x9E, 0xBD, 0xD8, 0x70,
					   0x58, 0x71, 0x39, 0x53 };

	m = dev->node;
	buf[0] = ACPI_PDC_REVID;
	buf[1] = 1;
	buf[2] = ACPI_PDC_C_C1_HALT | ACPI_PDC_P_FFH | ACPI_PDC_C_C1_FFH
	    | ACPI_PDC_C_C2C3_FFH | ACPI_PDC_SMP_P_SWCOORD | ACPI_PDC_SMP_C2C3
	    | ACPI_PDC_SMP_C1PT;

	b = amlmkbuf(buf, sizeof(buf));
	if(amleval(m, "b", b, &p) < 0) {
		print("_PDC not working");
		return 0;
	}
	if((m = amlwalk(dev->node, "^_OSC")) == 0) {
		print("no _OSC");
		return 0;
	}
	oscb = amlmkbuf(cpu_oscuuid, sizeof(cpu_oscuuid));
	if(amleval(m, "biib", oscb, 1, 1, b, &p) < 0) {
		print("_OSC not working");
		return 0;
	}
	return 1;
}

int
acpicpu_attach(struct acpidev *dev)
{
	struct acpicpu *cpu;

	if(evalpdc(dev) == 0)
		return 0;
	if((cpu = calloc(1, sizeof *cpu)) == 0)
		sysfatal("%r");
	dev->data = cpu;
	if(!evalpss(dev) && !evaltss(dev)) {
		free(cpu);
		return 0;
	}
	cpu->set_tss = writemsr;
	cpu->get_tss = readmsr;
	dev->status = status;
	dev->control = control;	
	if(cpu->ntss != 0)
		cpu->throttling = createfile(dev->dir, "throttling", "sys", 0444, dev);
	if(cpu->pss != 0)
		cpu->powerstates = createfile(dev->dir, "powerstates", "sys", 0444, dev);
	return 1;
}