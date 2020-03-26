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

#define ATT_CID 4

#define ATT_OP_FIND_INFO_REQ		0x04
#define ATT_OP_FIND_INFO_RESP		0x05
#define ATT_OP_WRITE_REQ		0x12

#define ATT_FIND_INFO_RESP_FMT_16BIT		0x01

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

	if (fcntl (sock, F_SETFL, 0) < 0) {
		perror ("fcntl turn off NONBLOCK");
		exit (1);
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

struct handle handles[] = {
	{ "tx", "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" },
	{ "rx", "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" },
};

int tx_handle;
int rx_handle;

int
parse_uuid (char *str, unsigned char *bin)
{
	int i;
	char *p;
	int val;
	
	p = str;
	for (i = 0; i < 16; i++) {
		if (sscanf (p, "%2x", &val) != 1)
			return (-1);
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

uint8_t base_uuid[16] = {	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
	0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB
};

int
process_resp (unsigned char *pdu, int togo)
{
	uint8_t format;
	int elen;
	int count;
	int elt;
	uint8_t uuid[16];
	int handle;
	int h;
	struct handle *hp;
	
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

		for (h = 0; h < sizeof handles / sizeof handles[0]; h++) {
			hp = &handles[h];
			if (memcmp (hp->uuid_bin, uuid, 16) == 0)
				hp->handle = handle;
		}

		pdu += elen;
		togo -= elen;
	}

	return (handle);
}

int
read_handles (int sock)
{
	int start_handle, end_handle;
	struct handle *hp;
	int h;
	unsigned char pdu_buf[100], *pdu;
	int used;
	int togo;
	int opcode;
	int last_handle;
	
	for (h = 0; h < sizeof handles / sizeof handles[0]; h++) {
		hp = &handles[h];
		if (parse_uuid (hp->uuid_str, hp->uuid_bin) < 0) {
			fprintf (stderr, "invalid uuid %s\n", 
				 hp->uuid_str);
			return (-1);
		}
	}

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

	for (h = 0; h < sizeof handles / sizeof handles[0]; h++) {
		hp = &handles[h];
		if (hp->handle == 0) {
			fprintf (stderr, "can't find handle for %s\n",
				 hp->uuid_str);
			return (-1);
		}
		if (strcmp (hp->name, "tx") == 0)
			tx_handle = hp->handle;
		else if (strcmp (hp->name, "rx") == 0)
			rx_handle = hp->handle;
	}

	return (0);
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
	
	if (read_handles (sock) < 0) {
		fprintf (stderr, "can't find handles\n");
		exit (1);
	}

	uint8_t pdu[100];
	int pdu_len;
	int handle = 0x13;
	char msg[2] = "f\n";
	
	pdu[0] = ATT_OP_WRITE_REQ;
	pdu[1] = handle;
	pdu[2] = handle >> 8;
	memcpy (pdu + 3, msg, sizeof msg);
	pdu_len = 3 + sizeof msg;
	
	printf ("%.3f writing\n", get_secs ());
	if (write (sock, pdu, pdu_len) < 0) {
		perror ("write");
		exit (1);
	}

	while (1) {
		char rbuf[100];
		int rlen;
		printf ("%.3f reading\n", get_secs ());
	
		rlen = read (sock, rbuf, sizeof rbuf);
		printf ("%.3f got %d %x\n", get_secs (), rlen, rbuf[0]);
	}

	return (0);
}

				
