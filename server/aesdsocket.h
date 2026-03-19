#ifndef AESDSOCKET_H
#define AESDSOCKET_H

/* Standard library includes */
#define _POSIX_C_SOURCE 200809L
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* Constants */
#define PORT        "9000"
#define FILE_NAME   "/var/tmp/aesdsocketdata"
#define BACKLOG     5
#define RECV_BUFSIZE 4096

/* Globals shared across signal handler and main */
extern volatile sig_atomic_t caught_signal;
extern int socketfd;
extern int client_fd;

/* Function declarations */
void signal_handler(int signo);
int  setup_socket(void);
int handle_connection(void);
void cleanup(void);
int process_message (char* mssg);
int daemonize(void);
#endif /* AESDSOCKET_H */