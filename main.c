#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <openssl/ssl.h>

#include "http.h"
#include "json-c/json.h"

#define REQSIZE 1024

#define GETFMT \
	"%s %s HTTP/1.1\r\n" \
	"Host: %s\r\n" \
	"accept: */*\r\n" \
	"\r\n"
#define POSTFMT \
	"%s %s HTTP/1.1\r\n" \
	"Host: %s\r\n" \
	"accept: */*\r\n" \
	"content-length: %lu\r\n" \
	"content-type: application/x-www-form-urlencoded\r\n" \
	"\r\n" \
	"%s\r\n" \
	"\r\n"

const char *hostname = "matrix.org";

SSL_CTX *ssl_ctx;
void ssl_init()
{
	SSL_load_error_strings();
	SSL_library_init();
	ssl_ctx = SSL_CTX_new(SSLv23_client_method());
}

int connect_server_ip(const char* addr, int *fd)
{
	struct sockaddr_in iaddr = {0};
	iaddr.sin_family = AF_INET;
	inet_aton(addr, &iaddr.sin_addr);
	iaddr.sin_port = htons(8008);

	*fd = socket(AF_INET, SOCK_STREAM, 0); 
	if (connect(*fd, (struct sockaddr *)&iaddr, sizeof(struct sockaddr_in))) {
		fprintf(stderr, "Could not connect to %s: ", addr);
		perror(NULL);
		return -1;
	}
	return 0;
}

int connect_server_ssl(const char *host, SSL *con)
{
	struct addrinfo req = {0};
	req.ai_family = AF_INET;
	req.ai_socktype = SOCK_STREAM;
	req.ai_protocol = 0;
	req.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED;

	struct addrinfo *iaddr;
	int err = getaddrinfo(host, "https", &req, &iaddr);
	if (err) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
		return -1;
	}

	int fd = socket(AF_INET, SOCK_STREAM, 0); 
	for (; iaddr->ai_next; iaddr = iaddr->ai_next) {
		err = connect(fd, iaddr->ai_addr, sizeof(struct sockaddr_in));
		if (!err)
			break;
	}
	if (err) {
		close(fd);
		fprintf(stderr, "Could not connect to %s: ", host);
		perror(NULL);
		return -1;
	}

	err = SSL_set_fd(con, fd);
	if (err != 1) {
		fprintf(stderr, "SSL_set_fd\n");
		return -1;
	}

	err = SSL_connect(con);
	if (err != 1) {
		fprintf(stderr, "SSL_connect\n");
		SSL_free(con);
		SSL_CTX_free(ssl_ctx);
		close(fd);
		return -1;
	}

	return 0;
}

int request_ssl(SSL *con, const char* type, const char *target, const char *body, const char *host)
{

	struct reqheader head;
	strcpy(head.version, "1.1");
	strcpy(head.type, type);
	strcpy(head.target, target);

	head.nfields = 4;
	strcpy(head.fields[0].name, "Host");
	strcpy(head.fields[0].value, host);
	strcpy(head.fields[1].name, "accept");
	strcpy(head.fields[1].value, "*/*");
	strcpy(head.fields[2].name, "content-length");
	sprintf(head.fields[2].value, "%lu", strlen(body));
	strcpy(head.fields[3].name, "content-type");
	strcpy(head.fields[3].value, "application/x-www-form-urlencoded");

	char req[REQSIZE];
	http_create_request(&head, body, req, REQSIZE);

	printf("%s\n", req);
	if (!SSL_write(con, req, strlen(req))) {
		fprintf(stderr, "SSL_write");
		return -1;
	}
	return 0;
}

void disconnect_server_ssl(SSL *con)
{
	int fd = SSL_get_fd(con);
	SSL_shutdown(con);
	SSL_free(con);
	SSL_CTX_free(ssl_ctx);
	close(fd);
}

