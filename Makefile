CFLAGS = -g -Wall

all: bt pair desc reset1200

reset1200: reset1200.o
	$(CC) $(CFLAGS) -o reset1200 reset1200.o

bt: bt.o
	$(CC) $(CFLAGS) -o bt bt.o -lbluetooth

desc: desc.o
	$(CC) $(CFLAGS) -o desc desc.o -lbluetooth

pair: pair.o
	$(CC) $(CFLAGS) -o pair pair.o -lbluetooth

upload:
	platformio run --target upload
