
#include "common.h"

extern sig_atomic_t sig_found;


/***********************************************************************************************************************
 *
 * broker()
 *
 * Input: A pointer to our io_helper object and a pointer to our config_helper object.
 * Output: 0 for EOF, -1 for errors.
 *
 * Purpose: Broker data between the terminal and the network socket. 
 *
 **********************************************************************************************************************/
int broker(struct config_helper *config){

	int retval = -1;
	int found;

	fd_set read_fds, write_fds;
	int fd_max;

	struct sigaction act;
	int current_sig;

	struct message_helper *message;

	struct proxy_node *cur_proxy_node;
	struct connection_node *cur_connection_node, *next_connection_node;
	
	unsigned long tmp_ulong;
	struct timeval timeout, *timeout_ptr;

	struct proxy_request_node *cur_proxy_req_node;


	/* We use this as a shorthand to make message syntax more readable. */
	message = &io->message;

	if(config->interactive){

		/* Set up proxies requested during launch. */
		cur_proxy_req_node = config->proxy_request_head;	
		while(cur_proxy_req_node){

			cur_proxy_node = proxy_node_new(cur_proxy_req_node->request_string, cur_proxy_req_node->type);	

			if(!cur_proxy_node){
				report_error("do_control(): proxy_node_new(%s, %d): %s", cur_proxy_req_node->request_string, cur_proxy_req_node->type, strerror(errno));
			}else{

				if(!io->proxy_head){
					io->proxy_head = cur_proxy_node;
					io->proxy_tail = cur_proxy_node;
				}else{
					io->proxy_tail->next = cur_proxy_node;
					io->proxy_tail = cur_proxy_node;
				}
			}
			cur_proxy_req_node = cur_proxy_req_node->next;
		}


		/* Prepare for window resize event handling. */
		memset(&act, 0, sizeof(act));
		act.sa_handler = signal_handler;

		if((retval = sigaction(SIGWINCH, &act, NULL)) == -1){
			report_error("broker(): sigaction(SIGWINCH, %lx, NULL): %s", (unsigned long) &act, strerror(errno));
			return(-1);
		}

		if((io->tty_winsize = (struct winsize *) calloc(1, sizeof(struct winsize))) == NULL){
			report_error("broker(): calloc(1, %d): %s", (int) sizeof(struct winsize), strerror(errno));
			return(-1);
		}

		if(io->controller){
			if(config->tun){
				if((cur_connection_node = handle_tun_tap_init(IFF_TUN)) == NULL){
					report_error("broker(): handle_tun_tap_init(%d): %s", IFF_TUN, strerror(errno));
				}else{
					handle_connection_read(cur_connection_node);
				}
			}

			if(config->tap){
				if((cur_connection_node = handle_tun_tap_init(IFF_TAP)) == NULL){
					report_error("broker(): handle_tun_tap_init(%d): %s", IFF_TAP, strerror(errno));
				}else{
					handle_connection_read(cur_connection_node);
				}
			}
		}
	}

	timeout_ptr = NULL;
	if(config->nop){
		timeout_ptr = &timeout;
	}

	/*  Start the broker() loop. */
	while(1){

		fd_max = 0;
		FD_ZERO(&read_fds);
		FD_ZERO(&write_fds);

		FD_SET(io->local_in_fd, &read_fds);
		fd_max = io->local_in_fd > fd_max ? io->local_in_fd : fd_max;

		FD_SET(io->remote_fd, &read_fds);
		fd_max = io->remote_fd > fd_max ? io->remote_fd : fd_max;

		io->fd_count = 2;

		cur_connection_node = io->connection_head;
		while((io->fd_count < FD_SETSIZE) && cur_connection_node){
			/*
				 fprintf(stderr, "\r-- FD_SET() --\n\rDEBUG: origin|id: %d|%d\n", cur_connection_node->origin, cur_connection_node->id);
				 fprintf(stderr, "\rDEBUG: cur_connection_node: %lx\n", (unsigned long) cur_connection_node);
				 fprintf(stderr, "\rDEBUG: cur_connection_node->state: %d\n", cur_connection_node->state); 
				 fprintf(stderr, "\rDEBUG: cur_connection_node->write_head: %lx\n", (unsigned long) cur_connection_node->write_head);
			 */

			if(! ((cur_connection_node->state == CON_DORMANT) || (cur_connection_node->state == CON_READY) || (cur_connection_node->state == CON_EINPROGRESS))){
				FD_SET(cur_connection_node->fd, &read_fds);
				fd_max = cur_connection_node->fd > fd_max ? cur_connection_node->fd : fd_max;
			}

			if(cur_connection_node->write_head || cur_connection_node->state == CON_EINPROGRESS){
				/*
					 fprintf(stderr, "\r-- FD_SET() write --\n\rDEBUG: origin|id: %d|%d\n", cur_connection_node->origin, cur_connection_node->id);
					 fprintf(stderr, "\rDEBUG: cur_connection_node: %lx\n", (unsigned long) cur_connection_node);
					 fprintf(stderr, "\rDEBUG: cur_connection_node->state: %d\n", cur_connection_node->state); 
					 fprintf(stderr, "\rDEBUG: cur_connection_node->write_head: %lx\n", (unsigned long) cur_connection_node->write_head);
				 */
				FD_SET(cur_connection_node->fd, &write_fds);
				fd_max = cur_connection_node->fd > fd_max ? cur_connection_node->fd : fd_max;
			}

			cur_connection_node = cur_connection_node->next;
			io->fd_count++;
		}

		/*
			 Only add proxy file descriptors to select() if we have enough space for more connections.
			 Skip the proxy listeners from the select() loop otherwise.
		 */
		cur_proxy_node = io->proxy_head;
		while((io->fd_count < FD_SETSIZE) && cur_proxy_node){

			FD_SET(cur_proxy_node->fd, &read_fds);
			fd_max = cur_proxy_node->fd > fd_max ? cur_proxy_node->fd : fd_max;

			cur_proxy_node = cur_proxy_node->next;
			io->fd_count++;
		}

		if(config->nop){
			tmp_ulong = rand();
			timeout.tv_sec = config->retry_start + (tmp_ulong % (config->retry_stop - config->retry_start));
			timeout.tv_usec = 0;
		}

		if(((retval = select(fd_max + 1, &read_fds, &write_fds, NULL, timeout_ptr)) == -1) \
				&& !sig_found){
			report_error("broker(): select(%d, %lx, %lx, NULL, %lx): %s", \
					fd_max + 1, (unsigned long) &read_fds, (unsigned long) &write_fds, (unsigned long) timeout_ptr, strerror(errno));
			goto CLEAN_UP;
		}

		//		fprintf(stderr, "\rDEBUG: select()\n");

		if(!retval){
			if((retval = handle_send_nop()) == -1){
				report_error("broker(): handle_send_nop(): %s", strerror(errno));
				goto CLEAN_UP;
			}
		}

		/* Determine which case we are in and call the appropriate handler. */

		if(sig_found){

			current_sig = sig_found;
			sig_found = 0;

			if(config->interactive && io->controller){

				/* I set this up as a switch statement because I think we will want to handle other signals down the road. */
				switch(current_sig){

					/* Gather and send the new window size. */
					case SIGWINCH:
						if((retval = handle_signal_sigwinch()) == -1){
							report_error("broker(): handle_signal_sigwinch(): %s", strerror(errno));
							goto CLEAN_UP;
						}
						break;
				}
			}

			continue;
		}

		if(FD_ISSET(io->local_in_fd, &write_fds)){

			if((retval = handle_local_write()) == -1){
				goto CLEAN_UP;
			}

			continue;
		}

		if(FD_ISSET(io->local_in_fd, &read_fds)){

			retval = handle_local_read();

			if(retval < 0){
				if(retval == -1){
					report_error("broker(): handle_local_read(): %s", strerror(errno));
				}else{
					retval = 0;
				}
				goto CLEAN_UP;
			}	

			continue;
		}

		if(FD_ISSET(io->remote_fd, &read_fds)){

			if((retval = message_pull()) == -1){
				if(io->eof){
					retval = 0;
				}else{
					report_error("broker(): message_pull(): %s", strerror(errno));
				}
				goto CLEAN_UP;
			}

			switch(message->data_type){

				case DT_TTY:

					if((retval = handle_message_dt_tty()) == -1){
						report_error("broker(): handle_message_dt_tty(): %s", strerror(errno));
						goto CLEAN_UP;
					}

					break;

				case DT_WINRESIZE:

					if(!io->controller){
						if((retval = handle_message_dt_winresize()) == -1){
							report_error("broker(): handle_message_dt_winresize(): %s", strerror(errno));
							goto CLEAN_UP;
						}
					}

					break;

				case DT_PROXY:

					if(message->header_type == DT_PROXY_HT_DESTROY){

						if((retval = handle_message_dt_proxy_ht_destroy()) == -1){
							report_error("broker(): handle_message_dt_proxy_ht_destroy(): %s", strerror(errno));
							goto CLEAN_UP;
						}

					}else if(message->header_type == DT_PROXY_HT_CREATE){

						retval = handle_message_dt_proxy_ht_create();

						if(retval == -1){
							report_error("broker(): handle_message_dt_proxy_ht_create(): %s", strerror(errno));
							goto CLEAN_UP;
						}

					}else if(message->header_type == DT_PROXY_HT_RESPONSE){

						if((retval = handle_message_dt_proxy_ht_response()) == -1){
							report_error("broker(): handle_message_dt_proxy_ht_response(): %s", strerror(errno));
							goto CLEAN_UP;
						}

					}else{
						// Malformed request.
						report_error("broker(): Unknown Proxy Header Type: %d", message->header_type);
						retval = -1;
						goto CLEAN_UP;
					}
					break;

				case DT_CONNECTION:

					if((retval = handle_message_dt_connection()) == -1){
						report_error("broker(): handle_message_dt_connection(): %s", strerror(errno));
						goto CLEAN_UP;
					}

					break;

				case DT_NOP:
					// Cool story, bro.
					break;

				case DT_ERROR:
					if(io->controller){
						if((retval = report_log("Target Error: %s", message->data)) == -1){
							goto CLEAN_UP;
						}
					}
					break;

				default:
					// Malformed request.
					report_error("broker(): Unknown Proxy Header Type: %d", message->header_type);
					break;
			}

			continue;
		}

		found = 0;
		cur_proxy_node = io->proxy_head;
		while(cur_proxy_node){

			if(FD_ISSET(cur_proxy_node->fd, &read_fds)){
				if((retval = handle_proxy_read(cur_proxy_node)) == -1){
					report_error("broker(): handle_proxy_read(%lx): %s", (unsigned long) cur_proxy_node, strerror(errno));
					goto CLEAN_UP;
				}

				found = 1;
				break;
			}

			cur_proxy_node = cur_proxy_node->next;		
		}

		if(found){
			continue;
		}


		cur_connection_node = io->connection_head;
		while(cur_connection_node){
			// Advancing to the next node in the list now, in case cur_connection_node gets deleted in the processing of the loop.
			next_connection_node = cur_connection_node->next;

			if(FD_ISSET(cur_connection_node->fd, &write_fds)){

				if(cur_connection_node->state == CON_EINPROGRESS){
					if((retval = handle_con_activate(cur_connection_node)) == -1){
						report_error("broker(): handle_con_activate(%lx): %s", (unsigned long) cur_connection_node, strerror(errno));
						goto CLEAN_UP;
					}
				} else {
					/*
						 fprintf(stderr, "\r-- FD_ISSET() write --\n\rDEBUG: origin|id: %d|%d\n", cur_connection_node->origin, cur_connection_node->id);
						 fprintf(stderr, "\rDEBUG: cur_connection_node: %lx\n", (unsigned long) cur_connection_node);
						 fprintf(stderr, "\rDEBUG: cur_connection_node->state: %d\n", cur_connection_node->state); 
						 fprintf(stderr, "\rDEBUG: cur_connection_node->write_head: %lx\n", (unsigned long) cur_connection_node->write_head);
					 */
					if((retval = handle_connection_write(cur_connection_node)) == -1){
						report_error("broker(): handle_connection_write(%lx): %s", (unsigned long) cur_connection_node, strerror(errno));
						goto CLEAN_UP;
					}
				}

				break;
			}

			if(FD_ISSET(cur_connection_node->fd, &read_fds)){

				if((retval = handle_connection_read(cur_connection_node)) == -1){
					report_error("broker(): handle_connection_read(%lx): %s", (unsigned long) cur_connection_node, strerror(errno));
					goto CLEAN_UP;
				}

				if(retval == 0){
					connection_node_queue(cur_connection_node);
				}

				break;
			}

			cur_connection_node = next_connection_node;
		}

	}

	report_error("broker(): while(1): Should not be here!");
	retval = -1;

CLEAN_UP:

	// right now things are fatal at this point, so we're letting the kernel clean up our mallocs and close our sockets.

	return(retval);
}



/***********************************************************************************************************************
 * 
 * signal_handler()
 *
 * Input: The signal being handled.
 * Output: None. 
 * 
 * Purpose: To handle signals! For best effort at avoiding race conditions, we simply mark that the signal was found
 *	and return. This allows the broker() select() call to manage signal generating events.
 * 
 **********************************************************************************************************************/
void signal_handler(int signal){
	sig_found = signal;
}
