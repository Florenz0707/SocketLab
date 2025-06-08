#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>
#include "parse.h"

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
    if (sock != -1) return;
    exit(0);
}

static Request *resolve(const char *buffer, int size) {
//    enum {
//        STATE_START, STATE_CR, STATE_CRLF, STATE_CRLFCR, STATE_CRLFCRLF
//    };
//
//    char buf[8192] = {0};
//    int i = 0, offset = 0;
//    int state = STATE_START;
//
//    /* First line */
//    while (state != STATE_CRLFCRLF && i < size) {
//        char expect = 0;
//        const char next = buffer[i++];
//        buf[offset++] = next;
//        switch (state) {
//            case STATE_START:
//            case STATE_CRLF:
//                expect = '\r';
//                break;
//            case STATE_CR:
//            case STATE_CRLFCR:
//                expect = '\n';
//                break;
//            default:
//                state = STATE_START;
//                continue;
//        }
//
//        if (next == expect) state++;
//        else state = STATE_START;
//    }
//
//    if (state != STATE_CRLFCRLF) {
//        fprintf(stdout, "Not reach STATE_CRLFCRLF, current state: %d\n", state);
//        return NULL;
//    }

    Request *request = malloc(sizeof(Request));
    request->header_count = 0;
    request->headers = (Request_header *) malloc(sizeof(Request_header) * 1);

    int pre = 0, post = 0, part = 0;
    for (; post < size; ++post) {
        if (buf[post] == ' ' || buf[post] == '\r' || buf[post] == '\n') {
            if (part == 0) strncpy(request->http_method, buf + pre, post - pre);
            else if (part == 1) strncpy(request->http_uri, buf + pre, post - pre);
            else strncpy(request->http_version, buf + pre, post - pre);

            part++;
            pre = post + 1;
        }
        if (buf[post] == '\r' || buf[post] == '\n' || part >= 3) break;
    }
    if (part < 3) {
        fprintf(stdout, "Parts are not enough\n");
        return NULL;
    }
    return request;
}

int isRequestValid(Request *request) {
    /*
     * if request is good, return 0;
     * if request is not implemented, return 1;
     * if request is bad(invalid), return 2;
     */
    if (request == NULL) return 2;

    fprintf(stdout, "method:%s, uri:%s, version:%s.\n",
            request->http_method, request->http_uri, request->http_version);
    if (strlen(request->http_method) == 0 || strlen(request->http_uri) == 0 ||
        strlen(request->http_version) == 0)
        return 2;
    if (request->http_uri[0] != '/') return 2;
    if (strcmp(request->http_version, "HTTP/1.1") != 0) return 2;

    if (strcmp(request->http_method, "GET") == 0 ||
        strcmp(request->http_method, "POST") == 0 ||
        strcmp(request->http_method, "HEAD") == 0)
        return 0;

    return 1;
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
            Request *request = resolve(buf, readret);
            int status = isRequestValid(request);
            if (status == 0) {
                fprintf(stdout, "Good Request, echo back\n");
                strcpy(msg, buf);
                strcat(msg, "(echo back)\r\n");
            } else if (status == 1) {
                fprintf(stdout, "Not Implemented\n");
                strcpy(msg, "HTTP/1.1 501 Not Implemented\r\n\r\n");
            } else {
                fprintf(stdout, "Bad request\n");
                strcpy(msg, "HTTP/1.1 400 Bad request\r\n\r\n");
            }

            if (send(client_sock, msg, strlen(msg), 0) < 0) break;
            fprintf(stdout, "Send back\n");
            if (request != NULL) {
                free(request->headers);
                free(request);
            }
            /*
             * when client is closing the connection：
             * FIN of client carrys empty，so recv() return 0
             * ACK of server only carrys"(echo back)", so send() return 11
             * ACK of client carrys empty, so recv() return 0
             * Then server finishes closing the connection, recv() and send() return -1
             */
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