void getpasswd(char *pw, size_t pwsize)
{
	char *const argv[] = { "pass", "matrix/account", NULL };

	int pin[2];
	pipe(pin);

	pid_t pid = fork();
	if (pid == 0) {
		close(pin[0]);
		int fout = pin[1];

		dup2(fout, STDOUT_FILENO);

		execvp("pass", argv);
	} else {
		close(pin[1]);
		int fin = pin[0];

		size_t readlen = read(fin, pw, pwsize);
		if (readlen == -1) {
			fprintf(stderr, "read: ");
			perror(NULL);
			exit(-1);
		}
		close(fin);

		char *e = strchr(pw, '\n');
		*e = '\0';
	}
}

int matrix_api_call(SSL *con, const char *type, const char *target, const char *data, char *resp, size_t respsize)
{
	request_ssl(con, type, target, data, hostname);

	/* recv http response */
	size_t fullrespsize = HTTP_RESP_READSIZE;
	char *fullresp = malloc(respsize);
	if (!fullresp)
		return -1;
	if (http_recv_response_ssl(con, &fullresp, &fullrespsize)) {
		fprintf(stderr, "main: Could not receive http response\n");
		return -1;
	}

	/* parse http header */
	struct respheader head;
	size_t rawbodysize = fullrespsize;
	char *rawbody = malloc(rawbodysize);
	if (!rawbody) {
		free(fullresp);
		return -1;
	}
	if (http_parse_response(fullresp, &head, rawbody, rawbodysize)) {
		fprintf(stderr, "main: Could not parse http response\n");
		free(rawbody);
		free(fullresp);
		return -1;
	}
	free(fullresp);

	/* decode http body */
	if (http_parse_chunked(rawbody, strlen(rawbody), resp, respsize)) {
		fprintf(stderr, "main: Could not decode body\n");
		free(rawbody);
		return -1;
	}
	free(rawbody);

	/* testing output */
	//printf("--- header ---\n");
	//printf("version: %s\n", head.version);
	//printf("status: %s\n", head.status);
	//printf("fields:\n");
	//for (int i = 0; i < head.nfields; ++i) {
	//	printf("%s: |%s|\n", head.fields[i].name, head.fields[i].value);
	//}
	//printf("\n");

	//printf("--- body ---\n");
	//printf("%s\n\n", resp);
	return 0;
}

int main(int argc, char *argv[])
{
	const char *username = "lordvile";

	size_t passwordsize = 64;
	char password[passwordsize];
	getpasswd(password, passwordsize);

	const char *type = "POST";
	const char *target = "/_matrix/client/r0/login";
	const char *bodyfmt = "{\"type\":\"m.login.password\", \"user\":\"%s\", \"password\":\"%s\"}";

	size_t bodysize = 1024;
	char body[bodysize];
	snprintf(body, bodysize, bodyfmt, username, password);

	ssl_init();

	SSL *con = SSL_new(ssl_ctx);
	if (connect_server_ssl(hostname, con))
		return -1;

	size_t respstrsize = HTTP_RESP_READSIZE;
	char *respstr = malloc(respstrsize);
	if (!respstr) {
		disconnect_server_ssl(con);
		return -1;
	}
	if (matrix_api_call(con, type, target, body, respstr, respstrsize)) {
		fprintf(stderr, "matrix api call failed\n");
		free(respstr);
		disconnect_server_ssl(con);
		return -1;
	}

	/* parse response as json */
	json_object *resp = json_tokener_parse(respstr);
	free(respstr);

	printf("--- resp ---\n%s\n", json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PRETTY));

	//json_object *versions;
	//json_object_object_get_ex(obj, "versions", &versions);

	//for (int i = 0; i < json_object_array_length(versions); ++i) {
	//	json_object *version = json_object_array_get_idx(versions, i);
	//	printf("versions[%i] = %s\n", i, json_object_get_string(version));
	//}

	json_object_put(resp);
	disconnect_server_ssl(con);
	return 0;
}
