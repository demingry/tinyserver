#include<stdio.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<ctype.h>
#include<strings.h>
#include<string.h>
#include<sys/stat.h>
#include<pthread.h>
#include<sys/wait.h>
#include<stdlib.h>
#include<stdint.h>

#define SERVER_STRING "Server: tinyserver\r\n"
#define STDIN   0
#define STDOUT  1
#define STDERR  2

#define SERVER_PORT 4000
#define CUSTOMIZE_RES 0


int create_socket();
void accept_request(void *);
static void _response(int, int);
void serv_file(int, const char *);
void filecontent(int, FILE *);
int get_line(int, char *, int);

int main(){

    int socket = create_socket();
    if(!socket) return -1;
    printf("server listening on port : %d\n",SERVER_PORT);

    struct sockaddr_in client_info;
    socklen_t client_info_len = sizeof(client_info);
    pthread_t newthread;

    while(1){
        int conn = accept(socket,(struct sockaddr *)&client_info,&client_info_len);
        if(conn == -1){
            printf("[!]Error in establishing connection with peer\n");
            return -1;
        }
        if(pthread_create(&newthread,NULL,(void *)accept_request,(void *)(intptr_t)conn) != 0){
            printf("[!]Error in create new thread\n");
            continue;
        }
        pthread_join(newthread,NULL);
    }

    close(socket);
    return 0;
}

enum response_type{success, notsupported, notfound, servererror, force_notfound};

