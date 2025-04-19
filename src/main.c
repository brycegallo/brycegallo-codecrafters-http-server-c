#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/* * * * HTTP Request
 * An HTTP request has three parts
 * 1. A request line
 * 2. Zero or more headers, each ending with a CRLF (\r\n)
 * 3. An optional response body
 * 
 * // Example Request: 
 * 	"GET /index.html HTTP/1.1\r\nHost: localhost:4221\r\nUser-Agent: curl/7.64.1\r\nAccept: * / *\r\n\r\n" 
 * 	(2 spaces added in Headers section to avoid messing with multi-line comments)
 * // Request Line
 * GET		// HTTP method
 * /index.html	// Request target
 * HTTP/1.1	// HTTP version
 * \r\n		// CRLF that marks the end of the request line
 * 
 * // Headers
 * Host: localhost:4221\r\n	// Header that specifies the server's host and port
 * User-Agent: curl/7.64.1\r\n	// Header that describes the client's user agent
 * Accept: * / *\r\n		// Header that specifies which media types the client can accept
 * \r\n				// CRLF that marks the end of the headers
 *
 * // Request body (empty in this case)
 *
 * */


/* * * * HTTP Response
 * An HTTP response has three parts:
 * 1. A status line
 * 2. Zero or more headers, each ending with a CRLF (\r\n)
 * 3. An optional response body
 *
 * // Example Response: "HTTP/1.1 200 OK\r\n\r\n"
 * // Status line
 * HTTP/1.1	// HTTP version
 * 200		// Status code
 * OK		// Optional reason phrase
 * \r\n		// CRLF that marks the end of the status line
 * 
 * // Headers (empty in this case)
 * \r\n		// CRLF that marks the end of the headers
 *
 * // Response body (empty in this case)
 *
 * */

/******************** Reponse Definitions ********************/
// Define a 200 response to to indicate that the connection succeeded
const char* response_buffer_200_OK = "HTTP/1.1 200 OK\r\n\r\n";
// Define a 404 response to to indicate that the requested resource was not found
const char* response_buffer_404_NF = "HTTP/1.1 404 Not Found\r\n\r\n";
// Define an int with value 0 to be used when we don't want to include any flags
const int no_flags = 0;

void disable_output_buffering(void) {
    // Disable output buffering
    setbuf(stdout, NULL);	
    setbuf(stderr, NULL);
}

void handle_request(char request_buffer[1024], int client_fd) {
    //char[50] HTTP_version = "HTTP/1.1 ";// 9  characters
    //char* status_200 = "200 OK";	// 6  characters
    //char* status_404 = "400 Not Found"; // 13 characters
    //char$ crlf = "\r\n";		// 4  characters
    // maybe user strncat()? could end up being more trouble than its worth
//    printf("%d", strlen(echo_target_part_1));
//    char echo_target_part_2[8] = "\r\n\r\n";
    // use sprintf() instead to build response_buffers probably

    char* request_method = strtok(request_buffer, " "); // first token will be request method
    printf("Request Method: %s\n", request_method);
    char* request_target = strtok(NULL, " ");		// second token will be request target
    printf("Request Target: %s\n", request_target);

    if (!strcmp(request_target, "/")) {
	// Send HTTP response to client, send(int socket, const void *buffer, size_t length, int flags)
	send(client_fd, response_buffer_200_OK, strlen(response_buffer_200_OK), no_flags);
    } else if (!strncmp(request_target + 1, "echo", 4)) {
	char* echo_message = request_target + 6;
	char response_buffer[1024];
	char* echo_response_template = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n\r\n%s";
	sprintf(response_buffer, echo_response_template, strlen(echo_message), echo_message);
	send(client_fd, response_buffer, strlen(response_buffer), no_flags);
    } else {
	send(client_fd, response_buffer_404_NF, strlen(response_buffer_404_NF), no_flags);
    }
}

int main() {
    disable_output_buffering();

    int server_fd, client_addr_len;
    struct sockaddr_in client_addr;

    // Creates a socket, server_fd is the socket's file descriptor
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
	printf("Socket creation failed: %s...\n", strerror(errno));
	return 1;
    }
	
    // Since the tester restarts your program quite often, setting SO_REUSEADDR
    // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
	printf("SO_REUSEADDR failed: %s \n", strerror(errno));
	return 1;
    }

    /* *
     * Socket configuration
     * sin_family: which address family to use (for struct sockaddr_in, actually must always be AF_INET)
     * sin_port: which port number to use for the server
     * sin_addr: IPv4 Address
     *
     * htons(uint16_t hostshort):
     * 		Converts a short integer from host byte order to network byte order. Returns translated short integer
     * 		Network byte order means that the most significant byte comes first
     * htonl(uint32_t hostlong):
     * 		Converts a long integer from host byte order to network byte order. Returns translated long integer
     *
     * AF_INET: using IPv4 (as opposed to IPv6)
     * INADDR_ANY: Server will listen at any interface (i.e. accept connections/packets from all interfaces) 
     */
    struct sockaddr_in serv_addr = { 
	.sin_family = AF_INET,
	.sin_port = htons(4221),
	.sin_addr = { htonl(INADDR_ANY) },
    };

    // Binds the socket created/configured above to a specified port
    if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
	printf("Bind failed: %s \n", strerror(errno));
	return 1;
    }

    int connection_backlog = 5; // number of pending client connections the server will allow to queue
    // Begin listening for incoming connections
    if (listen(server_fd, connection_backlog) != 0) {
	printf("Listen failed: %s \n", strerror(errno));
	return 1;
    }

    printf("Waiting for a client to connect...\n");
    client_addr_len = sizeof(client_addr);

    // Accept client connection, accept() will return file descriptor of the accepted socket
    int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
    printf("Client connected\n");

    // Create a request buffer to accept the request to be received by recv()
    char request_buffer[1024];

    // recv(int socket, void *buffer, size_t length, int flags), returns ssize_t of message length
    recv(client_fd, request_buffer, 1024, no_flags);

    handle_request(request_buffer, client_fd);

    // Close the server file descriptor
    close(server_fd);

    return 0;
}
