#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>

#define BACKLOG 10
#define	MAXLINE	 8192  /* Max text line length */
#define MAXBUF   8192  /* Max I/O buffer size */

void process_http_get(int fd, char *url, char *filename);
void *get_in_addr(struct sockaddr *sa);
int parse_url(char *url,char *filename);
void get_filetype(char *filename,char *filetype);
void serve_static(int fd,char *filename,int filesize);
void client_error(int fd,char *cause,char *errnum,char *shortmsg,char *longmsg);

int main(int argc, char **argv)
{
    int sockfd, connfd;
    char *port, buf[MAXBUF], method[MAXLINE], url[MAXLINE], version[MAXLINE];
    char filename[MAXLINE];
    struct sockaddr_in sever_address;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage other_addr;
    socklen_t sin_size;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    if(argc != 2){
        fprintf(stderr,"usage:%s <port>\n",argv[0]);
        exit(1);
    }
    port = argv[1];

    memset(&hints, 0, sizeof(hints));
    bzero(&sever_address,sizeof(sever_address));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;


    if((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo:%s\n", gai_strerror(rv));
        return 1;
    }

    for(p = servinfo;p != NULL;p = p->ai_next){

        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("server: socket");
            continue;
        }
        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
            perror("setsockopt");
            exit(1);
        }
        if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            close(sockfd);
            perror("server:bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if(p == NULL){
        fprintf(stderr, "server : failed to bind\n");
        exit(1);
    }

    if(listen(sockfd, BACKLOG) == -1){
        perror("listen");
        exit(1);
    }

    //printf("server:waiting for connections...\n");

    while(1){
        sin_size = sizeof(other_addr);
        connfd = accept(sockfd, (struct sockaddr *)&other_addr, &sin_size);
        if(connfd == -1){
            perror("accept");
            continue;
        }

        //inet_ntop(other_addr.ss_family, get_in_addr((struct sockaddr*)&other_addr), s, sizeof(s));
        //printf("server:got connection from %s\n",s);
        
        if(!fork()){
            close(sockfd);                       
            if(recv(connfd, buf, MAXBUF, 0) == -1) {
                perror("receive");
                close(connfd);
                exit(1);
            }
            printf("%s\n",buf);
            sscanf(buf,"%s %s %s",method,url,version);
            process_http_get(connfd,url,filename);
            /*if(!strcasecmp(method, "GET"))     
                process_http_get(connfd,url,filename);
            else
                client_error(connfd,method,"501","Not Implemented","Webserver does not implement this method");
            */
            close(connfd);
            exit(0);
        }
        close(connfd);
    }

    return 0;
}

void process_http_get(int fd, char *url, char *filename)
{
    struct stat buf;
    parse_url(url, filename);
    /*if(stat(filename,&buf)<0)
        client_error(fd,filename,"404","Not found","Webserver couldn't find this file");
    else{
        if(!(S_ISREG(buf.st_mode))||!(S_IRUSR & buf.st_mode)){      
            client_error(fd,filename,"403","forbidden","Webserver couldn't read the file");
        }
        serve_static(fd,filename,buf.st_size);
    }*/
    stat(filename,&buf);
    serve_static(fd,filename,buf.st_size);
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int parse_url(char *url,char *filename)
{
    strcpy(filename,".");
    strcat(filename,url);
    /*if(url[strlen(url)-1] == '/')
        strcat(filename,"home.html");*/
    return 1;
}

void get_filetype(char *filename,char *filetype)
{
    if(strstr(filename,".html"))
        strcpy(filetype,"text/html");
    else if (strstr(filename,".png"))
        strcpy(filetype,"image/png");
    else if (strstr(filename,".jpg"))
        strcpy(filetype,"image/jpeg");
    else
        strcpy(filetype,"text/plain");
}

void serve_static(int fd,char *filename,int filesize)
{
    int srcfd;
    char *srcp,filetype[MAXLINE],buf[MAXBUF];

    get_filetype(filename,filetype);
    sprintf(buf,"HTTP/1.1 200 OK\r\n");
    sprintf(buf,"%sServer:Web Server\r\n",buf);
    sprintf(buf,"%sContent-length:%d\r\n",buf,filesize);
    sprintf(buf,"%sContent-type:%s\r\n\r\n",buf,filetype);
    send(fd,buf,strlen(buf),0);

    srcfd = open(filename,O_RDONLY,0);
    srcp = mmap(0,filesize,PROT_READ,MAP_PRIVATE,srcfd,0);
    close(srcfd);
    send(fd,srcp,filesize,0);
    munmap(srcp,filesize);
}

void client_error(int fd,char *cause,char *errnum,char *shortmsg,char *longmsg)
{
    char buf[MAXLINE],body[MAXBUF];

    sprintf(body,"<html><title>Tiny Error</title>");
    sprintf(body,"%s<body bgcolor=""ffffff"">\r\n",body);
    sprintf(body,"%s%s:%s\r\n",body,errnum,shortmsg);
    sprintf(body,"%s<p>%s:%s\r\n",body,longmsg,cause);
    sprintf(body,"%s<hr><em>The Web server</em>\r\n",body);

    sprintf(buf,"HTTP/1.1 %s %s\r\n",errnum,shortmsg);
    send(fd,buf,strlen(buf),0);
    sprintf(buf,"Content-type: text/html\r\n");
    send(fd,buf,strlen(buf),0);
    sprintf(buf,"Content-length: %d\r\n\r\n",(int)strlen(body));
    send(fd,buf,strlen(buf),0);
    send(fd,body,strlen(body),0);
}