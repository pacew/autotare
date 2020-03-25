#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>




#define EIR_FLAGS                   0x01  /* flags */
#define EIR_UUID16_SOME             0x02  /* 16-bit UUID, more available */
#define EIR_UUID16_ALL              0x03  /* 16-bit UUID, all listed */
#define EIR_UUID32_SOME             0x04  /* 32-bit UUID, more available */
#define EIR_UUID32_ALL              0x05  /* 32-bit UUID, all listed */
#define EIR_UUID128_SOME            0x06  /* 128-bit UUID, more available */
#define EIR_UUID128_ALL             0x07  /* 128-bit UUID, all listed */
#define EIR_NAME_SHORT              0x08  /* shortened local name */
#define EIR_NAME_COMPLETE           0x09  /* complete local name */
#define EIR_TX_POWER                0x0A  /* transmit power level */
#define EIR_DEVICE_ID               0x10  /* device ID */

static void eir_parse_name(uint8_t *eir, size_t eir_len,
						char *buf, size_t buf_len)
{
	size_t offset;

	offset = 0;
	while (offset < eir_len) {
		uint8_t field_len = eir[0];
		size_t name_len;

		/* Check for the end of EIR */
		if (field_len == 0)
			break;

		if (offset + field_len > eir_len)
			goto failed;

		switch (eir[1]) {
		case EIR_NAME_SHORT:
		case EIR_NAME_COMPLETE:
			name_len = field_len - 1;
			if (name_len > buf_len)
				goto failed;

			memcpy(buf, &eir[2], name_len);
			return;
		}

		offset += field_len + 1;
		eir += field_len + 1;
	}

failed:
	buf[0] = 0;
}

struct dev {
	struct dev *next;
	int devnum;
	char *addr;
	char *name;
};

struct dev *devs;
int last_devnum;

struct dev *
find_dev (int devnum)
{
	struct dev *dp;

	for (dp = devs; dp; dp = dp->next) {
		if (dp->devnum == devnum)
			return (dp);
	}

	return (NULL);
}

void
add_device (char *addr, char *name)
{
	struct dev *dp;

	for (dp = devs; dp; dp = dp->next) {
		if (strcmp (dp->addr, addr) == 0
		    && strcmp (dp->name, name) == 0) {
			break;
		}
	}
	
	if (dp == NULL) {
		dp = calloc (1, sizeof *dp);
		dp->devnum = ++last_devnum;
		dp->addr = strdup (addr);
		dp->name = strdup (name);
		dp->next = devs;
		devs = dp;

		printf ("%-2d %-30.30s %-20s\n",
			dp->devnum, dp->name, dp->addr);
	}
}


void
process_input (int dd)
{
	unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr;
	int len;
	
	if ((len = read(dd, buf, sizeof(buf))) < 0)
		return;
	
	evt_le_meta_event *meta;
	le_advertising_info *info;
	char addr[18];

	ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
	len -= (1 + HCI_EVENT_HDR_SIZE);

	meta = (void *) ptr;

	if (meta->subevent != 0x02)
		return;

	info = (le_advertising_info *) (meta->data + 1);

	char name[30];

	memset(name, 0, sizeof(name));

	ba2str(&info->bdaddr, addr);
	eir_parse_name(info->data, info->length,
		       name, sizeof(name) - 1);
	
	add_device (addr, name);
}

char *outname;

int
process_stdin (void)
{
	int devnum;
	struct dev *dp;
	FILE *outf;
	
	if (scanf ("%d", &devnum) != 1)
		goto bad;

	if ((dp = find_dev (devnum)) == NULL)
		goto bad;

	remove (outname);
	if ((outf = fopen (outname, "w")) == NULL) {
		fprintf (stderr, "can't create %s\n", outname);
		exit (1);
	}
	fprintf (outf, "%s\n", dp->addr);
	fclose (outf);
	printf ("selected %s (%s)\n", dp->name, dp->addr);
	return (0);

bad:
	printf ("invalid selection\n");
	return (-1);
}


void
usage (void)
{
	fprintf (stderr, "usage: btpair [-o file]\n");
	exit (1);
}

int
main (int argc, char **argv)
{
	int c;

	outname = ".bluetooth_addr";
	
	while ((c = getopt (argc, argv, "o:")) != EOF) {
		switch (c) {
		case 'o':
			outname = optarg;
			break;
		default:
			usage ();
		}
	}

	if (optind != argc)
		usage ();
	
	if (geteuid () != 0)
		printf ("should be root\n");

	int dev_id;
	int dd;
	uint8_t own_type = LE_PUBLIC_ADDRESS;
	uint8_t scan_type = 0x01;
	uint8_t filter_policy = 0x00;
	uint16_t interval = htobs(0x0010);
	uint16_t window = htobs(0x0010);
	uint8_t filter_dup = 0x01;

	dev_id = hci_get_route (NULL);
	if ((dd = hci_open_dev (dev_id)) < 0) {
		perror("hci_open_dev");
		exit(1);
	}

	hci_le_set_scan_enable (dd, 0, filter_dup, 10000);

	if (hci_le_set_scan_parameters (dd, scan_type, interval, window,
					own_type, filter_policy, 10000) < 0) {
		perror("hci_le_set_scan_parameters");
		exit(1);
	}

	if (hci_le_set_scan_enable(dd, 1, filter_dup, 10000) < 0) {
		perror("hci_le_set_scan_enable");
		exit(1);
	}

	struct hci_filter nf;

	hci_filter_clear(&nf);
	hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
	hci_filter_set_event(EVT_LE_META_EVENT, &nf);

	if (setsockopt(dd, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0) {
		printf("Could not set socket options\n");
		return -1;
	}

	fcntl (dd, F_SETFL, O_NONBLOCK);
	
	while (1) {
		fd_set rset;

		FD_ZERO (&rset);
		FD_SET (0, &rset);
		FD_SET (dd, &rset);

		if (select (dd + 1, &rset, NULL, NULL, NULL) < 0) {
			perror ("select");
			exit (1);
		}

		if (FD_ISSET (dd, &rset)) {
			process_input (dd);
		}

		if (FD_ISSET (0, &rset)) {
			if (process_stdin () == 0)
				break;
		}
	}

	hci_le_set_scan_enable (dd, 0, filter_dup, 10000);

	return 0;
}