static void _response(int conn, int response_type){
    
    char buf[1024];

    switch(response_type){
        case notsupported:
            sprintf(buf,"HTTP/1.0 501 Method Not Supported\r\n");
            send(conn, buf, strlen(buf), 0);
            break;
        case success:
            sprintf(buf, "HTTP/1.0 200 OK\r\n");
            send(conn, buf, strlen(buf), 0);
            break;
        case notfound:
            sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
            send(conn, buf, strlen(buf), 0);
            break;
        case force_notfound:
            sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
            send(conn, buf, strlen(buf), 0);
    }

    sprintf(buf, SERVER_STRING);
    send(conn, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(conn, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(conn, buf, strlen(buf), 0);

    if(CUSTOMIZE_RES){
        switch(response_type){
            const char *filename;
            case notsupported:
                filename = "./html/notsupported.html";
                serv_file(conn, filename);
                return;
            case force_notfound:
                filename = "./html/notfound.html";
                serv_file(conn, filename);
                return;
            case notfound:
                filename = "./html/notfound.html";
                serv_file(conn, filename);
                return;
            default:
                break;
        }
    }


    switch(response_type){
        case notsupported:
            sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
            send(conn, buf, strlen(buf), 0);
            sprintf(buf, "</TITLE></HEAD>\r\n");
            send(conn, buf, strlen(buf), 0);
            sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
            send(conn, buf, strlen(buf), 0);
            sprintf(buf, "</BODY></HTML>\r\n");
            send(conn, buf, strlen(buf), 0);
            break;
        case notfound:
            sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
            send(conn, buf, strlen(buf), 0);
            sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
            send(conn, buf, strlen(buf), 0);
            sprintf(buf, "your request because the resource specified\r\n");
            send(conn, buf, strlen(buf), 0);
            sprintf(buf, "is unavailable or nonexistent.\r\n");
            send(conn, buf, strlen(buf), 0);
            sprintf(buf, "</BODY></HTML>\r\n");
            send(conn, buf, strlen(buf), 0);
            break;
        case force_notfound:  //in customize condition force to notfound
            sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
            send(conn, buf, strlen(buf), 0);
            sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
            send(conn, buf, strlen(buf), 0);
            sprintf(buf, "your request because the resource specified\r\n");
            send(conn, buf, strlen(buf), 0);
            sprintf(buf, "is unavailable or nonexistent.\r\n");
            send(conn, buf, strlen(buf), 0);
            sprintf(buf, "</BODY></HTML>\r\n");
            send(conn, buf, strlen(buf), 0);
    }
}

int create_socket(){

    struct sockaddr_in serv_info;

    int httpd = socket(PF_INET,SOCK_STREAM,0);
    if(httpd == -1){
        printf("[!]Error in create server socket\n");
        return -1;
    }

    memset(&serv_info,0,sizeof(serv_info));    //defination in struct sockaddr_in padding 0
    serv_info.sin_family = AF_INET;
    serv_info.sin_port = htons(((uint16_t)SERVER_PORT));   //host to network
    serv_info.sin_addr.s_addr = htonl(INADDR_ANY);

    int reused = 1;

    if(setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &reused, sizeof(reused)) < 0){
        printf("[!]Error in set socket options\n");
        return -1;
    }

    if(bind(httpd,(struct sockaddr *)&serv_info,sizeof(serv_info)) < 0 ){
        printf("[!]Error in bind\n");
        return -1;
    }

    if(listen(httpd, 10) < 0){
        printf("[!]Error in listen\n");
        return -1;
    }
    return httpd;
}

#define is_space(x) isspace((int)(x))

void accept_request(void *arg){

    int conn = (intptr_t)arg;
    char buf[1024];
    size_t numreceive;
    char method[255];
    char url[255];
    char path[512];
    size_t offseti = 0;
    size_t offsetj = 0;
    struct stat st;
    char *query_string = NULL;


    numreceive = get_line(conn,buf,sizeof(buf));
    while(!is_space(buf[offseti]) && (offseti < sizeof(method) -1)){
        method[offseti] = buf[offseti];
        offseti++;
    }
    offsetj=offseti;
    method[offseti] = '\0';

    if(strcasecmp(method, "GET")){
        enum response_type err = notsupported;
        _response(conn,err);
        return;
    }
    offseti=0;
    while(is_space(buf[offsetj]) && offsetj < numreceive) offsetj++;

    while(!is_space(buf[offsetj]) && (offseti < sizeof(url) - 1) && offsetj < numreceive){
        url[offseti] = buf[offsetj];
        offseti++;
        offsetj++;
    }
    url[offseti] = '\0';

    if(strcasecmp(method, "GET") == 0){
        query_string = url;
        while((*query_string != '?') && (*query_string != '\0')) query_string++;
        if(*query_string == '?'){
            *query_string = '\0';
            query_string++; //截取`?`后面的字符串
        }
    }

    sprintf(path, "html%s", url);
    if(path[strlen(path) - 1] =='/')
        strcat(path, "index.html");
    if(stat(path, &st) == -1){   //文件不存在html/index.html
        while((numreceive > 0) && strcmp("\n", buf))
            numreceive = get_line(conn, buf, sizeof(buf));
        enum response_type err = notfound;
        _response(conn, err);
    }else{
        if((st.st_mode & S_IFMT) == S_IFDIR)    //如果请求路径是文件夹
            strcat(path,"/index.html");
        
        serv_file(conn, path);
    }

    close(conn);
}


void serv_file(int conn, const char *filename){
    FILE *resource = NULL;
    int numchars;
    char buf[1024];

    buf[0] = 'A'; buf[1] = '\0';

    while((numchars > 0) && strcmp("\n", buf))
        numchars = get_line(conn, buf, sizeof(buf));
    
    resource = fopen(filename, "r");
    if(resource == NULL){
        enum response_type err;
        if(CUSTOMIZE_RES){
            err = force_notfound;
        }else{
            err = notfound;
        }
        _response(conn, err);
    }else{
        enum response_type suc = success;
        _response(conn, suc);
        filecontent(conn, resource);
    }

    fclose(resource);
}

void filecontent(int conn, FILE *resource){
    
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while(!feof(resource)){
        send(conn, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}


int get_line(int sock, char *buf, int size){
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return(i);
}
