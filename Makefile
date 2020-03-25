CFLAGS = -g -Wall

all: bt pair desc

bt: bt.o
	$(CC) $(CFLAGS) -o bt bt.o -lbluetooth

desc: desc.o
	$(CC) $(CFLAGS) -o desc desc.o -lbluetooth

pair: pair.o
	$(CC) $(CFLAGS) -o pair pair.o -lbluetooth
