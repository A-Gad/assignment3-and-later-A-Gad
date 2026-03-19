#include "aesdsocket.h"

volatile sig_atomic_t caught_signal = 0;
int socketfd = -1;
int client_fd = -1;

void signal_handler(int signo)
{
    if(signo == SIGTERM || signo == SIGINT)
    {
        
        caught_signal = 1;
    }
}
void cleanup(void)
{
    if(socketfd != -1)  close(socketfd);
    if(client_fd != -1)  close(client_fd);
    remove(FILE_NAME);
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

int handle_connection(void)
{
    struct sockaddr_storage client_addr;
    socklen_t addr_size = sizeof(client_addr);
    client_fd = accept(socketfd, (struct sockaddr*)&client_addr, &addr_size);

    if(client_fd == -1)
    {
        if(errno == EINTR)
        {
            return 0;
        }
        perror("accept");
        return -1;
    }
    char client_ip[INET6_ADDRSTRLEN];
    struct sockaddr_in *s = (struct sockaddr_in *)&client_addr;
    inet_ntop(AF_INET, &s->sin_addr, client_ip, sizeof(client_ip));
    syslog(LOG_INFO, "Accepted connection from %s", client_ip);

    char* heap_buffer;
    char stack_buffer[RECV_BUFSIZE];
    size_t capacity = 8;
    size_t msg_pos = 0;
    heap_buffer = malloc(capacity);
    if(heap_buffer == NULL)
    {
    perror("malloc");
    return -1;
    }
    while(1)
    {
        int n = recv(client_fd,stack_buffer,sizeof(stack_buffer),0);

        if (n == 0) //completion
        {
            free(heap_buffer);
            close(client_fd);
            client_fd = -1;
            syslog(LOG_INFO, "Closed connection from %s", client_ip);
            return 0;
        } 
        if (n == -1) 
        {   
            free(heap_buffer);
            if(errno == EINTR)
            {
                return 0;
            }
            return -1;
        } //failiure
        for(int i =0; i < n; i++)
        {
            if(stack_buffer[i] == '\n')
            {
                //message complete
                heap_buffer[msg_pos] = '\0'; //null termintating the buffer before passing it 
                if(process_message(heap_buffer) == -1)
                {
                    free(heap_buffer);
                    close(client_fd);
                    client_fd = -1;
                    return -1;
                }
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
                    close(client_fd);
                    client_fd = -1;
                    perror("realloc");
                    return -1;
                }
                heap_buffer = temp;
                }
                heap_buffer[msg_pos++] = stack_buffer[i];

            }
        }
    }
    return 0;
}

int process_message (char* mssg)
{
    FILE * file = fopen(FILE_NAME,"a");
    if (file == NULL)
    {
        perror("fopen");
        return -1;
    }
    if(fprintf(file, "%s\n", mssg) < 0)
    {
        fclose(file);
        perror("fprintf");
        return -1;
    }
    fflush(file);//redundant
    fclose(file);

    FILE* read_file = fopen(FILE_NAME,"r");
    if (read_file == NULL)
    {
        perror("fopen");
        return -1;
    }
    char send_buffer[RECV_BUFSIZE] ;
    size_t bytes;
    while ((bytes = fread(send_buffer,1,sizeof(send_buffer),read_file)) > 0)
    {
        if(send(client_fd,send_buffer,bytes,0) == -1)
        {
            perror("send");
            fclose(read_file);
            return -1;
        }
    }
    fclose(read_file);
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

int main(int argc, char *argv[])
{
    int daemon_mode = 0;
    if(argc > 2)
    {
        fprintf(stderr, "usage: ./aesdsocket or ./aesdsocket -d\n");
        return 1;
    }
    
    openlog("aesdsocket", LOG_PID, LOG_USER);
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));          // zero first
    sa.sa_handler  = signal_handler;     // callback function
    sigemptyset(&sa.sa_mask);            // no extra blocked signals
    sa.sa_flags    = 0;
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
    if(daemon_mode)
    {
        if(daemonize() == -1)
        {
            cleanup();
            return -1;
        }
    }
    while(!caught_signal)
    {
        if(handle_connection() == -1)
        {
            cleanup();
            return -1;
        }
    }
    syslog(LOG_INFO, "Caught signal, exiting");
    cleanup();
    return 0;
}