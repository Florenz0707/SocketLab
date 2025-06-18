#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>
#include "utils.h"
#include "parse.h"

#define BUF_SIZE 8196
#define PATH_LEN 4096

const char *http_version = "HTTP/1.1";

const char *RESPONSE_400 = "HTTP/1.1 400 Bad request\r\n\r\n";
const char *RESPONSE_404 = "HTTP/1.1 404 Not Found\r\n\r\n";
const char *RESPONSE_501 = "HTTP/1.1 501 Not Implemented\r\n\r\n";
const char *RESPONSE_505 = "HTTP/1.1 505 HTTP Version not supported\r\n\r\n";

FILE *error_log = NULL;
FILE *access_log = NULL;

void handle_request(int client_sock, char *recv_buf, size_t readret);

void init_logs() {
    error_log = fopen("error.log", "a");
    access_log = fopen("access.log", "a");
}

void log_error(const char *message) {
    if (error_log == NULL) return;

    fprintf(error_log, "%s\n", message);
    fflush(error_log);
}

void log_access(const char *request, const char *status) {
    if (access_log == NULL) return;

    time_t rawtime;
    struct tm *timeinfo;
    char buffer[80];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", timeinfo);

    fprintf(access_log, "%s - - [%s] \"%s\" %s\n", "127.0.0.1", buffer, request, status);
    fflush(access_log);
}

void response_echo(int client_sock, char *recv_buf) {
    /*
     * echo back
     */
    Send_nbytes(client_sock, recv_buf, strlen(recv_buf));
}

void response400(int client_sock) {
    /*
     * Bad Request
     */
    Send_nbytes(client_sock, RESPONSE_400, strlen(RESPONSE_400));
}

void response404(int client_sock) {
    /*
     * Not found
     */
    Send_nbytes(client_sock, RESPONSE_404, strlen(RESPONSE_404));
}

void response501(int client_sock) {
    /*
     * Not Implemented
     */
    Send_nbytes(client_sock, RESPONSE_501, strlen(RESPONSE_501));
}

void response505(int client_sock) {
    /*
     * HTTP Version not supported
     */
    Send_nbytes(client_sock, RESPONSE_505, strlen(RESPONSE_505));
}

int send_nbytes(int sock, const void *p, int nbytes) {
    size_t left_bytes = nbytes;
    ssize_t sent_bytes = 0;
    const char *ptr = p;

    while (left_bytes > 0) {
        sent_bytes = send(sock, ptr, left_bytes, 0);
        if (sent_bytes == -1) {
            perror("send failed");
            return -1;
        }
        ptr += sent_bytes;
        left_bytes -= sent_bytes;
    }
    return 0;
}

void Send_nbytes(int sock, const void *ptr, int nbytes) {
    if (send_nbytes(sock, ptr, nbytes) == 0) return;
    fprintf(stdout, "send_nbytes error\n");
}

void pipelining(int client_sock, char *recv_buf, size_t readret) {
    int cursor = 0, len = 0, request_cnt = 0;
    char buf[BUF_SIZE];
    while (cursor < readret && len < BUF_SIZE) {
        char cur = recv_buf[cursor++];
        buf[len++] = cur;
        if (len >= 4) {
            if (buf[len - 4] == '\r' && buf[len - 3] == '\n' && buf[len - 2] == '\r' && buf[len - 1] == '\n') {
                buf[len] = '\0';
                request_cnt++;
                handle_request(client_sock, buf, len + 1);
                memset(buf, 0, sizeof(buf));
                len = 0;
            }
        }
    }
    if (request_cnt == 0) {
        response400(client_sock);
        log_error("Bad Request");
    }
}

void handle_request(int client_sock, char *recv_buf, size_t readret) {
    fprintf(stdout, "Handling request...\n");
    Request *request = parse(recv_buf, readret, client_sock);
    if (request == NULL) {
        fprintf(stdout, "Request parsing failed\n");
        response400(client_sock);
        log_error("Request parsing failed");
        return;
    }

    fprintf(stdout, "Request method: %s\n", request->http_method);
    fprintf(stdout, "Request URI: %s\n", request->http_uri);

    if (strcmp(request->http_version, http_version) != 0) {
        response505(client_sock);
        log_error("Unsupported HTTP version");
    } else if (strcmp(request->http_method, "GET") == 0) {
        handle_get(client_sock, request);
    } else if (strcmp(request->http_method, "HEAD") == 0) {
        handle_head(client_sock, request);
    } else if (strcmp(request->http_method, "POST") == 0) {
        response_echo(client_sock, recv_buf);
        handle_post(client_sock, request);
    } else {
        response501(client_sock);
        log_error("Not Implemented");
    }

    free(request->headers);
    free(request);
}

