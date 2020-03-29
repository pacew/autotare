CFLAGS = -g -Wall

all: bt pair desc

pio:
	platformio run --silent

verbose:
	platformio run -v

clean:
	platformio run -t clean

bt: bt.o
	$(CC) $(CFLAGS) -o bt bt.o -lbluetooth -lm

desc: desc.o
	$(CC) $(CFLAGS) -o desc desc.o -lbluetooth

pair: pair.o
	$(CC) $(CFLAGS) -o pair pair.o -lbluetooth

upload:
	platformio run --silent --target upload
