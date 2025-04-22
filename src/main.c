#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <zconf.h>
#include <zlib.h>

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

/********************* Reponse Definitions *********************/
const char* test_directory = "/tmp/data/codecrafters.io/http-server-tester/";

/********************* Reponse Definitions *********************/
// Define a 200 response indicating that the connection succeeded
const char* response_buffer_200_OK = "HTTP/1.1 200 OK\r\n\r\n";
// Define a 201 response indicating that the requested resource was created
const char* response_buffer_201_Created = "HTTP/1.1 201 Created\r\n\r\n";
// Define a 404 response indicating that the requested resource was not found
const char* response_buffer_404_NF = "HTTP/1.1 404 Not Found\r\n\r\n";
// Define an int with value 0 to be used when we don't want to include any flags
const int no_flags = 0;

/********************** Reponse Building **********************/
const char* http_version_1_1 = "HTTP/1.1";

const char* status_code_200  = "200";
const char* status_code_201  = "201";
const char* status_code_404  = "404";

const char* reason_phrase_OK = " OK\r\n";
const char* reason_phrase_NF = " Not Found\r\n";
const char* reason_phrase_no = " \r\n";

const char* headers_none     = "\r\n";
const char* header_content_encoding = "\r\nContent-Encoding:";
const char* header_content_encoding_gzip = "\r\nContent-Encoding: gzip";

const char* header_content_length = "\r\nContent-Length:";

const char* body_empty       = "";
/******************************  ******************************/

typedef struct buffer_struct {
    char* request_method;
    char* request_target;
    char* http_version;
    char* host;
    char* user_agent;
    char* accept_content_type;
    int*  content_length;
    char* body;
} buffer_struct;


void disable_output_buffering(void) {
    // Disable output buffering
    setbuf(stdout, NULL);	
    setbuf(stderr, NULL);
}

// gzip_compress will take: a pointer to the body we want to compress, the size of the body, and the 
static char* gzip_deflate(char* input, size_t input_length, size_t* output_length) {
    // Initialize a z_stream structure to manage the compression process
    z_stream stream = {0};
    // (stream, compression level, compression algorithm, window size 15 bit + 16 bits enabling gzip header/footer for metadata
    // compression strategy)
    deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 0x1F, 8, Z_DEFAULT_STRATEGY);

    size_t max_length = deflateBound(&stream, input_length);
    char* gzip_data = malloc(max_length);
    memset(gzip_data, 0, max_length);

    stream.next_in = (Bytef*)input;
    stream.avail_in = input_length;

    stream.next_out = (Bytef*)gzip_data;
    stream.avail_out = max_length;

    deflate(&stream, Z_FINISH);
    *output_length = stream.total_out;

    deflateEnd(&stream);

    return gzip_data;
}

//void process_request_buffer(char request_buffer[1024]) {
//void process_request_buffer(buffer_struct* request_buffer_struct, char request_buffer[1024]) {
void process_request_buffer(struct buffer_struct *request_buffer_struct, char request_buffer[1024]) {
    // Use strtok_r() for method, target, http_version, host, since these will always be present
    char* save_pointer;
    char* request_method = strtok_r(request_buffer, " ", &save_pointer); // first token will be request method
    char* request_target = strtok_r(request_buffer, " ", &save_pointer); // second token will be request target
    char* request_http_version = strtok_r(request_buffer, " ", &save_pointer); // third token will be host
    char* request_host = strtok_r(request_buffer, " ", &save_pointer); // third token will be host
    request_buffer_struct->request_method = request_method;
    request_buffer_struct->request_target = request_target;
    request_buffer_struct->http_version = request_http_version;
    request_buffer_struct->host = request_host;

    // Use regex for user-agent, accept
    regex_t regex;
    const size_t n_match = 10;
    //int regex_comp_result = regcomp(&regex, "^GET", 0);
    //int regex_comp_result = regcomp(&regex, "^GET", REG_EXTENDED | REG_ICASE);

    //int regex_comp_method_result = regcomp(&regex, "GET|POST", REG_EXTENDED | REG_ICASE);
    int regex_comp_method_result = regcomp(&regex, "GET|POST", REG_EXTENDED | REG_ICASE);
    regmatch_t p_match[n_match + 1];
    int regexec_method_result = regexec(&regex, request_buffer, n_match, p_match, 0);
    if (regexec_method_result == 0) {
	printf("LOG____PRB()____REGEX MATCH\n");
	for (size_t i = 0; p_match[i].rm_so != -1 && i < n_match; i++) {
	//int i = 0;
	char regex_buffer[256] = {0};
	    //strncpy(regex_buffer, request_buffer_pointer + p_match[i].rm_so, p_match[i].rm_eo - p_match[i].rm_so);
	    strncpy(regex_buffer, request_buffer + p_match[i].rm_so, p_match[i].rm_eo - p_match[i].rm_so);
	    printf("LOG____PRB()____REGEX start %d, end %d: %s\n", p_match[i].rm_so, p_match[i].rm_eo, regex_buffer);
	}
    }
    regfree(&regex);

    printf("LOG____PRB()____Request Buffer: %s\n", request_buffer);
    //char* request_buffer_pointer = request_buffer;
    //char* array[6];
    //char* array[4];
    //char* array = (char*)malloc(6 * sizeof(char));
    // Array Contents:
    // array[0] = request method
    // array[1] = request target
    // array[2] = HTTP Version
    // array[3] = Host
    // array[4] = User-Agent	(optional - kind of)
    // array[5] = Accept	(optional)
    
}

