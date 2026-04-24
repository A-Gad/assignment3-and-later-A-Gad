#include "aesdsocket.h"
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
timer_t timer;
volatile sig_atomic_t caught_signal = 0;
int socketfd = -1;
int c_fd = -1;

void signal_handler(int signo)
{
    if(signo == SIGTERM || signo == SIGINT)
    {
        
        caught_signal = 1;
    }
}
void cleanup(void)
{
    #if !USE_AESD_CHAR_DEVICE
    timer_delete(timer);
    remove(FILE_NAME);
    #endif
    pthread_mutex_destroy(&file_mutex);
    if(socketfd != -1)  close(socketfd);
    if(c_fd != -1)  close(c_fd); 
    closelog();
}

int setup_socket(void)
{
    int rc;
    struct addrinfo hints;
    struct addrinfo *res;

    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    rc = getaddrinfo(NULL,PORT,&hints,&res);
    if (rc != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        return -1;
    }
    socketfd = socket(res->ai_family,res->ai_socktype, res->ai_protocol);
    if (socketfd == -1)
    {
        perror("socket");
        return -1;
    }
    int yes = 1;
    if(setsockopt(socketfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)) == -1)
    {
        perror("setsockopt");
        return -1;
    }
    if(bind(socketfd,res->ai_addr,res->ai_addrlen) == -1)
    {
        freeaddrinfo(res);
        perror("bind");
        return -1;
    }
    freeaddrinfo(res);
    if (listen(socketfd,BACKLOG) == -1)
    {
        perror("listen");
        return -1;
    }
    return 0;
}

//thread function
void *thread_connection(void *args)
{
    thread_node_t *thread = (thread_node_t*)args;//cast the args ptr to thread node struct

    char* heap_buffer;
    char stack_buffer[RECV_BUFSIZE];
    size_t capacity = 8;
    size_t msg_pos = 0;
    heap_buffer = malloc(capacity);
    if(heap_buffer == NULL)
    {
    perror("malloc");
    thread->thread_complete = true;
    return NULL;
    }
    while(1)
    {
        int n = recv(thread->client_fd,stack_buffer,sizeof(stack_buffer),0);

        if (n == 0) //completion
        {
            free(heap_buffer);
            close(thread->client_fd);
            thread->client_fd = -1;
            syslog(LOG_INFO, "Closed connection from %s", thread->client_ip);
            thread->thread_complete = true;
            return NULL;
        } 
        if (n == -1) 
        {   
            free(heap_buffer);
            thread->thread_complete = true;
            return NULL;
        } //failiure
        for(int i =0; i < n; i++)
        {
            if(stack_buffer[i] == '\n')
            {
                //message complete
                heap_buffer[msg_pos] = '\0'; //null termintating the buffer before passing it 
                pthread_mutex_lock(&file_mutex);//lock
                if(process_message(heap_buffer, thread->client_fd) == -1)
                {
                    free(heap_buffer);
                    close(thread->client_fd);
                    pthread_mutex_unlock(&file_mutex);//unlock in case of failure
                    thread->client_fd = -1;
                    thread->thread_complete = true;
                    return NULL;
                }
                pthread_mutex_unlock(&file_mutex);//unlock when finished 
                msg_pos = 0;
            }
            else
            {
                if(msg_pos == capacity)
                {
                //buffer full, resize
                capacity *= 2;
                char *temp = realloc(heap_buffer, capacity);
                if(temp == NULL)
                {
                    free(heap_buffer);
                    close(thread->client_fd);
                    thread->client_fd = -1;
                    perror("realloc");
                    thread->thread_complete = true;
                    return NULL;
                }
                heap_buffer = temp;
                }
                heap_buffer[msg_pos++] = stack_buffer[i];

            }
        }
    }
    thread->thread_complete = true;
    return NULL;
}
int process_message (char* mssg, int fd)
{
    //FILE * file = fopen(FILE_NAME,"a"); //************
    #if USE_AESD_CHAR_DEVICE
    int file = open(FILE_NAME, O_WRONLY);
    #else
    int file = open(FILE_NAME, O_WRONLY | O_CREAT | O_APPEND, 0644);
    #endif
    if (file == -1)
    {
        perror("open");
        return -1;
    }
    if (write(file, mssg, strlen(mssg)) == -1) 
    {
        perror("write");
        close(file);
        return -1;
    }
    if (write(file, "\n", 1) == -1) 
    {
        perror("write");
        close(file);
        return -1;
    }
    close(file);

    //FILE* read_file = fopen(FILE_NAME,"r");   //************************
    int read_file = open(FILE_NAME,O_RDONLY);
    if (read_file == -1)
    {
        perror("open");
        return -1;
    }
    char send_buffer[RECV_BUFSIZE] ;
    size_t bytes;
    while ((bytes = read(read_file,send_buffer,sizeof(send_buffer))) > 0)
    {
        if(send(fd,send_buffer,bytes,0) == -1)
        {
            perror("send");
            close(read_file);
            return -1;
        }
    }
    close(read_file);
    return 0;
}

