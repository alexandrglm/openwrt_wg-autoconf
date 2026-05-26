TARGETS = wg-get_conf_value wg-interface wg-route wg-setup wg-validator

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $

clean:
	rm -f $(TARGETS) *.o
