#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

enum dev_state {
	S_UNUSED,
	S_CONNECTING,
	S_CONNECTED,
	S_ERROR,
};

struct dev {
	struct dev *next;
	enum dev_state state;
	bdaddr_t bdaddr;
	char addrstr[100];
	int sock;
};

struct dev *devs;

struct dev *
find_dev (bdaddr_t *addrp)
{
	struct dev *dp;
	for (dp = devs; dp; dp = dp->next) {
		if (memcmp (&dp->bdaddr, addrp, sizeof dp->bdaddr) == 0)
			return (dp);
	}
	return (NULL);
}

struct dev *
create_dev (bdaddr_t *addrp)
{
	struct dev *dp;
	if ((dp = calloc (1, sizeof *dp)) == NULL) {
		fprintf (stderr, "out of memory\n");
		exit (1);
	}

	dp->bdaddr = *addrp;
	ba2str (addrp, dp->addrstr);

	dp->next = devs;
	devs = dp;

	return (dp);
}


int vflag;
int scan_sock;
int query_sock;

#define ATT_CID 4

#define ATT_OP_WRITE_REQ		0x12

enum {
	ATT_OP_TYPE_REQ = 0
};

void
usage (void)
{
	fprintf (stderr, "usage: pair\n");
	exit (1);
}

double
get_secs (void)
{
	struct timeval tv;
	static double start;
	double now;

	gettimeofday (&tv, NULL);

	now = tv.tv_sec + tv.tv_usec / 1e6;
	if (start == 0)
		start = now;
	return (now - start);
}

void
connect_dev (struct dev *dp)
{
	struct sockaddr_l2 src_addr;
	struct sockaddr_l2 dst_addr;

	if ((dp->sock = socket (PF_BLUETOOTH, SOCK_SEQPACKET, 
				  BTPROTO_L2CAP)) < 0) {
		perror ("socket bt");
		goto bad;
	}

	memset (&src_addr, 0, sizeof src_addr);
	src_addr.l2_family = AF_BLUETOOTH;
	memcpy (&src_addr.l2_bdaddr, BDADDR_ANY, sizeof src_addr.l2_bdaddr);
	
	src_addr.l2_cid = htobs (ATT_CID);
	src_addr.l2_bdaddr_type = BDADDR_BREDR;

	if (bind (dp->sock, (struct sockaddr *)&src_addr,sizeof src_addr)<0){
		perror ("bind");
		goto bad;
	}

	struct bt_security sec;
	memset (&sec, 0, sizeof sec);
	sec.level = BT_SECURITY_LOW;
	if (setsockopt (dp->sock, SOL_BLUETOOTH, BT_SECURITY, 
			&sec, sizeof sec) < 0) {
		/* btio.c allows ENOPROTOOPT and does other stuff */
		perror ("setsockopt BT_SECURITY");
		goto bad;
	}

	if (fcntl (dp->sock, F_SETFL, O_NONBLOCK) < 0) {
		perror ("fcntl NONBLOCK");
		goto bad;
	}

	memset (&dst_addr, 0, sizeof dst_addr);
	dst_addr.l2_family = AF_BLUETOOTH;
	dst_addr.l2_bdaddr = dp->bdaddr;
	dst_addr.l2_cid = htobs (ATT_CID);
	dst_addr.l2_bdaddr_type = BDADDR_LE_RANDOM;

	printf ("%.3f connecting to %s\n", get_secs(), dp->addrstr);

	if (connect (dp->sock, 
		     (struct sockaddr*)&dst_addr, 
		     sizeof dst_addr) < 0) {
		if (errno != EAGAIN && errno != EINPROGRESS) {
			perror ("connect");
			goto bad;
		}
	}

	dp->state = S_CONNECTING;
	return;

bad:
	dp->state = S_ERROR;
	if (dp->sock > 0)
		close (dp->sock);
	dp->sock = 0;
}

void
send_to_uart (struct dev *dp, char *buf, int len)
{
	uint8_t pdu[100];
	int idx;
	int handle = 0x13;
	int n;
	
	idx = 0;
	pdu[idx++] = ATT_OP_WRITE_REQ;
	pdu[idx++] = handle;
	pdu[idx++] = handle >> 8;
	memcpy (pdu + idx, buf, len);
	idx += len;

	if ((n = write (dp->sock, pdu, idx)) < 0) {
		printf ("sock %d\n", dp->sock);
		perror ("write uart");
	} else if (n != idx) {
		printf ("only wrote %d of %d\n", n, idx);
	}
}