int daemonize(void)
{
    pid_t pid = fork();
    if(pid == -1) { perror("fork"); return -1; }
    if(pid > 0)   { exit(0); }  // parent exits

    // child continues 
    if(setsid() == -1)
    {
        perror("setsid");
        return -1;
    }

    int fd = open("/dev/null", O_RDWR);
    if(fd == -1)
    {
        perror("open /dev/null");
        return -1;
    }
    if(dup2(fd, STDIN_FILENO) == -1) {close(fd);perror("dup2"); return -1;}
    if(dup2(fd, STDOUT_FILENO) == -1) {close(fd);perror("dup2"); return -1;}
    if(dup2(fd, STDERR_FILENO) == -1) {close(fd);perror("dup2"); return -1;}

    close(fd);

    return 0;
}
/*timer call back function*/
void timer_callback(union sigval sv)
{
    (void)sv;
    char buf[64];
    struct timespec ts;
    struct tm tm_info;
    clock_gettime(CLOCK_REALTIME,&ts);
    if((localtime_r(&ts.tv_sec,&tm_info)) == NULL)
    {
        perror("localtime_r");
        return ;
    }
    strftime(buf,sizeof(buf),"%a, %d %b %Y %T %z",&tm_info);
    pthread_mutex_lock(&file_mutex);

    FILE * file = fopen(FILE_NAME,"a"); //*************************
    if (file == NULL)
    {
        perror("fopen");
        pthread_mutex_unlock(&file_mutex);
        return ;
    }
    if(fprintf(file, "timestamp:%s\n", buf) < 0)
    {
        fclose(file);
        perror("fprintf");
        pthread_mutex_unlock(&file_mutex);
        return ;
    }
    fclose(file);
    pthread_mutex_unlock(&file_mutex);
}

int main(int argc, char *argv[])
{
    int daemon_mode = 0;
    if(argc > 2)
    {
        fprintf(stderr, "usage: ./aesdsocket or ./aesdsocket -d\n");
        return 1;
    }
    if (argc == 2)
    {
        if (strcmp(argv[1], "-d") == 0)
        {
            daemon_mode = 1;
        }
        else
        {
        printf("usage: ./aesdsocket [-d]\n");
        return -1;
        }
    }
    openlog("aesdsocket", LOG_PID, LOG_USER);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }
    if(sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }

    if(setup_socket() == -1)
    {
        cleanup();
        return -1;
    }    
    if(daemon_mode)
    {
        if(daemonize() == -1)
        {
            cleanup();
            return -1;
        }
    }
    
    struct sigevent sev;
    struct itimerspec its;

    memset(&sev,0,sizeof(sev));
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = timer_callback;
    sev.sigev_notify_attributes = NULL;
    #if !USE_AESD_CHAR_DEVICE
    if(timer_create(CLOCK_REALTIME,&sev,&timer) == -1)
    {
        perror("timer_create");
        return -1;
    }
    memset(&its,0,sizeof(its));
    its.it_value.tv_sec = 10;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 10;
    its.it_interval.tv_nsec = 0;

    timer_settime(timer,0,&its,NULL);
    #endif

    thread_node_t *np,*tmp;
    SLIST_HEAD(thread_list_s, thread_node) thread_list = SLIST_HEAD_INITIALIZER(thread_list);

    while(!caught_signal)
    {
        struct sockaddr_storage client_addr;
        socklen_t addr_size = sizeof(client_addr);
        c_fd = accept(socketfd, (struct sockaddr*)&client_addr, &addr_size);
        if(c_fd == -1)
        {
            if(errno == EINTR) break;  //signal interrupted, exit loop cleanly
            perror("accept");
            cleanup();
            return -1;
        }
        char client_ip[INET6_ADDRSTRLEN];
        struct sockaddr_in *s = (struct sockaddr_in *)&client_addr;
        inet_ntop(AF_INET, &s->sin_addr, client_ip, sizeof(client_ip));

        thread_node_t *node = malloc(sizeof(thread_node_t));
        if(node == NULL) { perror("malloc"); cleanup(); return -1; }
        node->client_fd = c_fd;
        strcpy(node->client_ip,client_ip);
        node->thread_complete = false;

        int res = pthread_create(&node->tid,NULL,thread_connection,node);
        if(res != 0)
        {
            perror("pthread_create");
            free(node);
            return -1;
        }

        SLIST_INSERT_HEAD(&thread_list,node,entries);//append the new node to the head of the list

        SLIST_FOREACH_SAFE(np,&thread_list,entries,tmp)//traverse the list to look for joinable threads
        {
            if(np->thread_complete == true)
            {
                pthread_join(np->tid, NULL);
                SLIST_REMOVE(&thread_list, np, thread_node, entries);
                free(np);
            }
              
        }        
    }
    syslog(LOG_INFO,"Caught signal, exiting");
    SLIST_FOREACH_SAFE(np, &thread_list, entries, tmp)
    {
        pthread_join(np->tid, NULL);
        SLIST_REMOVE(&thread_list, np, thread_node, entries);
        free(np);
    }
    cleanup();
    return 0;
}