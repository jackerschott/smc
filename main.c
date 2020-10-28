#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "http.h"
#include "json-c/json.h"

#define REQSIZE 1024

int main() {
	const char *addr = "127.0.0.1";

	struct addrinfo req = {0};
	req.ai_family = AF_INET;
	req.ai_socktype = SOCK_STREAM;
	req.ai_protocol = 0;
	req.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED;

	// struct addrinfo *iaddr;
	// int err = getaddrinfo(hostname, "http", &req, &iaddr);
	// if (err) {
	// 	fprintf(stderr, "getaddrinfo: %s", gai_strerror(err));
	// 	exit(1);
	// }
	struct sockaddr_in iaddr = {0};
	iaddr.sin_family = AF_INET;
	inet_aton("127.0.0.1", &iaddr.sin_addr);
	iaddr.sin_port = htons(8008);

	int fsock = socket(AF_INET, SOCK_STREAM, 0); 
	if (connect(fsock, (struct sockaddr *)&iaddr, sizeof(struct sockaddr_in))) {
		fprintf(stderr, "Could not connect to %s: ", addr);
		perror(NULL);
		exit(1);
	}

	char request[REQSIZE];
	char rscpath[REQSIZE];
	snprintf(rscpath, REQSIZE, "/_matrix/client/versions");
	snprintf(request, REQSIZE, "GET %s HTTP/1.1\r\n" \
		"Host: %s\r\n" \
		"accept: */*\r\n\r\n", rscpath, "localhost:8008");
	//printf("%s", request);

	send(fsock, request, strlen(request), 0);

	size_t respsize = HTTP_RESP_READSIZE;
	char *resp = malloc(respsize);
	if (!resp) {
		close(fsock);
		exit(1);
	}
	if (http_recv_response(fsock, &resp, &respsize)) {
		fprintf(stderr, "main: Could not receive http response\n");
		close(fsock);
		exit(1);
	}

	struct header head;
	size_t bodysize = respsize;
	char *body = malloc(bodysize);
	if (!body) {
		free(resp);
		close(fsock);
		exit(1);
	}
	if (http_parse_response(resp, &head, body, bodysize)) {
		fprintf(stderr, "main: Could not parse http response\n");
		free(resp);
		close(fsock);
		exit(1);
	}

	size_t bodylen = strlen(body);
	char *dbody = malloc(bodylen);
	if (!dbody) {
		free(resp);
		close(fsock);
		exit(1);
	}
	dbody[0] = '\0';
	if (http_parse_chunked(body, bodylen, dbody, bodylen)) {
		fprintf(stderr, "main: Could not decode body\n");
		free(dbody);
		free(resp);
		close(fsock);
		exit(1);
	}

	//printf("--- header ---\n");
	//printf("version: %s\n", head.version);
	//printf("status: %s\n", head.status);
	//printf("fields:");
	//for (int i = 0; i < head.nfields; ++i) {
	//	printf("%s: |%s|\n", head.fields[i].name, head.fields[i].value);
	//}
	//printf("\n");

	//printf("--- body ---\n");
	//printf("%s\n\n", dbody);

	json_object *obj = json_tokener_parse(dbody);

	json_object *versions;
	json_object_object_get_ex(obj, "versions", &versions);

	for (int i = 0; i < json_object_array_length(versions); ++i) {
		json_object *version = json_object_array_get_idx(versions, i);
		printf("versions[%i] = %s\n", i, json_object_get_string(version));
	}

	json_object_put(obj);

	close(fsock);
	return 0;
}
