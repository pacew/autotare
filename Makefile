CFLAGS = -g -Wall

bt: bt.o
	$(CC) $(CFLAGS) -o bt bt.o -lbluetooth
