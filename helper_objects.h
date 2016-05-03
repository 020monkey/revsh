
/* An object to assist in sending and recieving messages. */
struct message_helper {
	unsigned char data_type;
	unsigned short data_len;

	// Additional header info:
	//	* header_type: subtype defining what is going on.
	//	* header_id: the id number of the proxy connection.
	//	* header_errno: The errno for the remote request that has failed.
	unsigned short header_type;
	unsigned short header_origin;
	unsigned short header_id;
	unsigned short header_errno;

	char *data;
	unsigned short data_size;
};

/* This struct allows for tracking of proxies being requested on the command line. They may or may not become real proxies later on. */
struct proxy_request_node {
	char *request_string;
	int type;
	
	struct proxy_request_node *next;
};

/* An object acting as a collection of different configuration states. */
struct config_helper {

	unsigned char interactive;

	int bindshell;

	char *ip_addr;
	char *keys_dir;
	char *rc_file;
	char *shell;
	char *local_forward;

	int keepalive;

	unsigned int retry_start;
	unsigned int retry_stop;
	unsigned int timeout;

#ifdef OPENSSL
	int encryption;
	char *cipher_list;
#endif /* OPENSSL */

	struct proxy_request_node *proxy_request_head;

};

/* A node for a linked list of proxies. */
struct proxy_node {
	char *lhost;
	char *lport;
	char *rhost_rport;

	int type;
	int fd;

	struct proxy_node *next;
};

/* A node for a linked list of connections. Used for tracking established proxy connections. */
struct connection_node {

	unsigned short origin;
	unsigned short id;
	int fd;

	// A copy of the original rhost_rport string in the related proxy_node struct, to simplify retry requests.
	// Note, this has to be a copy, because in the remote connection state, the original proxy node does not exist.
	char *rhost_rport;

	// May be useful for debugging stats.
	// unsigned long data_count;

	struct connection_node *next;
	struct connection_node *prev;
};

/* An object for organizing I/O structures and interfaces. */
struct io_helper {

	/* Denote whether this instance is on the control node or the target node. */
	int controller;
	int child_sid;

	/* We use pointers to functions here so we can invoke the appropriate function on the backend (crypto / no crypto). */
	int (*remote_read)(struct io_helper *io, void *buf, size_t count);
	int (*remote_write)(struct io_helper *io, void *buf, size_t count);

	int local_in_fd;
	int local_out_fd;
	int remote_fd;

	struct message_helper message;
	int eof;

#ifdef OPENSSL
	BIO *connect;
	SSL_CTX *ctx;
	SSL *ssl;
	DH *dh;

	const EVP_MD *fingerprint_type;
#endif /* OPENSSL */

	struct proxy_node *proxy_head;
	struct proxy_node *proxy_tail;

	struct connection_node *connection_head;
};

