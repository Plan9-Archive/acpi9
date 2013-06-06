#include "acpi.h"

/* drivers */

/* AC */
int acpiac_attach(struct acpidev *);
int acpiac_match(char *);

/* Battery */
int acpibat_attach(struct acpidev *);
int acpibat_match(char *);

/* CPU */
int foundtss(void *, void *);

struct acpidev acpidev[] = {
	{
		"ac",
		0,	
		acpiac_match,
		acpiac_attach,
		nil,
		nil,
		nil,
		nil
	},
	{
		"bat",
		0,
		acpibat_match,
		acpibat_attach,
		nil,
		nil,
		nil,
		nil
	}
};