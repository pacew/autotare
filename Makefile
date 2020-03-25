CFLAGS = -g -Wall

all: bt pair simplescan pair2

bt: bt.o
	$(CC) $(CFLAGS) -o bt bt.o -lbluetooth

simplescan: simplescan.o
	$(CC) $(CFLAGS) -o simplescan simplescan.o -lbluetooth

pair: pair.o
	$(CC) $(CFLAGS) -o pair pair.o -lbluetooth

pair2: pair2.o
	$(CC) $(CFLAGS) -o pair2 pair2.o -lbluetooth
