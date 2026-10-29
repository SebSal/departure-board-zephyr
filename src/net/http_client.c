#include "http_client.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(app_http_client, CONFIG_APP_HTTP_CLIENT_LOG_LEVEL);

/* ---------------------------------------- Module state ---------------------------------------- */

#define HTTPS_PORT    "443"
#define RECV_BUF_SIZE 2048

static char recv_buf[RECV_BUF_SIZE];

/* ---------------------------------------- Certificates ---------------------------------------- */

/* Both Let's Encrypt roots. X1 (RSA, until 2035) covers the older chains, YE (ECDSA, until 2045)
 * the current ones without the cross-signatures. A credential tag holds one CA certificate, so
 * each root gets its own.
 */
enum ca_tag {
	CA_TAG_ISRG_ROOT_X1 = 1,
	CA_TAG_ISRG_ROOT_YE,
};

static const unsigned char isrg_root_x1[] = {
#include "isrg_root_x1.der.inc"
};

static const unsigned char isrg_root_ye[] = {
#include "isrg_root_ye.der.inc"
};

static const struct {
	sec_tag_t tag;
	const unsigned char *cert;
	size_t size;
	const char *name;
} ca_roots[] = {
	{CA_TAG_ISRG_ROOT_X1, isrg_root_x1, sizeof(isrg_root_x1), "ISRG Root X1"},
	{CA_TAG_ISRG_ROOT_YE, isrg_root_ye, sizeof(isrg_root_ye), "ISRG Root YE"},
};

static const sec_tag_t ca_tags[] = {
	CA_TAG_ISRG_ROOT_X1,
	CA_TAG_ISRG_ROOT_YE,
};

/* Registers the roots. The store keeps pointers, the arrays stay in flash. A root left over from
 * an earlier connect reports -EEXIST, which counts as registered.
 */
static int ca_register(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(ca_roots); i++) {
		int ret = tls_credential_add(ca_roots[i].tag, TLS_CREDENTIAL_CA_CERTIFICATE,
									 ca_roots[i].cert, ca_roots[i].size);

		if (ret < 0 && ret != -EEXIST) {
			LOG_ERR("Failed to add %s: %d", ca_roots[i].name, ret);
			return ret;
		}
	}

	return 0;
}

/* Sets up the peer check on a fresh socket. The check runs in optional mode and is evaluated by
 * cert_check() once connected, because a failed handshake resets the session and with it the
 * verify result, which would leave a certificate failure looking like any other TLS error.
 * TODO: Validity dates are not checked.
 */
static int cert_setup(int sock, bool verify)
{
	int mode = verify ? TLS_PEER_VERIFY_OPTIONAL : TLS_PEER_VERIFY_NONE;

	if (verify) {
		int nocopy = TLS_CERT_NOCOPY_OPTIONAL;
		int ret = ca_register();

		if (ret < 0) {
			return ret;
		}

		if (zsock_setsockopt(sock, SOL_TLS, TLS_SEC_TAG_LIST, ca_tags, sizeof(ca_tags)) < 0) {
			ret = -errno;
			LOG_ERR("Failed to set TLS sec tag list (%d)", ret);
			return ret;
		}

		/* Parse the roots in place instead of copying them onto the mbedTLS heap. */
		if (zsock_setsockopt(sock, SOL_TLS, TLS_CERT_NOCOPY, &nocopy, sizeof(nocopy)) < 0) {
			ret = -errno;
			LOG_ERR("Failed to set TLS cert nocopy option (%d)", ret);
			return ret;
		}
	}

	if (zsock_setsockopt(sock, SOL_TLS, TLS_PEER_VERIFY, &mode, sizeof(mode)) < 0) {
		int ret = -errno;

		LOG_ERR("Failed to set TLS peer verify option (%d)", ret);
		return ret;
	}

	return 0;
}

/* Fails the connection if the handshake did not verify the server certificate. */
static int cert_check(int sock, const char *host)
{
	uint32_t result;
	net_socklen_t len = sizeof(result);

	if (zsock_getsockopt(sock, SOL_TLS, TLS_CERT_VERIFY_RESULT, &result, &len) < 0) {
		int ret = -errno;

		LOG_ERR("Failed to read TLS verify result (%d)", ret);
		return ret;
	}

	if (result != 0) {
		LOG_ERR("Certificate check failed for %s: 0x%x", host, result);
		return HTTP_CERT_INVALID;
	}

	return 0;
}

/* ---------------------------------------- Response body --------------------------------------- */