void
handle_connected (struct dev *dp)
{
	printf ("%s connected\n", dp->addrstr);
	dp->state = S_CONNECTED;
	
	int sk_err;
	socklen_t sk_err_len = sizeof sk_err;
	if (getsockopt (scan_sock, SOL_SOCKET, SO_ERROR,
			&sk_err, &sk_err_len) < 0) {
		sk_err = errno;
	}
	if (sk_err != 0) {
		printf ("%s: connect error %s\n", 
			dp->addrstr, strerror (sk_err));
	}

	send_to_uart (dp, "f\n", 2);
}

int pflag;

/* from hcitool.c */
/* Extended Inquiry Response */
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


void
dump (void *buf, int n)
{
	int i;
	int j;
	int c;

	for (i = 0; i < n; i += 16) {
		printf ("%04x: ", i);
		for (j = 0; j < 16; j++) {
			if (i+j < n)
				printf ("%02x ", ((unsigned char *)buf)[i+j]);
			else
				printf ("   ");
		}
		printf ("  ");
		for (j = 0; j < 16; j++) {
			c = ((unsigned char *)buf)[i+j] & 0x7f;
			if (i+j >= n)
				putchar (' ');
			else if (c < ' ' || c == 0x7f)
				putchar ('.');
			else
				putchar (c);
		}
		printf ("\n");

	}
}

void
uuid_to_str (void *arg, char *buf)
{
	uint8_t *p = arg;
	int i;
	char *outp;

	outp = buf;
	for (i = 0; i < 16; i++) {
		if (i == 4 || i == 6 || i == 8 || i == 10)
			*outp++ = '-';
		outp += sprintf (outp, "%02x", p[i]);
	}
	*outp = 0;
}

void
process_info (le_advertising_info *info)
{
	char addrstr[100];
	uint8_t *eir;
	int eir_togo;
	int eir_op;
	int eir_flags;
	char name[100];
	int name_len;
	uint8_t uuid[16];
	char uuid_str[100];
	uint8_t uuid16[2];
	char uuid16_str[100];
	struct dev *dp;

	name[0] = 0;
	eir_flags = 0;
	memset (uuid, 0, sizeof uuid);
	memset (uuid16, 0, sizeof uuid16);

	eir = info->data;
	eir_togo = info->length;
	
	if (vflag) {
		printf ("eir\n");
		dump (eir, eir_togo);
	}

	while (eir_togo > 0) {
		uint8_t field_len;

		field_len = *eir++;
		eir_togo--;
		
		if (field_len < 1 || field_len > eir_togo)
			break;
		
		eir_op = *eir++;
		eir_togo--;
		field_len--;
		
		switch (eir_op) {
		case EIR_FLAGS:
			if (eir_togo < 1)
				goto done;
			eir_flags = *eir++;
			eir_togo--;
			break;
	
		case EIR_NAME_SHORT:
		case EIR_NAME_COMPLETE:
			if (eir_togo < field_len)
				goto done;
			name_len = field_len;
			if (name_len > sizeof name - 1)
				name_len = sizeof name - 1;
			memcpy (name, eir, name_len);
			name[name_len] = 0;
			eir += field_len;
			eir_togo -= field_len;
			break;
		case EIR_UUID16_SOME:
		case EIR_UUID16_ALL:
			if (field_len < 2 || eir_togo < field_len)
				goto done;

			memcpy (uuid16, eir, sizeof uuid16);
			eir += field_len;
			eir_togo -= field_len;
			break;
		case EIR_UUID128_SOME:
		case EIR_UUID128_ALL:
			if (field_len != 16 || eir_togo < field_len)
				goto done;
			memcpy (uuid, eir, sizeof uuid);
			eir += field_len;
			eir_togo -= field_len;
			break;
		default:
			if (vflag) {
				printf ("unknown eir op 0x%x %d\n", 
					eir_op, field_len);
				dump (eir, field_len);
			}
			break;
		}

	}
done:
	ba2str (&info->bdaddr, addrstr);
	uuid_to_str (uuid, uuid_str);
	sprintf (uuid16_str, "%02x%02x", uuid16[0], uuid16[1]);
	
	if ((dp = find_dev (&info->bdaddr)) == NULL) {
		printf ("%-20s %-20s %-4s 0x%02x %s\n", 
			addrstr, uuid_str, uuid16_str, eir_flags, name);
		dp = create_dev (&info->bdaddr);
		if (strcmp (addrstr, "DC:99:65:B8:F1:F6") == 0)
			connect_dev (dp);
	}
}


