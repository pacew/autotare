#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>

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

int
main (int argc, char **argv)
{
	int c;
	int sock;
	struct sockaddr_l2 src_addr;
	struct sockaddr_l2 dst_addr;


	while ((c = getopt (argc, argv, "")) != EOF) {
		switch (c) {
		default:
			usage ();
		}
	}

	printf ("ok\n");

	if ((sock = socket (PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) < 0) {
		perror ("socket bt");
		exit (1);
	}

	memset (&src_addr, 0, sizeof src_addr);
	src_addr.l2_family = AF_BLUETOOTH;
	memcpy (&src_addr.l2_bdaddr, BDADDR_ANY, sizeof src_addr.l2_bdaddr);
	
	src_addr.l2_cid = htobs (ATT_CID);
	src_addr.l2_bdaddr_type = BDADDR_BREDR;

	if (bind (sock, (struct sockaddr *)&src_addr, sizeof src_addr) < 0) {
		perror ("bind");
		exit (1);
	}

	struct bt_security sec;
	memset (&sec, 0, sizeof sec);
	sec.level = BT_SECURITY_LOW;
	if (setsockopt (sock, SOL_BLUETOOTH, BT_SECURITY, 
			&sec, sizeof sec) < 0) {
		/* btio.c allows ENOPROTOOPT and does other stuff */
		perror ("setsockopt BT_SECURITY");
		exit (1);
	}
	if (fcntl (sock, F_SETFL, O_NONBLOCK) < 0) {
		perror ("fcntl NONBLOCK");
		exit (1);
	}


	memset (&dst_addr, 0, sizeof dst_addr);
	dst_addr.l2_family = AF_BLUETOOTH;
	str2ba("DC:99:65:B8:F1:F6", &dst_addr.l2_bdaddr);
	dst_addr.l2_cid = htobs (ATT_CID);
	dst_addr.l2_bdaddr_type = BDADDR_LE_RANDOM;

	printf ("connecting...\n");

	if (connect (sock, (struct sockaddr*)&dst_addr, sizeof dst_addr) < 0) {
		if (errno != EAGAIN && errno != EINPROGRESS) {
			perror ("connect");
			exit (1);
		}
	}

	fd_set rset, eset;
	FD_ZERO (&rset);
	FD_SET (sock, &rset);
	FD_ZERO (&eset);
	FD_SET (sock, &eset);
	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	
	if (select (sock + 1, &rset, NULL, &eset, &tv) < 0) {
		perror ("select");
		exit (1);
	}

	if (FD_ISSET (sock, &eset)) {
		printf ("sock exception\n");
	}
	if (FD_ISSET (sock, &rset)) {
		printf ("readable\n");
	}

	int sk_err;
	socklen_t sk_err_len = sizeof sk_err;
	if (getsockopt (sock, SOL_SOCKET, SO_ERROR,
			&sk_err, &sk_err_len) < 0) {
		perror ("getsockopt SO_ERROR");
		exit (1);
	}
	printf ("sk_err %d\n", sk_err);
	
	uint8_t pdu[100];
	int pdu_len;
	int handle = 0x13;
	char msg[2] = "f\n";
	
	pdu[0] = ATT_OP_WRITE_REQ;
	pdu[1] = handle;
	pdu[2] = handle >> 8;
	memcpy (pdu + 3, msg, sizeof msg);
	pdu_len = 3 + sizeof msg;
	
	while (1) {
		int ret = write (sock, pdu, pdu_len);
		if (ret < 0 && errno != EINTR) {
			perror ("write");
			exit (1);
		}
		if (ret >= 0)
			break;
	}

	FD_ZERO (&rset);
	FD_SET (sock, &rset);
	FD_ZERO (&eset);
	FD_SET (sock, &eset);
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	
	if (select (sock + 1, &rset, NULL, &eset, &tv) < 0) {
		perror ("select");
		exit (1);
	}

	if (FD_ISSET (sock, &eset)) {
		printf ("sock exception\n");
	}
	if (FD_ISSET (sock, &rset)) {
		printf ("readable\n");

		char rbuf[100];
		int rlen;
		rlen = read (sock, rbuf, sizeof rbuf);
		printf ("got %d %x\n", rlen, rbuf[0]);

	}


/*
		str2ba(dst, &dba); dst = "DC:xx:xx..."
		ba->b[i] = strtol(str, NULL, 16);
		dest_type = BDADDR_LE_RANDOM;
		chan = bt_io_connect(connect_cb, NULL, NULL, &tmp_err,
				BT_IO_OPT_SOURCE_TYPE, BDADDR_LE_PUBLIC,
				BT_IO_OPT_DEST_BDADDR, &dba,
				BT_IO_OPT_DEST_TYPE, dest_type,
				BT_IO_OPT_INVALID);
*/


	return (0);
}

				