/* Where one response body is assembled. The http client hands the body over in
 * recv_buf sized fragments, so anything longer than RECV_BUF_SIZE arrives across
 * several callbacks and has to be appended rather than overwritten.
 */
struct body_sink {
	char *buf;
	size_t size;
	size_t used;
	bool truncated;
	uint16_t status;
};

static void sink_append(struct body_sink *sink, const char *data, size_t len)
{
	if (len == 0) {
		return;
	}

	size_t space = sink->size - sink->used - 1;
	size_t copy_len = MIN(len, space);

	if (copy_len < len) {
		sink->truncated = true;
	}
	memcpy(sink->buf + sink->used, data, copy_len);
	sink->used += copy_len;
	sink->buf[sink->used] = '\0';
}

/* Runs once per body fragment, so a receive holding several chunks of a chunked response calls
 * it once per chunk, each with only that chunk's payload. A receive without body has no fragment.
 */
static int response_cb(struct http_response *rsp, enum http_final_call final_data, void *user_data)
{
	struct body_sink *sink = user_data;

	/* Overwritten with the same value on every call. */
	sink->status = rsp->http_status_code;

	if (rsp->body_frag_start) {
		sink_append(sink, (const char *)rsp->body_frag_start, rsp->body_frag_len);
	}

	if (final_data == HTTP_DATA_FINAL) {
		/* content_length is 0 on a chunked response, so the assembled size is left to
		 * http_get, which knows what actually landed in the buffer.
		 */
		LOG_INF("HTTP %s", rsp->http_status);
	}

	return 0;
}

/* ---------------------------------------- Requests -------------------------------------------- */

int http_connect(const char *host, bool verify)
{
	struct zsock_addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	struct zsock_addrinfo *res;

	int ret = zsock_getaddrinfo(host, HTTPS_PORT, &hints, &res);

	if (ret != 0) {
		LOG_ERR("DNS lookup failed for %s: %d", host, ret);

		return ret < 0 ? ret : -EIO;
	}

	char ip_str[NET_IPV4_ADDR_LEN];

	zsock_inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr, ip_str,
					sizeof(ip_str));
	LOG_INF("Connecting to %s (%s)", host, ip_str);

	int sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TLS_1_2);

	if (sock < 0) {
		ret = -errno;
		LOG_ERR("Socket create failed: %d", ret);
		zsock_freeaddrinfo(res);
		return ret;
	}

	ret = cert_setup(sock, verify);
	if (ret < 0) {
		goto close_sock;
	}

	if (zsock_setsockopt(sock, SOL_TLS, TLS_HOSTNAME, host, strlen(host)) < 0) {
		ret = -errno;
		LOG_ERR("Failed to set hostname option (%d)", ret);
		goto close_sock;
	}

	if (zsock_connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
		ret = -errno;
		LOG_ERR("Connect failed: %d", ret);
		goto close_sock;
	}

	if (verify) {
		ret = cert_check(sock, host);
		if (ret < 0) {
			goto close_sock;
		}
	}

	zsock_freeaddrinfo(res);
	return sock;

close_sock:
	zsock_close(sock);
	zsock_freeaddrinfo(res);
	return ret;
}

int http_get(int sock, const char *host, const char *path, char *buf, size_t buf_len)
{
	buf[0] = '\0';

	struct body_sink sink = {
		.buf = buf,
		.size = buf_len,
	};

	struct http_request req = {
		.method = HTTP_GET,
		.url = path,
		.host = host,
		.protocol = "HTTP/1.1",
		.response = response_cb,
		.recv_buf = recv_buf,
		.recv_buf_len = sizeof(recv_buf),
	};

	int ret = http_client_req(sock, &req, HTTP_TIMEOUT_MS, &sink);

	if (ret < 0) {
		LOG_ERR("HTTP GET failed: %d", ret);
		return ret;
	}

	/* Handle HTTP errors. */
	if (sink.status < 200 || sink.status > 299) {
		LOG_ERR("HTTP request failed with status %u", (unsigned int)sink.status);
		return -EIO;
	}

	/* Buffer is too small. */
	if (sink.truncated) {
		LOG_ERR("Response body does not fit the %u byte buffer", (unsigned int)buf_len);
		return -ENOMEM;
	}

	LOG_INF("HTTP GET successful, %u bytes stored in user buffer", (unsigned int)sink.used);
	return 0;
}

void http_close(int sock)
{
	zsock_close(sock);
}