void
process_meta (void *pdata, int togo)
{
	evt_le_meta_event *meta = pdata;
	le_advertising_info *info;

	switch (meta->subevent) {
	case EVT_LE_ADVERTISING_REPORT:
		/* why +1? */
		info = (void *)meta->data + 1;
		process_info (info);
		break;
	default:
		printf ("unknown subevent 0x%x\n",
			meta->subevent);
		dump (pdata, togo);
		break;
	}
}

void
process_scan_result (void)
{
	int len;
	unsigned char buf[HCI_MAX_EVENT_SIZE];
	
	if ((len = read(scan_sock, buf, sizeof buf)) < 0) {
		perror ("read");
		exit (1);
	}

	if (vflag)
		dump (buf, len);

	uint8_t *pdata = buf;
	int togo = len;

	if (togo == 0) {
		printf ("runt\n");
		return;
	}

	int hci_packet_type = *pdata++;
	togo--;

	if (hci_packet_type != HCI_EVENT_PKT) {
		printf ("unknown hci_packet_type %d\n",
			hci_packet_type);
		return;
	}

	if (togo < HCI_EVENT_HDR_SIZE) {
		printf ("hci event hdr runt\n");
		return;
	}

	hci_event_hdr *evhdr = (void *)pdata;
	pdata += HCI_EVENT_HDR_SIZE;
	togo -= HCI_EVENT_HDR_SIZE;

	if (evhdr->plen > togo) {
		printf ("evhdr overflow\n");
		return;
	}
		
	switch (evhdr->evt) {
	case EVT_LE_META_EVENT:
		process_meta (pdata, togo);
		break;
	default:
		printf ("unknown event 0x%x\n", evhdr->evt);
		break;
	}
}

int
main (int argc, char **argv)
{
	int c;
	int dev_id;
	struct dev *dp;

	while ((c = getopt (argc, argv, "")) != EOF) {
		switch (c) {
		default:
			usage ();
		}
	}

	if ((dev_id = hci_get_route (NULL)) < 0) {
		printf ("hci_get_route failed\n");
		exit (1);
	}
	
	if ((scan_sock = hci_open_dev (dev_id)) < 0) {
		perror("hci_open_dev");
		exit(1);
	}

	uint8_t filter_dup = 1;

	/* make sure scanning is disabled */
	hci_le_set_scan_enable(scan_sock, 0, filter_dup, 10000);

	uint8_t scan_type = 0; /* 1=active, 0=passive */
	uint16_t interval = htobs(0x0010);
	uint16_t window = htobs (0x0010);
	uint8_t own_type = LE_PUBLIC_ADDRESS;
	uint8_t filter_policy = 0;
	if (hci_le_set_scan_parameters(scan_sock, scan_type, interval, window,
				       own_type, filter_policy, 10000) < 0) {
		perror("hci_le_set_scan_parameters");
		exit(1);
	}

	/* enable scanning */
	if (hci_le_set_scan_enable(scan_sock, 1, filter_dup, 10000) < 0) {
		perror("Enable scan failed");
		exit(1);
	}

	struct hci_filter nf;

	hci_filter_clear(&nf);
	hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
	hci_filter_set_event(EVT_LE_META_EVENT, &nf);
	if (setsockopt(scan_sock, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0) {
		perror ("HCI_FILTER2\n");
		exit (1);
	}

	while (1) {
		fd_set rset, wset;
		int maxfd;

		FD_ZERO (&rset);
		FD_ZERO (&wset);
		FD_SET (0, &rset);
		maxfd = 0;

		FD_SET (scan_sock, &rset);
		if (scan_sock > maxfd)
			maxfd = scan_sock;
		
		for (dp = devs; dp; dp = dp->next) {
			if (dp->state == S_CONNECTING) {
				FD_SET (dp->sock, &wset);
				if (dp->sock > maxfd)
					maxfd = dp->sock;
			}
		}

		if (select (maxfd + 1, &rset, &wset, NULL, NULL) < 0) {
			perror ("select");
			exit (1);
		}

		if (FD_ISSET (scan_sock, &rset)) {
			process_scan_result ();
		}
		for (dp = devs; dp; dp = dp->next) {
			if (dp->state == S_CONNECTING 
			    && FD_ISSET (dp->sock, &wset)) {
				handle_connected (dp);
			}
		}
	}

	if (hci_le_set_scan_enable(scan_sock, 0x00, filter_dup, 10000) < 0) {
		perror("Disable scan failed");
		exit(1);
	}


	return (0);
}

				
