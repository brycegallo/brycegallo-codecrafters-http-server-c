#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
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
const char* test_directory = "/tmp/data/codecrafters.io/http-server-tester/";

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
    //char* HTTP_version = "HTTP/1.1 ";// 9  characters
    //char* status_200 = "200 OK";	// 6  characters
    //char* status_404 = "400 Not Found"; // 13 characters
    //char$ crlf = "\r\n";		// 4  characters
    // use sprintf() instead to build response_buffers probably
    char response_buffer[1024]; // used for "echo" and "user-agent"
    char* response_template = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n\r\n%s";
    char* file_response_template = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %zu\r\n\r\n%s";
    char* request_method = strtok(request_buffer, " "); // first token will be request method
    printf("LOG____Request Method: %s\n", request_method);
    char* request_target = strtok(NULL, " ");		// second token will be request target
    printf("LOG____Request Target: %s\n", request_target);

    if (!strcmp(request_target, "/")) {
	// Send HTTP response to client, send(int socket, const void *buffer, size_t length, int flags)
	send(client_fd, response_buffer_200_OK, strlen(response_buffer_200_OK), no_flags);
    } else if (!strncmp(request_target, "/echo/", 6)) {
	char* echo_message = request_target + 6;
	sprintf(response_buffer, response_template, strlen(echo_message), echo_message);
	send(client_fd, response_buffer, strlen(response_buffer), no_flags);
    } 
    else if (!strcmp(request_target, "/user-agent")) {
       	strtok(NULL, "\r\n");
       	strtok(NULL, "\r\n");
	char* user_agent = strtok(NULL, "\r\n") + 12;
	sprintf(response_buffer, response_template, strlen(user_agent), user_agent);
	send(client_fd, response_buffer, strlen(response_buffer), no_flags);
    }
    else if (!strncmp(request_target, "/files/", 7)) {
	char* file_name = request_target + 7;
	char file_path[1024];
	char* file_path_template = "/tmp/data/codecrafters.io/http-server-tester/%s";
	sprintf(file_path, file_path_template, file_name);
	printf("LOG____%s\n", file_name);
	printf("LOG____Test Directory: %s\n", test_directory);
	printf("LOG____Filepath: %s", file_path);

	// create file descriptor for requested file
	FILE *file_fd = fopen(file_path, "r");

	if (file_fd == NULL) {
	    send(client_fd, response_buffer_404_NF, strlen(response_buffer_404_NF), no_flags);
	}
	else {
	    char* file_read_buffer[1024] = {};
	    int file_read_syze_bites = fread(file_read_buffer, 1, 1024, file_fd);

	    if (file_read_syze_bites > 0) {
		sprintf(response_buffer, file_response_template, file_read_syze_bites, file_read_buffer);
		send(client_fd, response_buffer, strlen(response_buffer), no_flags);
	    }
	    else {
		send(client_fd, response_buffer_404_NF, strlen(response_buffer_404_NF), no_flags);
	    }
	}
    }
    else {
	send(client_fd, response_buffer_404_NF, strlen(response_buffer_404_NF), no_flags);
    }
}

// must take void* as an argument and return void*, to be made into a thread
void* handle_client(void* arg_client_fd) {
    int client_fd = *((int*)arg_client_fd);

    // Create a request buffer to accept the request to be received by recv()
    char request_buffer[1024];

    // recv(int socket, void *buffer, size_t length, int flags), returns ssize_t of message length
    recv(client_fd, request_buffer, 1024, no_flags);

    handle_request(request_buffer, client_fd);
}

int main(int argc, char **argv) {
//int main() {
    disable_output_buffering();
    
    int server_fd;

    // Creates a socket, server_fd is the socket's file descriptor
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
	printf("LOG____Socket creation failed: %s...\n", strerror(errno));
	return 1;
    }
	
    // Tester restarts the program quite often, setting SO_REUSEADDR ensures we don't get 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
	printf("LOG____SO_REUSEADDR failed: %s \n", strerror(errno));
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
	printf("LOG____Bind failed: %s \n", strerror(errno));
	return 1;
    }

    int connection_backlog = 5; // number of pending client connections the server will allow to queue
    // Begin listening for incoming connections
    if (listen(server_fd, connection_backlog) != 0) {
	printf("LOG____Listen failed: %s \n", strerror(errno));
	return 1;
    }

    printf("LOG____Waiting for a client to connect...\n");

    // (1) malloc() here
    int* client_fd = malloc(sizeof(int));
    
    while (1) {
	struct sockaddr_in client_addr;
	int client_addr_len = sizeof(client_addr);

	// Accept client connection, accept() will return file descriptor of the accepted socket
	*client_fd = accept(server_fd, (struct sockaddr*) &client_addr, &client_addr_len);
	// // theoretically, client_fd might be variable and could potentially require realloc(), but probably not
	printf("LOG____Client connected\n");

	// Create a thread for handle_client()
	pthread_t thread_id;
	pthread_create(&thread_id, NULL, handle_client, (void*)client_fd);
	//pthread_create(&thread_id, NULL, handle_client, (void*)client_fd, (void*)directory_path);
	pthread_detach(thread_id);

    }
    // (1) free() here
    if (client_fd) { free(client_fd); }
    
    // Close the server file descriptor
    close(server_fd);

    return 0;
}
