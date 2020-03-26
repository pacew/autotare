/* this started life as the lescan command in bluez/tools/hcitool.c */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

int vflag;

#define EIR_NAME_SHORT              0x08  /* shortened local name */
#define EIR_NAME_COMPLETE           0x09  /* complete local name */

static void 
extract_name (uint8_t *eir_base, size_t eir_len,
	      char *name, size_t name_len)
{
	uint8_t *eir, *field_ptr;
	size_t togo, field_togo;
	uint8_t field_len;
	int field_op;
	int n;
	
	name[0] = 0;

	eir = eir_base;
	togo = eir_len;

	while (togo > 0) {
		if ((field_len = *eir++) == 0)
			break;
		togo--;
		
		if (field_len > togo)
			break;

		field_ptr = eir;
		field_togo = field_len;

		field_op = *field_ptr++;
		field_togo--;

		switch (field_op) {
		case EIR_NAME_SHORT:
		case EIR_NAME_COMPLETE:
			n = field_togo;
			if (n > name_len - 1)
				n = name_len - 1;
			memcpy(name, field_ptr, n);
			name[n] = 0;
			goto done;
		}

		eir += field_len;
		togo -= field_len;
	}

done:
	return;
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
	int print_flag;

	for (dp = devs; dp; dp = dp->next) {
		if (strcmp (dp->addr, addr) == 0
		    && strcmp (dp->name, name) == 0) {
			break;
		}
	}
	
	print_flag = 0;

	if (dp == NULL) {
		dp = calloc (1, sizeof *dp);
		dp->devnum = ++last_devnum;
		dp->addr = strdup (addr);
		dp->name = strdup (name);
		dp->next = devs;
		devs = dp;

		print_flag = 1;
	}

	if (vflag && dp->name[0])
		print_flag = 1;
	
	if (print_flag) {
		printf ("%-2d %-30.30s %-20s\n",
			dp->devnum, dp->name, dp->addr);
	}

}


void
process_info (le_advertising_info *info, int togo)
{
	char name[30];
	char addr[100];

	ba2str (&info->bdaddr, addr);

	if (LE_ADVERTISING_INFO_SIZE + info->length > togo) {
		printf ("invalid le_advertising_info->length\n");
		return;
	}
		
	extract_name (info->data, info->length, name, sizeof name);

	add_device (addr, name);
}

void
process_input (int dd)
{
	unsigned char buf[HCI_MAX_EVENT_SIZE];
	int togo;
	unsigned char *pdata;
	int hci_packet_type;
	evt_le_meta_event *meta;
	le_advertising_info *info;
	
	if ((togo = read(dd, buf, sizeof(buf))) < 0)
		return;
	pdata = buf;

	if (togo == 0) {
		printf ("runt\n");
		return;
	}

	hci_packet_type = *pdata++;
	togo--;

	if (hci_packet_type == HCI_EVENT_PKT) {
		if (togo < HCI_EVENT_HDR_SIZE) {
			printf ("runt HCI_EVENT_PKT\n");
			return;
		}
		pdata += HCI_EVENT_HDR_SIZE;
		togo -= HCI_EVENT_HDR_SIZE;

		meta = (evt_le_meta_event *)pdata;

		if (togo < EVT_LE_META_EVENT_SIZE) {
			printf ("runt evt_le_meta_event\n");
			return;
		}

		pdata += EVT_LE_META_EVENT_SIZE;
		togo -= EVT_LE_META_EVENT_SIZE;

		if (meta->subevent == EVT_LE_ADVERTISING_REPORT) {
			/* don't know why we need to discard the first byte */
			pdata++;
			togo--;
			
			if (togo < LE_ADVERTISING_INFO_SIZE) {
				printf ("runt le_advertising_info\n");
				return;
			}
			info = (le_advertising_info *)pdata;
			process_info (info, togo);
		}
	}
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
	
	while ((c = getopt (argc, argv, "vo:")) != EOF) {
		switch (c) {
		case 'v':
			vflag = 1;
			break;
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

