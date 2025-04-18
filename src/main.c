#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int main() {
    // Disable output buffering
    setbuf(stdout, NULL);	
    setbuf(stderr, NULL);

    // You can use print statements as follows for debugging, they'll be visible when running tests.
    printf("Logs from your program will appear here!\n");

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

    /* Socket configuration
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

    // Accept client connection, accept() will return
    int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
    printf("Client connected\n");

    // Define a 200 response to to indicate that the connection succeeded
    char* response_buffer = "HTTP/1.1 200 OK\r\n\r\n";
    // Send HTTP response to client, send(int socket, const void *buffer, size_t length, int flags)
    send(client_fd, response_buffer, strlen(response_buffer), 0);

    // Close the server file descriptor
    close(server_fd);

    return 0;
}
