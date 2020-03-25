CFLAGS = -g -Wall

all: bt pair

bt: bt.o
	$(CC) $(CFLAGS) -o bt bt.o -lbluetooth

pair: pair.o
	$(CC) $(CFLAGS) -o pair pair.o -lbluetooth
