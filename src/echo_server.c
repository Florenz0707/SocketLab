#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>
#include <parse.h>

#define ECHO_PORT 9999
#define BUF_SIZE 4096

int sock = -1, client_sock = -1;
char buf[BUF_SIZE];
char msg[BUF_SIZE];

int close_socket(int sock) {
    if (close(sock)) {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

void handle_signal(const int sig) {
    if (sock != -1) {
        fprintf(stderr, "\nReceived signal %d. Closing socket.\n", sig);
        close_socket(sock);
    }
    exit(0);
}

void handle_sigpipe(const int sig) {
    if (sock != -1) {
        return;
    }
    exit(0);
}

int isBadRequest(Request *request) {
    if (strlen(request->http_method) == 0 || strlen(request->http_uri) == 0 ||
        strlen(request->http_version) == 0)
        return 1;
    char http[6];
    strncpy(http, request->http_version, 5);
    http[5] = '\0';
    if (strcmp(http, "HTTP/") != 0) return 1;
    return 0;
}

Request *analyze(const char *buffer, const int size) {
    Request *request = malloc(sizeof(Request));
    int cursor = 0, len = 0, cnt = 0;
    char parts[5][5000];
    while (cursor < size) {
        while (buffer[cursor + len] != ' ' &&
               buffer[cursor + len] != '\n' && buffer[cursor + len] != '\r')
            len++;
        strncpy(parts[cnt], buffer + cursor, len);
        parts[cnt][len] = '\0';
        cnt++;
        if (buffer[cursor + len] == '\n' || buffer[cursor + len] == '\r') break;
        cursor = cursor + len + 1;
        len = 0;
    }
    strcpy(request->http_method, parts[0]);
    strcpy(request->http_uri, parts[1]);
    strcpy(request->http_version, parts[2]);
    return request;
}

int main(int argc, char *argv[]) {
    /* register signal handler */
    /* process termination signals */
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    signal(SIGSEGV, handle_signal);
    signal(SIGABRT, handle_signal);
    signal(SIGQUIT, handle_signal);
    signal(SIGTSTP, handle_signal);
    signal(SIGFPE, handle_signal);
    signal(SIGHUP, handle_signal);
    /* normal I/O event */
    signal(SIGPIPE, handle_sigpipe);
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    fprintf(stdout, "----- Echo Server -----\n");

    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }
    /* set socket SO_REUSEADDR | SO_REUSEPORT */
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        fprintf(stderr, "Failed setting socket options.\n");
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET; // ipv4
    addr.sin_port = htons(ECHO_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr))) {
        close_socket(sock);
        fprintf(stderr, "Failed binding socket.\n");
        return EXIT_FAILURE;
    }

    if (listen(sock, 5)) {
        close_socket(sock);
        fprintf(stderr, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }

    /* finally, loop waiting for input and then write it back */

    while (1) {
        /* listen for new connection */
        cli_size = sizeof(cli_addr);
        fprintf(stdout, "Waiting for connection...\n");
        client_sock = accept(sock, (struct sockaddr *) &cli_addr, &cli_size);
        if (client_sock == -1) {
            fprintf(stderr, "Error accepting connection.\n");
            close_socket(sock);
            return EXIT_FAILURE;
        }
        fprintf(stdout, "New connection from %s:%d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
        while (1) {
            /* receive msg from client, and concatenate msg with "(echo back)" to send back */
            memset(buf, 0, BUF_SIZE);
            memset(msg, 0, BUF_SIZE);
            int readret = recv(client_sock, buf, BUF_SIZE, 0);
            if (readret < 0) break;
            fprintf(stdout, "Received (total %d bytes):%s \n", readret, buf);
            /* parse request */
            Request *request = analyze(buf, readret);

            fprintf(stdout, "version: %s\nmethod: %s\nuri: %s\n",
                    request->http_version, request->http_method, request->http_uri);

            if (isBadRequest(request)) {
                fprintf(stdout, "Bad request\n");
                strcpy(msg, "HTTP/1.1 400 Bad request\r\n\r\n");
            } else if (strcmp(request->http_method, "GET") == 0 ||
                       strcmp(request->http_method, "POST") == 0 ||
                       strcmp(request->http_method, "HEAD") == 0) {
                fprintf(stdout, "Good Request, echo back\n");
                strcpy(msg, buf);
                strcat(msg, "(echo back)");
            } else {
                fprintf(stdout, "Not Implemented\n");
                strcpy(msg, "HTTP/1.1 501 Not Implemented\r\n\r\n");
            }

            if (send(client_sock, msg, strlen(msg), 0) < 0) break;
            fprintf(stdout, "Send back\n");
            /* when client is closing the connection：
                FIN of client carrys empty，so recv() return 0
                ACK of server only carrys"(echo back)", so send() return 11
                ACK of client carrys empty, so recv() return 0
                Then server finishes closing the connection, recv() and send() return -1 */
        }
        /* client closes the connection. server free resources and listen again */
        if (close_socket(client_sock)) {
            close_socket(sock);
            fprintf(stderr, "Error closing client socket.\n");
            return EXIT_FAILURE;
        }
        fprintf(stdout, "Closed connection from %s:%d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
    }
    close_socket(sock);
    return EXIT_SUCCESS;
}
