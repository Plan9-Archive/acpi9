#include "acpi.h"

/* drivers */

/* Embedded Controller */
void acpiec_init(void*, void*);
void ecread(AcpiIo*, uvlong, uvlong, uchar*);
void ecwrite(AcpiIo*,uvlong, uvlong, uvlong);

/* AC */
int acpiac_attach(struct acpidev *);
int acpiac_match(char *);

/* Battery */
int acpibat_attach(struct acpidev *);
int acpibat_match(char *);

/* CPU */
int acpicpu_match(char *);
int acpicpu_attach(struct acpidev *);

/* Thinkpad  */
int acpitp_match(char *);
int acpitp_attach(struct acpidev *);

struct acpidev acpidev[] = {
	{
		"ac",
		0,	
		acpiac_match,
		acpiac_attach,
	},
	{
		"bat",
		0,
		acpibat_match,
		acpibat_attach,
	},
	{
		"cpu",
		0,
		acpicpu_match,
		acpicpu_attach,
	},
	{
		"thinkpad",
		0,
		acpitp_match,
		acpitp_attach,
	}
};