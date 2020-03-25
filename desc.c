#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

int vflag;

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

#define ATT_CID 4

#define ATT_OP_FIND_INFO_REQ		0x04
#define ATT_OP_FIND_INFO_RESP		0x05
#define ATT_OP_WRITE_REQ		0x12

#define ATT_FIND_INFO_RESP_FMT_16BIT		0x01
#define ATT_FIND_INFO_RESP_FMT_128BIT		0x02

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
connect_bluetooth (bdaddr_t *dest_addr)
{
	struct sockaddr_l2 src_addr;
	struct sockaddr_l2 dst_addr;

	int sock = -1;
	
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
	dst_addr.l2_bdaddr = *dest_addr;
	dst_addr.l2_cid = htobs (ATT_CID);
	dst_addr.l2_bdaddr_type = BDADDR_LE_RANDOM;

	if (vflag)
		printf ("%.3f connecting...\n", get_secs());

	if (connect (sock, (struct sockaddr*)&dst_addr, sizeof dst_addr) < 0) {
		if (errno != EAGAIN && errno != EINPROGRESS) {
			perror ("connect");
			goto bad;
		}
	}

	if (vflag)
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

	if (vflag)
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
	return (-1);
}

void
await_readable (int sock)
{
	fd_set rset;

	while (1) {
		FD_ZERO (&rset);
		FD_SET (sock, &rset);
		if (select (sock + 1, &rset, NULL, NULL, NULL) < 0) {
			perror ("select");
			exit (1);
		}
		if (FD_ISSET (sock, &rset))
			break;
	}
}

uint8_t base_uuid[16] = {	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
	0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB
};

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

		printf ("0x%x ", handle);
		print_uuid (uuid);
		printf ("\n");

		pdu += elen;
		togo -= elen;
	}

	return (handle);
}
      


int
main (int argc, char **argv)
{
	int c;
	int sock;
	char *fname;
	FILE *inf;
	char addrstr[100];
	bdaddr_t bdaddr;
	int n;
	int start_handle, end_handle, last_handle;
	int togo;
	unsigned char pdu_buf[100], *pdu;
	int used;
	int opcode;
	

	while ((c = getopt (argc, argv, "")) != EOF) {
		switch (c) {
		default:
			usage ();
		}
	}

	fname = ".bluetooth_addr";
	if ((inf = fopen (fname, "r")) == NULL) {
		fprintf (stderr, "can't open %s - maybe run ./pair?\n", fname);
		exit (1);
	}
	if (fgets (addrstr, sizeof addrstr, inf) == NULL) {
		fprintf (stderr, "can't read %s\n", fname);
		exit (1);
	}
	n = strlen (addrstr);
	while (n > 0 && isspace (addrstr[n-1]))
		addrstr[--n] = 0;
	
	if (str2ba(addrstr, &bdaddr) < 0) {
		fprintf (stderr, "can't parse address %s from %s\n",
			 addrstr, fname);
		exit (1);
	}

	if ((sock = connect_bluetooth (&bdaddr)) < 0)
		exit (1);
	
	if (fcntl (sock, F_SETFL, 0) < 0) {
		perror ("fcntl turn off NONBLOCK");
		exit (1);
	}

	/* in bluez/attrib/att.c : enc_find_info_req */
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

				
