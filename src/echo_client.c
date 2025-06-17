#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>

#define ECHO_PORT 9999
#define BUF_SIZE 8192
#define SAMPLE_DIR "samples"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <server-ip> <port> <filename>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char buf[BUF_SIZE];
    char file_path[BUF_SIZE];
    snprintf(file_path, BUF_SIZE, "%s/%s", SAMPLE_DIR, argv[3]);

    int status, sock;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    struct addrinfo *servinfo; // will point to the results
    hints.ai_family = AF_INET;  // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE; // fill in my IP for me

    if ((status = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s \n", gai_strerror(status));
        return EXIT_FAILURE;
    }

    if ((sock = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1) {
        fprintf(stderr, "Socket failed\n");
        return EXIT_FAILURE;
    }

    if (connect(sock, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        fprintf(stderr, "Connect failed\n");
        return EXIT_FAILURE;
    }

    // Open the file and read its content
    int file_fd = open(file_path, O_RDONLY);
    if (file_fd == -1) {
        perror("Failed to open file");
        close(sock);
        freeaddrinfo(servinfo);
        return EXIT_FAILURE;
    }

    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, buf, BUF_SIZE)) > 0) {
        if (send(sock, buf, bytes_read, 0) < 0) {
            perror("Send failed");
            close(file_fd);
            close(sock);
            freeaddrinfo(servinfo);
            return EXIT_FAILURE;
        }
    }

    if (bytes_read == -1) {
        perror("Read failed");
        close(file_fd);
        close(sock);
        freeaddrinfo(servinfo);
        return EXIT_FAILURE;
    }

    close(file_fd);

    fprintf(stdout, "Received:\n");
    int bytes_received;
    if ((bytes_received = recv(sock, buf, BUF_SIZE, 0)) > 0) {
        buf[bytes_received] = '\0';
        fprintf(stdout, "%s", buf);
    }

    freeaddrinfo(servinfo);
    close(sock);
    return EXIT_SUCCESS;
}