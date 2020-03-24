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

#define ATT_CID 4

#define ATT_OP_WRITE_REQ		0x12

enum {
	ATT_OP_TYPE_REQ = 0
};

void
usage (void)
{
	fprintf (stderr, "usage: bt\n");
	exit (1);
}

double
get_timestamp (void)
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
	str2ba("DC:99:65:B8:F1:F6", &dst_addr.l2_bdaddr);
	dst_addr.l2_cid = htobs (ATT_CID);
	dst_addr.l2_bdaddr_type = BDADDR_LE_RANDOM;

	printf ("%.3f connecting...\n", get_timestamp());

	if (connect (sock, (struct sockaddr*)&dst_addr, sizeof dst_addr) < 0) {
		if (errno != EAGAIN && errno != EINPROGRESS) {
			perror ("connect");
			goto bad;
		}
	}

	printf ("%.3f waiting\n", get_timestamp ());
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

	printf ("%.3f connected\n", get_timestamp ());

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

	if ((sock = connect_bluetooth ()) < 0)
		exit (1);
	
	if (fcntl (sock, F_SETFL, 0) < 0) {
		perror ("fcntl turn off NONBLOCK");
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
	
	printf ("%.3f writing\n", get_timestamp ());
	if (write (sock, pdu, pdu_len) < 0) {
		perror ("write");
		exit (1);
	}

	while (1) {
		char rbuf[100];
		int rlen;
		printf ("%.3f reading\n", get_timestamp ());
	
		rlen = read (sock, rbuf, sizeof rbuf);
		printf ("%.3f got %d %x\n", get_timestamp (), rlen, rbuf[0]);
	}

	return (0);
}

				
