#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "parse.h"

extern const char *http_version;

extern const char *RESPONSE_400;
extern const char *RESPONSE_404;
extern const char *RESPONSE_501;
extern const char *RESPONSE_505;

extern FILE *error_log;
extern FILE *access_log;

void init_logs();
void log_error(const char *message);
void log_access(const char *request, const char *status);

void response_echo(int client_sock, char *recv_buf);
void response400(int client_sock);
void response404(int client_sock);
void response501(int client_sock);
void response505(int client_sock);

int send_nbytes(int sock, const void *p, int nbytes);
void Send_nbytes(int sock, const void *ptr, int nbytes);
void pipelining(int client_sock, char *recv_buf, size_t readret);
void handle_request(int client_sock, char *recv_buf, size_t readret);
void handle_get(int client_sock, Request *request);
void handle_head(int client_sock, Request *request);
void handle_post(int client_sock, Request *request);

const char *get_mime_type(const char *filename);

#endif
