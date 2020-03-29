#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#define ATT_CID 4

#define ATT_OP_FIND_INFO_REQ		0x04
#define ATT_OP_FIND_INFO_RESP		0x05
#define ATT_OP_READ_BY_GROUP_REQ	0x10
#define ATT_OP_READ_BY_GROUP_RESP	0x11
#define ATT_OP_WRITE_REQ		0x12
#define ATT_OP_WRITE_RESP		0x13
#define ATT_OP_HANDLE_NOTIFY		0x1B
#define ATT_OP_WRITE_CMD		0x52

#define ATT_FIND_INFO_RESP_FMT_16BIT		0x01


#define GATT_PRIM_SVC_UUID		0x2800

#define TX_HANDLE_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define RX_HANDLE_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

#define xAUTOTARE_SERVICE_UUID "75c3c6d0-75e4-4223-a823-bdc65e738996"
#define AUTOTARE_SERVICE_UUID "0000ec00-0000-1000-8000-00805f9b34fb"

#define CLIENT_CHAR_CONFIG_UUID "00002902-0000-1000-8000-00805f9b34fb"


void write_cmd (int sock, int handle, void *buf, int len);

void do_xmit (int sock, char *msg);

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

int vflag;

void
usage (void)
{
	fprintf (stderr, "usage: bt\n");
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

int
connect_bluetooth (void)
{
	struct sockaddr_l2 src_addr;
	struct sockaddr_l2 dst_addr;
	char dst_addr_str[100];
	char *fname;
	int n;

	FILE *inf = NULL;
	int sock = -1;
	
	fname = ".bluetooth_addr";
	if ((inf = fopen (fname, "r")) == NULL) {
		fprintf (stderr, "can't open %s - maybe run ./pair?\n", fname);
		goto bad;
	}
	if (fgets (dst_addr_str, sizeof dst_addr_str, inf) == NULL) {
		fprintf (stderr, "can't read %s\n", fname);
		fclose (inf);
		goto bad;
	}
	fclose (inf);
	inf = NULL;

	n = strlen (dst_addr_str);
	while (n > 0 && isspace (dst_addr_str[n-1]))
		dst_addr_str[--n] = 0;

	if ((sock = socket (PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) < 0) {
		perror ("socket bt");
		goto bad;
	}

	memset (&src_addr, 0, sizeof src_addr);
	src_addr.l2_family = AF_BLUETOOTH;
	memcpy (&src_addr.l2_bdaddr, BDADDR_ANY, sizeof src_addr.l2_bdaddr);
	src_addr.l2_cid = htobs (ATT_CID);
	src_addr.l2_bdaddr_type = BDADDR_BREDR;
	if (bind (sock, (struct sockaddr *)&src_addr, sizeof src_addr) < 0) {
		perror ("bind");
		goto bad;
	}

	struct bt_security sec;
	memset (&sec, 0, sizeof sec);
	sec.level = BT_SECURITY_LOW;
	if (setsockopt (sock, SOL_BLUETOOTH, BT_SECURITY, 
			&sec, sizeof sec) < 0) {
		/* btio.c allows ENOPROTOOPT and does other stuff */
		perror ("setsockopt BT_SECURITY");
		goto bad;
	}

	if (fcntl (sock, F_SETFL, O_NONBLOCK) < 0) {
		perror ("fcntl NONBLOCK");
		goto bad;
	}

	memset (&dst_addr, 0, sizeof dst_addr);
	dst_addr.l2_family = AF_BLUETOOTH;
	if (str2ba(dst_addr_str, &dst_addr.l2_bdaddr) < 0) {
		fprintf (stderr, "can't parse address %s from %s\n",
			 dst_addr_str, fname);
		return (-1);
	}
	dst_addr.l2_cid = htobs (ATT_CID);
	dst_addr.l2_bdaddr_type = BDADDR_LE_RANDOM;

	printf ("%.3f connecting...\n", get_secs());

	if (connect (sock, (struct sockaddr*)&dst_addr, sizeof dst_addr) < 0) {
		if (errno != EAGAIN && errno != EINPROGRESS) {
			perror ("connect");
			goto bad;
		}
	}

	printf ("%.3f waiting\n", get_secs ());
	fd_set wset;
	FD_ZERO (&wset);
	FD_SET (sock, &wset);
	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	
	if (select (sock + 1, NULL, &wset, NULL, &tv) < 0) {
		perror ("select for connect");
		goto bad;
	}

	if (! FD_ISSET (sock, &wset)) {
		printf ("socket didn't become writable\n");
		goto bad;
	}

	printf ("%.3f connected\n", get_secs ());

	int sk_err;
	socklen_t sk_err_len = sizeof sk_err;
	if (getsockopt (sock, SOL_SOCKET, SO_ERROR,
			&sk_err, &sk_err_len) < 0) {
		perror ("getsockopt SO_ERROR");
		goto bad;
	}
	if (sk_err != 0) {
		printf ("connect error %s\n", strerror (sk_err));
		goto bad;
	}

	return (sock);

bad:
	if (sock >= 0)
		close (sock);
	if (inf)
		fclose (inf);
	return (-1);
}

struct handle {
	char *name;
	char *uuid_str;
	unsigned char uuid_bin[16];
	int handle;
};

int tx_handle;
int rx_handle;

int
uuid_to_bin (char *str, unsigned char *bin)
{
	int i;
	char *p;
	int val;
	
	p = str;
	for (i = 0; i < 16; i++) {
		if (sscanf (p, "%2x", &val) != 1) {
			memset (bin, 0, 16);
			return (-1);
		}
		bin[i] = val;
		p += 2;
		if (*p == '-')
			p++;
	}
	return (0);
}

void
print_uuid (uint8_t *uuid)
{
	int i;
	
	for (i = 0; i < 16; i++) {
		if (i == 4 || i == 6 || i == 8 || i == 10)
			printf ("-");
		printf ("%02x", uuid[i]);
	}
}

void
uuid_to_str (unsigned char bin[16], char *str)
{
	int i;
	char *p;

	p = str;
	for (i = 0; i < 16; i++) {
		if (i == 4 || i == 6 || i == 8 || i == 10)
			*p++ = '-';
		p += sprintf (p, "%02x", bin[i]);
	}
	*p = 0;
}


int
await_readable (int sock, double secs)
{
	fd_set rset;
	struct timeval tv;

	FD_ZERO (&rset);
	FD_SET (sock, &rset);
	tv.tv_sec = secs;
	tv.tv_usec = (secs - tv.tv_sec) * 1e6;

	if (select (sock + 1, &rset, NULL, NULL, &tv) < 0) {
		perror ("select");
		exit (1);
	}

	if (FD_ISSET (sock, &rset))
		return (0);

	return (-1);
}

uint8_t base_uuid[16] = {	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
	0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB
};

struct characteristic {
	struct characteristic *next;
	char uuid_str[50];
	unsigned char uuid_bin[16];
	int handle;
};

struct service {
	struct service *next;
	char uuid_str[50];
	unsigned char uuid_bin[16];
	int start_handle;
	int end_handle;
	struct characteristic *characteristics;
	struct characteristic **characteristics_tail;

};
struct service *services, **services_tail = &services;

void
remember_service (unsigned char uuid_bin[16], int start_handle, int end_handle)
{
	struct service *sp;

	sp = calloc (1, sizeof *sp);
	sp->characteristics_tail = &sp->characteristics;

	memcpy (sp->uuid_bin, uuid_bin, 16);
	uuid_to_str (uuid_bin, sp->uuid_str);
	sp->start_handle = start_handle;
	sp->end_handle = end_handle;

	*services_tail = sp;
	services_tail = &sp->next;
}

struct service *
find_service_for_handle (int handle)
{
	struct service *sp;

	for (sp = services; sp; sp = sp->next) {
		if (sp->start_handle <= handle && handle <= sp->end_handle)
			return (sp);
	}

	return (NULL);
}

void
remember_handle (int handle, unsigned char uuid_bin[16])
{
	struct service *sp;
	struct characteristic *cp;
	
	if ((sp = find_service_for_handle (handle)) == NULL) {
		fprintf (stderr, "can't find service for handle 0x%x\n", handle);
		return;
	}
	
	cp = calloc (1, sizeof *cp);
	memcpy (cp->uuid_bin, uuid_bin, 16);
	uuid_to_str (uuid_bin, cp->uuid_str);
	cp->handle = handle;

	*sp->characteristics_tail = cp;
	sp->characteristics_tail = &cp->next;
}	


void
print_services (void)
{
	struct service *sp;
	
	for (sp = services; sp; sp = sp->next) {
		printf ("0x%04x 0x%04x %s\n", 
			sp->start_handle, sp->end_handle, sp->uuid_str);
	}
}

int
lookup_handle (char *uuid_str)
{
	unsigned char uuid_bin[16];
	struct service *sp;
	struct characteristic *cp;

	if (uuid_to_bin (uuid_str, uuid_bin) < 0) {
		fprintf (stderr, "invalid uuid %s\n", uuid_str);
		return (-1);
	}

	for (sp = services; sp; sp = sp->next) {
		for (cp = sp->characteristics; cp; cp = cp->next) {
			if (memcmp (cp->uuid_bin, uuid_bin, 16) == 0)
				return (cp->handle);
		}
	}

	return (-1);
}

struct service *
find_service_by_uuid (char *uuid)
{
	struct service *sp;
	unsigned char uuid_bin[16];

	uuid_to_bin (uuid, uuid_bin);
	for (sp = services; sp; sp = sp->next) {
		if (memcmp (sp->uuid_bin, uuid_bin, 16) == 0)
			return (sp);
	}

	return (NULL);
}

int
find_handle_in_service (char *service_uuid, char *char_uuid)
{
	unsigned char service_uuid_bin[16];
	unsigned char char_uuid_bin[16];
	struct service *sp;
	struct characteristic *cp;

	uuid_to_bin (service_uuid, service_uuid_bin);
	uuid_to_bin (char_uuid, char_uuid_bin);

	if ((sp = find_service_by_uuid (service_uuid)) == NULL)
		return (-1);
	
	for (cp = sp->characteristics; cp; cp = cp->next) {
		if (memcmp (cp->uuid_bin, char_uuid_bin, 16) == 0)
			return (cp->handle);
	}

	return (-1);
}

void
enable_notifications (int sock)
{
	int handle;
	unsigned char xbuf[100];
	
	if ((handle = find_handle_in_service (AUTOTARE_SERVICE_UUID,
					      CLIENT_CHAR_CONFIG_UUID)) < 0) {
		fprintf (stderr, "can't find ccc handle\n");
		return;
	}
	
	xbuf[0] = 0x01;
	write_cmd (sock, handle, xbuf, 1);
}

void read_services (int sock);
int read_handles (int sock);

int process_read_by_group (unsigned char *pdu, int togo);

int
read_device_info (int sock)
{
	read_services (sock);
	read_handles (sock);

	if ((tx_handle = lookup_handle (TX_HANDLE_UUID)) < 0) {
		fprintf (stderr, "can't find tx handle\n");
		return (-1);
	}

	if ((rx_handle = lookup_handle (RX_HANDLE_UUID)) < 0) {
		fprintf (stderr, "can't find rx handle\n");
		return (-1);
	}
	
	printf ("tx handle 0x%x; rx 0x%x\n", tx_handle, rx_handle);
	return (0);
}

void
read_services (int sock)
{
	int start_handle, end_handle;
	unsigned char pdu_buf[100], *pdu;
	int used;
	int togo;
	int opcode;
	int last_handle;

	start_handle = 1;
	end_handle = 0xffff;

	while (start_handle < 0xffff) {
		pdu = pdu_buf;
		*pdu++ = ATT_OP_READ_BY_GROUP_REQ;
		*pdu++ = start_handle & 0xff;
		*pdu++ = start_handle >> 8;
		*pdu++ = end_handle & 0xff;
		*pdu++ = end_handle >> 8;
		*pdu++ = GATT_PRIM_SVC_UUID & 0xff;
		*pdu++ = GATT_PRIM_SVC_UUID >> 8;
		used = pdu - pdu_buf;

		if (write (sock, pdu_buf, used) < 0) {
			perror ("write");
			exit (1);
		}

		if (await_readable (sock, 1.0) < 0) {
			printf ("read timeout\n");
			break;
		}

		if ((togo = read (sock, pdu_buf, sizeof pdu_buf)) < 0) {
			perror ("read");
			exit (1);
		}
		if (togo == 0)
			break;

		pdu = pdu_buf;

		opcode = *pdu++;
		togo--;

		if (opcode != ATT_OP_READ_BY_GROUP_RESP)
			break;

		last_handle = process_read_by_group (pdu, togo);

		if (last_handle < start_handle) {
			printf ("didn't make proress\n");
			break;
		}

		start_handle = last_handle + 1;
	}

}

int
process_read_by_group (unsigned char *pdu, int togo)
{
	int elen;
	int start_handle, end_handle;
	int count;
	int elt;
	unsigned char uuid[16];
	
	if (togo == 0)
		return (-1);

	/* dec_read_by_grp_resp */

	if (vflag) {
		printf ("grp response\n");
		dump (pdu, togo);
	}

	elen = *pdu++;
	togo--;

	if (elen != 4+2 && elen != 4+16)
		return (-1);

	if (togo % elen != 0)
		return (-1);
/*
attr handle: 0x0001, end grp handle: 0x0009 uuid: 00001800-0000-1000-8000-00805f9b34fb
attr handle: 0x000a, end grp handle: 0x000d uuid: 00001801-0000-1000-8000-00805f9b34fb
attr handle: 0x000e, end grp handle: 0x0011 uuid: 75c3c6d0-75e4-4223-a823-bdc65e738996
attr handle: 0x0012, end grp handle: 0xffff uuid: 6e400001-b5a3-f393-e0a9-e50e24dcca9e
*/
	count = togo / elen;
	end_handle = -1;
	
	for (elt = 0; elt < count; elt++) {
		/* we know we have a complete record
		 * since we checked that togo was a multiple of elen
		 */ 
		start_handle = pdu[0] | (pdu[1] << 8);
		end_handle = pdu[2] | (pdu[3] << 8);
		pdu += 4;
		togo -= 4;
		
		if (elen == 4+2) {
			memcpy (uuid, base_uuid, sizeof uuid);
			uuid[2] = pdu[1];
			uuid[3] = pdu[0];
			pdu += 2;
			togo -= 2;
		} else {
			int i;
			for (i = 0; i < 16; i++)
				uuid[15 - i] = pdu[i];
		}
			
		remember_service (uuid, start_handle, end_handle);
	}

	return (end_handle);
}

int process_resp (unsigned char *pdu, int togo);

int
read_handles (int sock)
{
	int start_handle, end_handle;
	unsigned char pdu_buf[100], *pdu;
	int used;
	int togo;
	int opcode;
	int last_handle;
	
	start_handle = 1;
	end_handle = 0xffff;

	while (start_handle < end_handle) {
		pdu = pdu_buf;
		*pdu++ = ATT_OP_FIND_INFO_REQ;
		*pdu++ = start_handle;
		*pdu++ = start_handle >> 8;
		*pdu++ = end_handle;
		*pdu++ = end_handle >> 8;
		used = pdu - pdu_buf;

		if (write (sock, pdu_buf, used) < 0) {
			perror ("write");
			exit (1);
		}

		if (await_readable (sock, 1.0) < 0) {
			printf ("read timeout\n");
			break;
		}

		if ((togo = read (sock, pdu_buf, sizeof pdu_buf)) < 0) {
			perror ("read");
			exit (1);
		}
		if (togo == 0)
			break;

		pdu = pdu_buf;

		opcode = *pdu++;
		togo--;

		if (opcode != ATT_OP_FIND_INFO_RESP)
			break;

		last_handle = process_resp (pdu, togo);

		if (last_handle < start_handle) {
			printf ("didn't make proress\n");
			break;
		}

		start_handle = last_handle + 1;
	}

	return (0);
}

int
process_resp (unsigned char *pdu, int togo)
{
	uint8_t format;
	int elen;
	int count;
	int elt;
	uint8_t uuid[16];
	int handle;
	
	if (togo == 0)
		return (-1);

	format = *pdu++;
	togo--;
	
	elen = 2;
	if (format == ATT_FIND_INFO_RESP_FMT_16BIT) {
		elen += 2;
	} else {
		elen += 16;
	}

	count = togo / elen;
	handle = 0;
	
	for (elt = 0; elt < count; elt++) {
		uint8_t valbuf[100];

		if (togo < elen) {
			printf ("runt\n");
			break;
		}
		
		memcpy (valbuf, pdu, elen);

		handle = valbuf[0] | (valbuf[1] << 8);
		
		if (format == ATT_FIND_INFO_RESP_FMT_16BIT) {
			memcpy (uuid, base_uuid, sizeof uuid);
			uuid[2] = valbuf[3];
			uuid[3] = valbuf[2];
		} else {
			int i;
			for (i = 0; i < 16; i++)
				uuid[15 - i] = valbuf[2 + i];
		}

		if (0) {
			printf ("0x%x ", handle);
			print_uuid (uuid);
			printf ("\n");
		}

		remember_handle (handle, uuid);

		pdu += elen;
		togo -= elen;
	}

	return (handle);
}

void
lpf_step (double *valp, double newval, double dt, double tc)
{
        double factor;

	factor = 2 * M_PI * 1/tc * dt;
        if (factor > 1)
                factor = 1;
        *valp = *valp * (1 - factor) + newval * factor;
}
	
double rawval_smooth;

double cal1_raw = -9200;
double cal1_g = 0;

double cal2_raw = -15850;
double cal2_g = 10;

double
raw_to_grams (double raw)
{
	return ((raw - cal1_raw) / (cal2_raw - cal1_raw) * (cal2_g - cal1_g) + cal1_g);
}

void
process_weight (int rawval)
{
	static FILE *f_raw;
	static FILE *f_raw_smooth;
	static FILE *f_grams;
	static double start;
	static double last;
	double now;
	double x;
	double tc;
	double dt;

	now = get_secs ();
	dt = now - last;
	last = now;

	if (f_raw == NULL) {
		start = now;
		dt = .001;

		if ((f_raw = fopen ("raw.dat", "w")) == NULL) {
			fprintf (stderr, "can't create raw.dat\n");
			exit (1);
		}
		if ((f_raw_smooth = fopen ("smooth.dat", "w")) == NULL) {
			fprintf (stderr, "can't create smooth.dat\n");
			exit (1);
		}
		if ((f_grams = fopen ("g.dat", "w")) == NULL) {
			fprintf (stderr, "can't create g.dat\n");
			exit (1);
		}
	}

	x = now - start;
	
	tc = 5;
	lpf_step (&rawval_smooth, rawval, dt, tc);

	fprintf (f_raw, "%.3f %d\n", x, rawval);
	fprintf (f_raw_smooth, "%.3f %.6f\n", x, rawval_smooth);
	fprintf (f_grams, "%8.3f %8.1f\n", x, raw_to_grams (rawval));

	fflush (f_raw);
	fflush (f_raw_smooth);
	fflush (f_grams);
}

void
process_notify (int handle, unsigned char *rawbuf, int rawlen)
{
	int rawval;
	
	if (vflag) {
		printf ("notify from 0x%x\n", handle);
		dump (rawbuf, rawlen);
	}

	if (rawlen != 4) {
		printf ("can't parse notify data\n");
		return;
	}

	rawval = rawbuf[0] 
		| (rawbuf[1] << 8)
		| (rawbuf[2] << 16)
		| (rawbuf[3] << 24);

	process_weight (rawval);
}

void
process_input (unsigned char *rbuf, int rlen)
{
	unsigned char *ptr;
	int togo;
	int opcode;
	int handle;
	
	ptr = rbuf;
	togo = rlen;

	if (togo == 0)
		return;
	
	opcode = *ptr++;
	togo--;

	switch (opcode) {
	case ATT_OP_WRITE_RESP:
		printf ("[rcv WRITE_RESP]\n");
		break;
	case ATT_OP_HANDLE_NOTIFY:
		handle = ptr[0] | (ptr[1] << 8);
		ptr += 2;
		togo -= 2;
		process_notify (handle, ptr, togo);
		break;
	default:
		printf ("unknown opcode 0x%x\n", opcode);
		dump (rbuf, rlen);
		break;
	}
}

int
main (int argc, char **argv)
{
	int c;
	int sock;

	while ((c = getopt (argc, argv, "")) != EOF) {
		switch (c) {
		default:
			usage ();
		}
	}

	if ((sock = connect_bluetooth ()) < 0) {
		fprintf (stderr, "can't connect\n");
		exit (1);
	}
	
	read_device_info (sock);

	print_services ();
	
	do_xmit (sock, "hello");

	enable_notifications (sock);

	while (1) {
		fd_set rset;
		
		FD_ZERO (&rset);
		FD_SET (0, &rset);
		FD_SET (sock, &rset);

		if (select (sock + 1, &rset, NULL, NULL, NULL) < 0) {
			perror ("select");
			exit (1);
		}

		if (FD_ISSET (0, &rset)) {
			char xbuf[100];
			int n;
			n = read (0, xbuf, sizeof xbuf - 1);
			if (n > 0) {
				xbuf[n] = 0;
				do_xmit (sock, xbuf);
			}
		}

		if (FD_ISSET (sock, &rset)) {
			unsigned char rbuf[100];
			int rlen;
	
			rlen = read (sock, rbuf, sizeof rbuf);
			process_input (rbuf, rlen);
		}
	}

	return (0);
}

				
void
do_xmit (int sock, char *msg)
{
	uint8_t pdu_base[100], *pdu;
	int pdu_len;

	pdu = pdu_base;
	*pdu++ = ATT_OP_WRITE_REQ;
	*pdu++ = tx_handle;
	*pdu++ = tx_handle >> 8;
	memcpy (pdu, msg, strlen(msg));
	pdu += strlen(msg);
	pdu_len = pdu - pdu_base;
	
	printf ("%.3f writing\n", get_secs ());
	if (write (sock, pdu_base, pdu_len) < 0) {
		perror ("write");
		exit (1);
	}
}

void
write_cmd (int sock, int handle, void *buf, int len)
{
	uint8_t pdu_base[100], *pdu;
	int pdu_len;

	pdu = pdu_base;
	*pdu++ = ATT_OP_WRITE_CMD;
	*pdu++ = handle;
	*pdu++ = handle >> 8;
	memcpy (pdu, buf, len);
	pdu += len;
	pdu_len = pdu - pdu_base;
	
	printf ("write_cmd 0x%x\n", handle);
	dump (pdu_base, pdu_len);

	if (write (sock, pdu_base, pdu_len) < 0) {
		perror ("write");
		exit (1);
	}
}


