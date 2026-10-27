#ifndef APP_NET_HTTP_CLIENT_H_
#define APP_NET_HTTP_CLIENT_H_

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>

/* Returned by http_connect() when the server certificate fails the check. */
#define HTTP_CERT_INVALID (-EACCES)

/* Deadline for one whole http_get() request, response included. */
#define HTTP_TIMEOUT_MS 5000

/* Longest http_connect() can take: DNS lookup, then TCP connect, then the TLS handshake. */
#define HTTP_CONNECT_MAX_MS                                                                        \
	(CONFIG_NET_SOCKETS_DNS_TIMEOUT + CONFIG_NET_SOCKETS_CONNECT_TIMEOUT +                         \
	 CONFIG_NET_SOCKETS_TLS_CONNECT_TIMEOUT)

/* Opens a TLS 1.2 socket to host on port 443. */
int http_connect(const char *host, bool verify);

/* Downloads the resource at path from host over the given socket. */
int http_get(int sock, const char *host, const char *path, char *buf, size_t buf_len);

/* Closes a socket from http_connect. */
void http_close(int sock);

#endif /* APP_NET_HTTP_CLIENT_H_ */
