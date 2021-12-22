
.PHONY := all
all: samplemqtt

clean:
	rm -f samplemqtt

run: samplemqtt
	./samplemqtt

%: %.c
	$(CC) $(CFLAGS) -o $@ $<
