# This Makefile assumes the following command line utilities are in its PATH:
#		openssl
#		grep
#		sed
#		xargs
#		base64
#		echo
#		tr
#		xxd
#		wc
#
#
# There are many ways to skin this cat. This is the one I came up with. If you know of a better way, 
# (or if you know of a native openssl command line method for turning the private key into C code),
# let me know.


KEY_BITS = 2048

CC = /usr/bin/gcc
CFLAGS = -std=gnu99 -Wall -Wextra -pedantic -Os
LIBS = -lssl

OBJS = revsh_io.o string_to_vector.o broker.o

KEYS_DIR = ./keys


all: revsh

revsh: revsh.c revsh.h remote_io_helper.h common.h $(OBJS)
	if [ ! -e $(KEYS_DIR) ]; then \
		mkdir $(KEYS_DIR) ; \
	fi
	if [ ! -e $(KEYS_DIR)/dh_params_$(KEY_BITS).c ]; then \
		openssl dhparam -C $(KEY_BITS) -noout >$(KEYS_DIR)/dh_params_$(KEY_BITS).c ; \
	fi
	if [ ! -e $(KEYS_DIR)/listener_key.pem ]; then \
		(openssl req -batch -newkey rsa:$(KEY_BITS) -nodes -x509 -days 2147483647 -keyout $(KEYS_DIR)/listener_key.pem -out $(KEYS_DIR)/listener_cert.pem) && \
		(echo -n 'char *listener_fingerprint_str = "' >$(KEYS_DIR)/listener_fingerprint.c) && \
		(openssl x509 -in $(KEYS_DIR)/listener_cert.pem -fingerprint -sha1 -noout | \
			sed 's/.*=//' | \
			sed 's/://g' | \
			tr '[:upper:]' '[:lower:]' | \
			sed 's/,\s\+/,/g' | \
			sed 's/{ /{\n/' | \
			sed 's/}/\n}/' | \
			sed 's/\(\(0x..,\)\{16\}\)/\1\n/g' | \
			xargs echo -n >>$(KEYS_DIR)/listener_fingerprint.c) && \
		(echo '";' >>$(KEYS_DIR)/listener_fingerprint.c) ; \
	fi
	if [ ! -e $(KEYS_DIR)/connector_key.pem ]; then \
		(openssl req -batch -newkey rsa:$(KEY_BITS) -nodes -x509 -days 2147483647 -keyout $(KEYS_DIR)/connector_key.pem -out $(KEYS_DIR)/connector_cert.pem) && \
		(openssl x509 -in $(KEYS_DIR)/connector_cert.pem -C -noout | \
			sed 's/XXX_/connector_/g' | \
			xargs | \
			sed 's/.*; \(unsigned char connector_certificate\)/\1/'  >$(KEYS_DIR)/connector_cert.c) && \
		(echo -n 'unsigned char connector_private_key[' >$(KEYS_DIR)/connector_key.c) && \
		(cat $(KEYS_DIR)/connector_key.pem | \
			grep -v '^-----BEGIN RSA PRIVATE KEY-----$$' | \
			grep -v '^-----END RSA PRIVATE KEY-----$$' | \
			base64 -d | \
			xxd -p | \
			xargs echo -n | \
			sed 's/\s//g' | \
			sed 's/\(..\)/\1\n/g' | \
			wc -l | \
			xargs echo -n >>$(KEYS_DIR)/connector_key.c) && \
		(echo ']={') >>$(KEYS_DIR)/connector_key.c && \
		(cat $(KEYS_DIR)/connector_key.pem | \
			grep -v '^-----BEGIN RSA PRIVATE KEY-----$$' | \
			grep -v '^-----END RSA PRIVATE KEY-----$$' | \
			base64 -d | \
			xxd -p | \
			xargs echo -n | \
			sed 's/\s//g' | \
			tr '[:lower:]' '[:upper:]' | \
			sed 's/\(.\{32\}\)/\1\n/g' | \
			sed 's/\(..\)/0x\1,/g' >>$(KEYS_DIR)/connector_key.c) && \
		(echo '\n};' >>$(KEYS_DIR)/connector_key.c) ; \
	fi
	$(CC) $(LIBS) $(CFLAGS) $(OBJS) -o revsh revsh.c

revsh_io: revsh_io.c revsh_io.h remote_io_helper.h common.h
	$(CC) $(LIBS) $(CFLAGS) -c -o revsh_io.o revsh_io.c

string_to_vector: string_to_vector.c string_to_vector.h common.h
	$(CC) $(CFLAGS) -c -o string_to_vector.o string_to_vector.c

broker: broker.c broker.h common.h
	$(CC) $(CFLAGS) -c -o broker.o broker.c

install:
	if [ ! -e $(HOME)/.revsh ]; then \
		mkdir $(HOME)/.revsh ; \
	fi
	if [ -e $(HOME)/.revsh/$(KEYS_DIR) ]; then \
		echo "\nERROR: $(HOME)/.revsh/$(KEYS_DIR) already exists! Move it safely out of the way then try again, please." ; \
	else \
		cp -r $(KEYS_DIR) $(HOME)/.revsh ; \
		cp revsh $(HOME)/.revsh ; \
	fi

dirty:
	rm revsh $(KEYS_DIR)/connector* $(KEYS_DIR)/listener* $(OBJS)

clean:
	rm revsh $(KEYS_DIR)/* $(OBJS)
	rmdir $(KEYS_DIR)