void handle_get(int client_sock, Request *request) {
    char fullpath[PATH_LEN];
    snprintf(fullpath, sizeof(fullpath), "./static_site%s", request->http_uri);

    struct stat file_stat;
    if (stat(fullpath, &file_stat) == -1) {
        if (access(fullpath, F_OK) < 0) {
            response404(client_sock);
            return;
        }
    }

    if (S_ISDIR(file_stat.st_mode)) {
        strcat(fullpath, "/index.html");
        if (stat(fullpath, &file_stat) == -1) {
            response404(client_sock);
            return;
        }
    }

    int file_fd = open(fullpath, O_RDONLY);
    if (access(fullpath, F_OK) < 0) {
        response404(client_sock);
        return;
    }

    char response[4096];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Length: %ld\r\n"
             "Content-Type: %s\r\n"
             "\r\n",
             file_stat.st_size, get_mime_type(fullpath));

    Send_nbytes(client_sock, response, strlen(response));

    char buffer[BUF_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        Send_nbytes(client_sock, buffer, bytes_read);
    }

    close(file_fd);
    log_access(request->http_uri, "200");
}

void handle_head(int client_sock, Request *request) {
    char fullpath[PATH_LEN];
    snprintf(fullpath, sizeof(fullpath), "./static_site%s", request->http_uri);

    struct stat file_stat;

    if (S_ISDIR(file_stat.st_mode)) {
        strcat(fullpath, "/index.html");
        if (stat(fullpath, &file_stat) == -1) {
            response404(client_sock);
            return;
        }
    }

    if (access(fullpath, F_OK) < 0) {
        response404(client_sock);
        return;
    }

    char response[4096];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Length: %ld\r\n"
             "Content-Type: %s\r\n"
             "\r\n",
             file_stat.st_size, get_mime_type(fullpath));

    Send_nbytes(client_sock, response, strlen(response));
    log_access(request->http_uri, "200");
}

void handle_post(int client_sock, Request *request) {
    log_access(request->http_uri, "200");
}

const char *get_mime_type(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (ext == NULL)
        return "application/octet-stream";

    if (strcmp(ext, ".html") == 0)
        return "text/html";
    if (strcmp(ext, ".css") == 0)
        return "text/css";
    if (strcmp(ext, ".png") == 0)
        return "image/png";
    if (strcmp(ext, ".jpeg") == 0 || strcmp(ext, ".jpg") == 0)
        return "image/jpeg";
    if (strcmp(ext, ".gif") == 0)
        return "image/gif";

    return "application/octet-stream";
}

void init_pool(int client_fd, ClientPool *client_pool) {
    FD_ZERO(&client_pool->all_fd);
    FD_SET(client_fd, &client_pool->all_fd);
    client_pool->listen_fd = client_fd;
    memset(client_pool->client_fds, -1, sizeof(client_pool->client_fds));
}

void add2pool(int client_fd, ClientPool *client_pool) {
    for (int i = 0; i < MAX_CLIENT_NUM; ++i) {
        if (client_pool->client_fds[i] == -1) {
            client_pool->client_fds[i] = client_fd;
            FD_SET(client_fd, &client_pool->all_fd);
            break;
        }
    }
}

void remove2pool(int client_fd, ClientPool *client_pool) {
    for (int i = 0; i < MAX_CLIENT_NUM; ++i) {
        if (client_pool->client_fds[i] == client_fd) {
            client_pool->client_fds[i] = -1;
            FD_CLR(client_fd, &client_pool->all_fd);
            close(client_fd);
            break;
        }
    }
}

void handle_pool(ClientPool *client_pool) {
    char buf[BUF_SIZE];
    while (1) {
        client_pool->read_fd = client_pool->all_fd;
        int result = select(FD_SETSIZE, &client_pool->read_fd, NULL, NULL, NULL);
        if (result == -1) {
            perror("select failed");
            continue;
        }
        for (int i = 0; i < FD_SETSIZE; ++i) {
            if (!FD_ISSET(i, &client_pool->read_fd)) continue;
            if (i == client_pool->listen_fd) {
                // New Connection
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(client_pool->listen_fd, (struct sockaddr *) &client_addr, &client_len);
                if (client_fd < 0) {
                    perror("accept failed");
                    continue;
                }
                add2pool(client_fd, client_pool);
                fprintf(stdout, "New connection from %s:%d\n",
                        inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            } else {
                // handle request
                memset(buf, 0, BUF_SIZE);
                ssize_t bytes_received = recv(i, buf, BUF_SIZE, 0);
                if (bytes_received <= 0) remove2pool(i, client_pool);
                else {
                    buf[bytes_received] = '\0';
                    printf("Received from client %d: %s\n", i, buf);
                    handle_request(i, buf, bytes_received);
                }
            }
        }
    }
}