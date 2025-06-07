/******************************************************************************
* echo_client.c                                                               *
*                                                                             *
* Description: This file contains the C source code for an echo client.  The  *
*              client connects to an arbitrary <host,port> and sends input    *
*              from stdin.                                                    *
*                                                                             *
* Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                         *
*          Wolf Richter <wolf@cs.cmu.edu>                                     *
*                                                                             *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#define ECHO_PORT 9999
#define BUF_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <server-ip> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char buf[BUF_SIZE];

    int status, sock;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    struct addrinfo *serverinfo;        // will point to the results
    hints.ai_family = AF_INET;          // IPv4
    hints.ai_socktype = SOCK_STREAM;    // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;        // fill in my IP for me

    if ((status = getaddrinfo(argv[1], argv[2], &hints, &serverinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s \n", gai_strerror(status));
        return EXIT_FAILURE;
    }

    if ((sock = socket(serverinfo->ai_family, serverinfo->ai_socktype, serverinfo->ai_protocol)) == -1) {
        fprintf(stderr, "Socket failed\n");
        return EXIT_FAILURE;
    }

    if (connect(sock, serverinfo->ai_addr, serverinfo->ai_addrlen) == -1) {
        fprintf(stderr, "Connection failed\n");
        return EXIT_FAILURE;
    }

    char msg[BUF_SIZE];
    while (1) {
        memset(msg, 0, BUF_SIZE);
        if (fgets(msg, BUF_SIZE, stdin) == NULL) break;

        int bytes_received;
        fprintf(stdout, "Sending %s\n", msg);
        send(sock, msg, strlen(msg), 0);
        if ((bytes_received = recv(sock, buf, BUF_SIZE, 0)) > 1) {
            buf[bytes_received] = '\0';
            fprintf(stdout, "Received %s\n", buf);
        }
    }


    freeaddrinfo(serverinfo);
    close(sock);
    return EXIT_SUCCESS;
}
