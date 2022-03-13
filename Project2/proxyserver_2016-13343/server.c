#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>

#define PORT 7777
#define BUFFER_SIZE 1000000
#define min(a, b) (((a) < (b)) ? (a) : (b))

void forward(int sock);

/*
 * Function:  sendbuf
 
 * --------------------
 * send length amount of data in the buffer
 *
 * Args
 * sock: socket descriptor
 * buffer: buffer with data
 * length: size of data to send
 *
 * returns: void
 */
int sendbuf(int sock, char *buffer, size_t length)
{
    int bytes;
    int offset = 0;
    while (length > 0)
    {
        bytes = send(sock, buffer + offset, length, 0);
        offset += bytes;
        length -= bytes;
        if (bytes < 0)
            return 0;
    }
    return 1;
}

/*
 * Function:  receiveall
 
 * --------------------
 * receive data from the socket until end or error
 *
 * Args
 * sock: socket descriptor
 * buffer: buffer with data
 *
 * returns: amount of data received, -1 or 0 when error
 */
int receiveall(int sock, char *buffer, int needmodification)
{
    int offset;
    int bytes;
    int isheader;

    char *contentstart = buffer;
    int contentlength = 0;
    char *tmp;

    offset = 0;
    bytes = 0;
    isheader = 1;

    while (1)
    {
        bytes = recv(sock, buffer + offset, 9000, 0);
        if (bytes <= 0)
        {
            return bytes;
        }

        // header(\r\n\r\n) detection
        // if detected mark after header(\r\n\r\n) as content start
        // get content length if given in header
        if (isheader)
            if (contentstart = strstr(buffer + offset, "\r\n\r\n"))
            {
                isheader = 0;
                contentstart += 4;
                if (tmp = strstr(buffer, "Content-Length"))
                {
                    contentlength = atoi(tmp + 16);
                }
                else
                    contentlength = 0;
            }

        offset += bytes;

        // when header is done
        // check if content is all loaded
        if (!isheader)
            if ((buffer + offset) - contentstart >= contentlength)
                break;
    }

    // modify data
    if (needmodification)
    {
        char *tmp = strstr(buffer, "20xx-xxxxx");
        if (tmp)
        {
            tmp[2] = '1';
            tmp[3] = '6';
            tmp[5] = '1';
            tmp[6] = '3';
            tmp[7] = '3';
            tmp[8] = '4';
            tmp[9] = '3';
        }
    }
    return offset;
}

/*
 * Function:  proxyresponse
 
 * --------------------
 * proxy server response to client
 *
 * Args
 * from: server socket descriptor
 * to: client socket descriptor
 * buffer: buffer to store data
 *
 * returns: void
 */
void proxyresponse(int from, int to, char *buffer, int need_modification)
{
    int bytes;
    bzero(buffer, BUFFER_SIZE);
    bytes = receiveall(from, buffer, need_modification);
    sendbuf(to, buffer, bytes);
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno = PORT;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    clilen = sizeof(cli_addr);

    /* First call to socket() function */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
    {
        perror("ERROR opening socket");
        exit(1);
    }

    // port reusable
    int tr = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &tr, sizeof(int)) == -1)
    {
        perror("setsockopt");
        exit(1);
    }

    /* Initialize socket structure */
    bzero((char *)&serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    /* Now bind the host address using bind() call.*/
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        perror("bind error");
        exit(1);
    }

    /* listen on socket you created */
    if (listen(sockfd, 10) == -1)
    {
        perror("listen error");
        exit(1);
    }

    printf("Server is running on port %d\n", portno);

    while (1)
    {
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

        if (newsockfd == -1)
        {
            perror("accept error");
            exit(1);
        }
        forward(newsockfd);
    }

    return 0;
}

/*
 * Function:  proxyresponse
 
 * --------------------
 * proxy server response to client
 *
 * Args
 * from: server socket descriptor
 * to: client socket descriptor
 * buffer: buffer to store data
 *
 * returns: void
 */
