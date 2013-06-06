#include <u.h>
#include <libc.h>
#include <aml.h>

struct acpicpu_tss {
	uint	tss_core_perc;
	uint	tss_power;
	uint	tss_trans_latency;
	uint	tss_ctrl;
	uint	tss_status;
};

int
foundtss(void *dot, void *)
{
	//void *m;
	void *p, **a, **pkg;
	int i;
	struct acpicpu_tss tss;
/*
	struct Package {
		int	n;
		void	*a[];
	} *pkg;
*/

/*
	print("found cpu\n");
	if((m = amlwalk(dot, "_TSS")) == 0) {
		print("cpu: no _TSS\n");
		return 1;
	}
*/
	//amldebug = 1;
	print("found tss\n");
	if(amleval(dot, "", &p) < 0) {
		print("ac: _TSS not working\n");
		return 1;
	}
	amldebug = 0;
	print("len(p) = %d\n", amllen(p));
	pkg = amlval(p);
	for(i = 0; i < amllen(p); i++) {
		a = amlval(&pkg[i]);
		tss.tss_core_perc = amlint(a[0]);
		print("core perc: %d%%\n", tss.tss_core_perc);
		//tss.tss_power 
	}
	return 1;
}