void handle_request(char request_buffer[1024], int client_fd) {
    char* request_buffer_duplicate = strdup(request_buffer);
    //process_request_buffer(request_buffer_duplicate);

    struct buffer_struct request_buffer_struct;
    process_request_buffer(&request_buffer_struct, request_buffer_duplicate);
    //printf("LOG____struct request method: %s\n", request_buffer_struct.request_method);


    printf("LOG____Request Buffer: %s\n", request_buffer);

    int content_encoding_active;
    char* content_encoding;
    char response_buffer[1024]; // used for "echo" and "user-agent"
    
    // %s1 = HTTP Version ____ %s2 = Status Code ____ %s3 = Reason Phrase ____ %s4 = Headers
    char* empty_response_template = "%s %s%s%s%s";
    char* response_template       = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n\r\n%s";
    char* gzip_response_template  = "HTTP/1.1 200 OK\r\nContent-Type: text/plain%sContent-Length: %zu\r\n\r\n%s";
    char* file_response_template  = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %zu\r\n\r\n%s";
    
    // Detect gzip as an accepted encoding
    if (strstr(request_buffer, "Accept-Encoding:") != NULL && strstr(request_buffer, "gzip") != NULL) {
	content_encoding_active = 1;
	content_encoding = "\r\nContent-Encoding: gzip\r\n";
	printf("LOG____Client Accepts gzip\n");
    } else {
	content_encoding_active = 0;
	printf("LOG____Client DOES NOT Accept gzip\n");
	content_encoding = "\r\n";
    }
    printf("LOG____content_encoding_active = %d\n", content_encoding_active);
    printf("LOG____content_encoding = %s\n", content_encoding);

    char* request_buffer_pointer = request_buffer;
    char* request_method = strtok_r(request_buffer_pointer, " ", &request_buffer_pointer); // first token will be request method
    //printf("LOG____Request Method: %s\n", request_method);
    printf("LOG____Request Method: %s\n", request_buffer_struct.request_method);
    char* request_target = strtok_r(request_buffer_pointer, " ", &request_buffer_pointer);		// second token will be request target
    printf("LOG____Request Target: %s\n", request_target);

    // Updated to use Empty Response Template
    if (!strcmp(request_target, "/")) {
	sprintf(response_buffer, empty_response_template, http_version_1_1, status_code_200, reason_phrase_OK, headers_none, body_empty);
	printf("LOG____New Response Buffer: %s\n", response_buffer);
	// Send HTTP response to client, send(int socket, const void *buffer, size_t length, int flags)
	send(client_fd, response_buffer, strlen(response_buffer), no_flags);
    }
    else if (!strncmp(request_target, "/echo/", 6)) {    
	char* echo_message = request_target + 6;

	// No content_encoding
	if (!content_encoding_active) {
	    sprintf(response_buffer, gzip_response_template, content_encoding, strlen(echo_message), echo_message);
	    printf("LOG____ Echo Response Buffer Without GZIP: %s\n", response_buffer);
	    send(client_fd, response_buffer, strlen(response_buffer), no_flags);
	}
	// content_encoding
	else {
	    sprintf(response_buffer, gzip_response_template, content_encoding, strlen(echo_message), echo_message);
	    printf("LOG____response_buffer With GZIP: %s\n", response_buffer);
	    send(client_fd, response_buffer, strlen(response_buffer), no_flags);
	}
    } 
    else if (!strcmp(request_target, "/user-agent")) {
       	strtok_r(request_buffer_pointer, "\r\n", &request_buffer_pointer);
       	strtok_r(request_buffer_pointer, "\r\n", &request_buffer_pointer);
	char* user_agent = strtok_r(request_buffer_pointer, "\r\n", &request_buffer_pointer) + 12;
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
	printf("LOG____Filepath: %s\n", file_path);

	if (!strcmp(request_method, "GET")) {
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
	    // i think i need this here, maybe higher actually
	    fclose(file_fd);
	}
	else if (!strcmp(request_method, "POST")) {
	    strtok_r(request_buffer_pointer, "\r\n", &request_buffer_pointer);
	    strtok_r(request_buffer_pointer, "\r\n", &request_buffer_pointer);
	    char* request_body_length = strtok_r(request_buffer_pointer, "\r\n", &request_buffer_pointer);
	    printf("LOG___Request Body Length: %s\n", request_body_length + 16);
	    strtok_r(request_buffer_pointer, "\r\n", &request_buffer_pointer);
	    char* request_body = strtok_r(request_buffer_pointer, "\r\n", &request_buffer_pointer);
	    printf("LOG___Request Body: %s\n", request_body);
	    // i'm thinking either FILE *file_fd = fopen(file_path, "r"); above is not the best name, or this below isn't, will look into it
	    FILE *file_pointer = fopen(file_path, "w");
	    fprintf(file_pointer, request_body);
	    // i think i need this here
	    fclose(file_pointer);
	    send(client_fd, response_buffer_201_Created, strlen(response_buffer_201_Created), no_flags);
	}
    }
    else {
	send(client_fd, response_buffer_404_NF, strlen(response_buffer_404_NF), no_flags);
    }
    // All members of the struct buffer_struct assigned in process_request_buffer() will be invalid after this call
    free(request_buffer_duplicate);
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