void forward(int client_sock)
{
    struct sockaddr_in serv_addr;
    int server_sock;
    struct hostent *he;
    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);

    if (receiveall(client_sock, buffer, 0) <= 0)
    {
        printf("error while receiving");
        goto EXIT;
    }
    printf("%s\n", buffer);

    /* get destination server hostname / port / path and other needed data from proxy request ***********/
    char firstline[200];
    bzero(firstline, 200);
    char hostname[100];
    bzero(hostname, 100);
    char port[10];
    bzero(port, 10);
    char *path = "\0";

    strcpy(firstline, strtok(buffer, "\r\n"));
    char *rest = strtok(NULL, "\0");

    char *httpmethod = strtok(firstline, " "); // "CONNECT", "GET", ...
    char *absoluteuri = strtok(NULL, " ");     // "http://snu.nxclab.org:9000/index.html"
    char *httpversion = strtok(NULL, "\r");    // "HTTP/1.1"

    char *ptr1;
    char *ptr2;
    char *ptr3;
    ptr1 = absoluteuri;
    if (ptr3 = strstr(absoluteuri, "://"))
    {
        ptr1 = ptr3;
        ptr1 = ptr1 + 3;
    }

    if (ptr3 = strchr(ptr1, ':'))
    {
        memcpy(hostname, ptr1, ptr3 - ptr1);
        ptr2 = ptr3 + 1;
        if (ptr3 = strchr(ptr3, '/'))
        {
            path = ptr3;
            memcpy(port, ptr2, ptr3 - ptr2);
        }
        else
        {
            strcpy(port, ptr2);
        }
    }

    else if (ptr2 = strchr(ptr1, '/'))
    {
        path = ptr2;
        memcpy(hostname, ptr1, ptr2 - ptr1);
    }

    else
    {
        strcpy(hostname, ptr1);
    }

    // default port 80
    if (!strlen(port))
    {
        port[0] = '8';
        port[1] = '0';
    }

    // default path '/'
    if (!strlen(path))
    {
        path = "/";
    }

    int need_modification = 0;
    if (((strcmp(path, "/") == 0) || (strcmp(path, "/index.html") == 0)) && strstr(hostname, "snu.nxclab.org") && (strcmp(port, "9000") == 0))
        need_modification = 1;

    /* make new tcp socket connection with destination server *************/
    if (!(he = gethostbyname(hostname)))
    {
        printf("invalid server name\n");
        goto EXIT;
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr, he->h_addr, he->h_length);
    serv_addr.sin_port = htons(atoi(port)); // port

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("ERROR opening socket");
        goto EXIT;
    }

    if (connect(server_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        printf("connection error");
        goto EXIT;
    }

    /* send modified HTTP request *************/

    if (!sendbuf(server_sock, httpmethod, strlen(httpmethod)))
        goto EXIT;
    if (!sendbuf(server_sock, " ", 1))
        goto EXIT;
    if (!sendbuf(server_sock, path, strlen(path)))
        goto EXIT;
    if (!sendbuf(server_sock, " ", 1))
        goto EXIT;
    if (!sendbuf(server_sock, httpversion, strlen(httpversion)))
        goto EXIT;

    char *tmp1;
    char *tmp2;
    if (need_modification)
    {
        if (tmp1 = strstr(rest, "Accept-Encoding"))
        {
            if (!sendbuf(server_sock, rest, tmp1 - rest))
                goto EXIT;
            if (tmp2 = strstr(tmp1, "\n"))
            {
                if (*(tmp2 + 3) != '\0')
                    if (!sendbuf(server_sock, tmp2 + 1, strlen(tmp2)))
                        goto EXIT;
                    else
                    {
                        if (!sendbuf(server_sock, "\r\n\r\n", 4))
                            goto EXIT;
                    }
            }
        }
        else
        {
            if (!sendbuf(server_sock, rest, strlen(rest)))
                goto EXIT;
        }
    }
    else
    {
        if (!sendbuf(server_sock, rest, strlen(rest)))
            goto EXIT;
    }

    // proxy the response from server to client
    proxyresponse(server_sock, client_sock, buffer, need_modification);

EXIT:
    shutdown(server_sock, SHUT_RDWR);
    close(server_sock);
    shutdown(client_sock, SHUT_RDWR);
    close(client_sock);
    return;
}