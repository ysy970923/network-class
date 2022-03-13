#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <string.h>

#define PORT 8080

char *ROOT;
void sendContentTypeHeader(int sock, char *file_path);
void respond(int sock);

void sendall(int sock, char *msg)
{
    int length = strlen(msg);
    int bytes;
    while (length > 0)
    {
        bytes = send(sock, msg, length, 0);
        length = length - bytes;
    }
}

int main(int argc, char *argv[])
{
    int newsockfd;
    int sockfd, portno = PORT;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    clilen = sizeof(cli_addr);
    ROOT = getenv("PWD");

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

    // Initialize socket structure
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    // bind host address using bind()
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        perror("bind error");
        exit(1);
    }

    // listen on socket, backlog queue max size is set to 10
    if (listen(sockfd, 10) == -1)
    {
        perror("listen error");
        exit(1);
    }

    printf("Server is running on port %d\n", portno);

    // loop for continous requests
    while (1)
    {
        // waits till new connection
        // accept new connection and get socket descriptor
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd == -1)
        {
            perror("accept error");
            exit(1);
        }

        // process connection and send response with respond function
        respond(newsockfd);
    }

    return 0;
}

/*
 * Function:  respond 
 * --------------------
 * process request and respond
 * can currently handle only GET method with files
 *
 * Args
 * sock: integer matching with the socket descriptor
 *
 * returns: void
 */
void respond(int sock)
{
    int offset, bytes;
    char buffer[9000];
    bzero(buffer, 9000);
    FILE *fp;

    char *method_name;
    char *file_path;
    char *http_version;

    long int length;

    // recv sequentially upto 1500 bytes from socket
    // copy recevied bytes in buffer
    // end loop if recv returns 0, < 0 or "\r\n\r\n"
    offset = 0;
    bytes = 0;
    do
    {
        bytes = recv(sock, buffer + offset, 1500, 0);
        offset += bytes;

        if (strncmp(buffer + offset - 4, "\r\n\r\n", 4) == 0)
            break;
    } while (bytes > 0);

    // if recv returns < 0, it means error occurred in recv()
    if (bytes < 0)
    {
        printf("recv() error\n");
        goto EXIT;
    }

    // if recv returns 0, it means empty buffer and unexpected disconnection
    else if (bytes == 0)
    {
        printf("Client disconnected unexpectedly\n");
        goto EXIT;
    }

    // printf("%s", buffer);

    // get needed information from request
    // method name (GET, POST, ...)
    method_name = strtok(buffer, " ");
    // file path (index.html, /images/image1.jpg)
    file_path = strtok(NULL, " ") + 1; // + 1 to remove '/' in start
    // HTTP version (HTTP/1.0, HTTP/1.1, HTTP/2.0)
    http_version = strtok(NULL, "\r");

    // HTTP version check (only allow HTTP/1.1, HTTP/1.0)
    if (strcmp(http_version, "HTTP/1.1") && strcmp(http_version, "HTTP/1.0"))
    {
        sendall(sock, "HTTP/1.1 505 HTTP Version Not Supported;\r\n\r\n");
        goto EXIT;
    }

    // GET
    if (!strcmp(method_name, "GET"))
    {
        // when file path is ""(empty) open index.html
        if (!strlen(file_path))
            fp = fopen("index.html", "r");
        // else open requested file
        else
            fp = fopen(file_path, "r");

        // File Not Found, send 404 ERROR
        if (!fp)
        {
            // http version is matched with request
            sendall(sock, http_version);
            sendall(sock, " 404 Not Found;\r\n\r\n");
            goto EXIT;
        }

        // start sending header for valid response
        // http version is matched with request
        sendall(sock, http_version);
        sendall(sock, " 200 OK\r\n");

        // Check for requested file type and send Content-type header matching file type
        sendContentTypeHeader(sock, file_path);
        sendall(sock, "\r\n\r\n");

        // get file bytelength
        fseek(fp, 0L, SEEK_END);
        length = ftell(fp);
        rewind(fp);

        // while file byte length is left, send bytes
        while (length > 0)
        {
            // read file using buffer, then send bytes
            bytes = fread(buffer, sizeof(char), sizeof(buffer), fp);
            send(sock, buffer, bytes, 0);
            length -= bytes;
        }

        // tell end of response
        sendall(sock, "\r\n\r\n");
    }

    // POST, ... (other methods than GET is currently not Allowed)
    else
    {
        // http version is matched with request
        sendall(sock, http_version);
        sendall(sock, " 405 Method Not Allowed;\r\n\r\n");
    }

// shutdown and close socket
EXIT:
    // printf("close\n");
    shutdown(sock, SHUT_RDWR);
    close(sock);
}

/*
 * Function:  sendContentTypeHeader 
 * --------------------
 * get content-type from filename and send Content-Type Header.
 * can currently handle only jpg, PNG, png, svg, mp4, css, js, ico, html
 *
 * Args
 * sock: integer matching with the socket descriptor
 * file_path: file_path string for checking file type
 * returns: void
 */
void sendContentTypeHeader(int sock, char *file_path)
{
    // Check for requested file type and send Content-type header matching file type
    char *ptr = file_path + strlen(file_path);
    if (strncmp(ptr - 4, ".jpg", 4) == 0)
        sendall(sock, "Content-Type: image/jpg;");
    else if (strncmp(ptr - 4, ".PNG", 4) == 0 || strncmp(ptr - 4, ".png", 4) == 0)
        sendall(sock, "Content-Type: image/jpg;");
    else if (strncmp(ptr - 4, ".svg", 4) == 0)
        sendall(sock, "Content-Type: image/svg+xml;");
    else if (strncmp(ptr - 4, ".mp4", 4) == 0)
        sendall(sock, "Content-Type: video/mp4;");
    else if (strncmp(ptr - 4, ".css", 4) == 0)
        sendall(sock, "Content-Type: text/css;");
    else if (strncmp(ptr - 3, ".js", 3) == 0)
        sendall(sock, "Content-Type: text/javascript;");
    else if (strncmp(ptr - 3, ".ico", 4) == 0)
        sendall(sock, "Content-Type: image/x-icon;");
    else
        sendall(sock, "Content-Type: text/html;");
}