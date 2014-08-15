
#include "revsh_io.h"

int remote_read_plaintext(struct remote_io_helper *io, void *buff, size_t count){
  return(BIO_read(io->connect, buff, count));
}

int remote_write_plaintext(struct remote_io_helper *io, void *buff, size_t count){
  return(BIO_write(io->connect, buff, count));
}

int remote_read_encrypted(struct remote_io_helper *io, void *buff, size_t count){
	
	int retval;
	fd_set fd_select;
	int ssl_error = SSL_ERROR_NONE;	

	do{

		if(ssl_error != SSL_ERROR_NONE){
			FD_ZERO(&fd_select);
			FD_SET(io->remote_fd, &fd_select);

			if(ssl_error == SSL_ERROR_WANT_READ){
				if((retval = select(io->remote_fd + 1, &fd_select, NULL, NULL, NULL)) == -1){
					return(-1);
				}

				// if(ssl_error == SSL_ERROR_WANT_WRITE)
			}else{
				if((retval = select(io->remote_fd + 1, NULL, &fd_select, NULL, NULL)) == -1){
					return(-1);
				}
			}
		}

		retval = SSL_read(io->ssl, buff, count);

		switch(SSL_get_error(io->ssl, retval)){

			case SSL_ERROR_NONE:
			case SSL_ERROR_ZERO_RETURN:
				return(retval);
				break;

			case SSL_ERROR_WANT_READ:
				ssl_error = SSL_ERROR_WANT_READ;
				break;

			case SSL_ERROR_WANT_WRITE:
				ssl_error = SSL_ERROR_WANT_WRITE;
				break;

			default:
				return(-1);
		}
	} while(ssl_error);

	return(-1);
}

int remote_write_encrypted(struct remote_io_helper *io, void *buff, size_t count){
	
	int retval;
	fd_set fd_select;
	int ssl_error = SSL_ERROR_NONE;	

	do{

		if(ssl_error != SSL_ERROR_NONE){
			FD_ZERO(&fd_select);
			FD_SET(io->remote_fd, &fd_select);

			if(ssl_error == SSL_ERROR_WANT_READ){
				if((retval = select(io->remote_fd + 1, &fd_select, NULL, NULL, NULL)) == -1){
					return(-1);
				}

				// if(ssl_error == SSL_ERROR_WANT_WRITE)
			}else{
				if((retval = select(io->remote_fd + 1, NULL, &fd_select, NULL, NULL)) == -1){
					return(-1);
				}
			}
		}

		retval = SSL_write(io->ssl, buff, count);

		switch(SSL_get_error(io->ssl, retval)){

			case SSL_ERROR_NONE:
			case SSL_ERROR_ZERO_RETURN:
				return(retval);
				break;

			case SSL_ERROR_WANT_READ:
				ssl_error = SSL_ERROR_WANT_READ;
				break;

			case SSL_ERROR_WANT_WRITE:
				ssl_error = SSL_ERROR_WANT_WRITE;
				break;

			default:
				return(-1);
		}
	} while(ssl_error);

	return(-1);
}

int remote_printf(struct remote_io_helper *io, char *fmt, ...){

	int retval;
	char buff[BUFFER_SIZE];
	va_list list_ptr;


	va_start(list_ptr, fmt);

	memset(buff, 0, BUFFER_SIZE);
	retval = vsnprintf(buff, BUFFER_SIZE - 1, fmt, list_ptr);
	io->remote_write(io, buff, retval + 1);

	va_end(list_ptr);

	return(retval);
}

int print_error(struct remote_io_helper *io, char *fmt, ...){

	int retval = 0;
	va_list list_ptr;


	va_start(list_ptr, fmt);

	if(io->listener){
		retval = vfprintf(stderr, fmt, list_ptr);
		fflush(stderr);
	}else{
		retval = remote_printf(io, fmt, list_ptr); 
	}

	return(retval);
}
