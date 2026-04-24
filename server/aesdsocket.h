#ifndef AESDSOCKET_H
#define AESDSOCKET_H
#define _POSIX_C_SOURCE 200809L
/* Standard library includes */

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
#include <pthread.h>
#include <stdbool.h>
#include <sys/queue.h>
#include <time.h>
#define SLIST_FOREACH_SAFE(var, head, field, tvar)          \
    for((var) = SLIST_FIRST(head);                          \
        (var) && ((tvar) = SLIST_NEXT(var, field), 1);      \
        (var) = (tvar))

/* Constants */
#if USE_AESD_CHAR_DEVICE
    #define FILE_NAME "/dev/aesdchar"
#else
    #define FILE_NAME "/var/tmp/aesdsocketdata"
#endif

#define PORT        "9000"
#define BACKLOG     5
#define RECV_BUFSIZE 4096

/* Globals shared across signal handler and main */
extern volatile sig_atomic_t caught_signal;
extern int socketfd;
extern int c_fd;
extern timer_t timer;
extern pthread_mutex_t file_mutex;
/*thread node*/
typedef struct thread_node
{
    pthread_t tid;
    int client_fd;
    char client_ip[INET_ADDRSTRLEN];
    bool thread_complete;
    SLIST_ENTRY(thread_node) entries;
    
}thread_node_t;

/* Function declarations */
void signal_handler(int signo);
int  setup_socket(void);
void cleanup(void);
int process_message (char* mssg, int fd);
int daemonize(void);
void *thread_connection(void *args);
void timer_callback(union sigval sv);
#endif /* AESDSOCKET_H */

