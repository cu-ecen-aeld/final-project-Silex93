/****************************************************************************
 *  Author:     Daniel Mendez
 *  Course:     ECEN 5823
 *  Project:    Final Project
 *
 ****************************************************************************/

/**
 * @file        webserver.c
 * @brief       Source file for socket server
 *
 * @details
 *
 * @sources     - Beej Guide to Network Programming :https://beej.us/guide/bgnet/html/ Leveraged code from 6.1 A simple Stream Server with modifications
 *              - Linux System Programming : Chapter 10 Signals Page 342
 *              -https://dev.to/jeffreythecoder/how-i-built-a-simple-http-server-from-scratch-using-c-739
 *

 *
 * @date        1 Dec 2023
 * @version     2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <regex.h>
#include "queue.h"


#define USE_AESD_CHAR_DEVICE 0

#define PORT "80"
#define FLASK_PORT 5500
#define BUFFER_SIZE 10485760 //100 MB buffer for requests
#define BACKLOG 10   // how many pending connections queue will hold

#if USE_AESD_CHAR_DEVICE
#define OUTPUT_FILE "/dev/aesdchar"
#else
#define OUTPUT_FILE "/var/tmp/aesdsocketdata"
#endif

#define ERROR_RESULT (-1)


int server_socket_fd;
regex_t http_get_regex;
struct sockaddr_in flask_addr;
	   
//Mappings for MIME types
typedef struct {
    const char *extension;
    const char *mime_type;
} MimeMapping;

// Define an ar

MimeMapping mimeMappings[] = {
    { "html", "text/html" },
    { "htm", "text/html" },
    { "txt", "text/plain" },
    { "jpg", "image/jpeg" },
    { "jpeg", "image/jpeg" },
    { "png", "image/png" },
    { NULL, "application/octet-stream" } // Default MIME type
};


typedef struct thread_data {

    int thread_num;
    int client_socket;
    bool thread_complete_success;
} thread_data_t;


typedef struct thread_node {
    pthread_t thread_id;
    thread_data_t *thread_params;
    SLIST_ENTRY(thread_node) entries;
} thread_node_t;

SLIST_HEAD(threadList, thread_node) threadListHead = SLIST_HEAD_INITIALIZER(threadListHead);

        static void signal_handler(int signo) {
    
    syslog(LOG_DEBUG, "Caught signal in sign handler %d", signo);
 
    //close the server socket
    close(server_socket_fd);
    printf("Signal Recieved %d \r\n", signo);
    syslog(LOG_DEBUG, "closed socket and exiting now");
    exit(EXIT_SUCCESS);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}


void daemonize() {
    // Fork off the parent process
    pid_t pid = fork();

    // Exit if the fork was unsuccessful
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    // If we got a good PID, then we can exit the parent process
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // Change the file mode mask
    umask(0);

    // Create a new SID for the child process
    pid_t sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    // Change the current working directory (optional)
    // chdir("/");

    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Redirect standard file descriptors to /dev/null or log files
    int fd = open("/dev/null", O_RDWR);
    if (fd != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > 2) {
            close(fd);
        }
    }


}

//Leveraged from https://github.com/JeffreytheCoder/Simple-HTTP-Server/blob/939ede03cbfb45a5441da511b62b588962453358/server.c#L21
const char *get_file_extension(const char *file_name) {
    const char *dot = strrchr(file_name, '.');
    if (!dot || dot == file_name) {
        return "";
    }
    return dot + 1;
}

char* decode_url(const char* src) {
    size_t src_len = strlen(src);
    char* decoded = (char*)malloc(src_len + 1);
    size_t decoded_len = 0;

    for (size_t i = 0; i < src_len; i++) {
        if (src[i] == '%') {
            if (i + 2 < src_len) {
                char hex[3];
                hex[0] = src[i + 1];
                hex[1] = src[i + 2];
                hex[2] = '\0';
                int hex_val;
                if (sscanf(hex, "%2x", &hex_val) == 1) {
                    decoded[decoded_len++] = (char)hex_val;
                    i += 2;
                } else {
                    decoded[decoded_len++] = src[i];
                }
            } else {
                decoded[decoded_len++] = src[i];
            }
        } else {
            decoded[decoded_len++] = src[i];
        }
    }

    decoded[decoded_len] = '\0';
    return decoded;
}


const char *get_mime_type(const char *file_ext) {
    // Loop through the MimeMapping array to find a match
    for (int i = 0; mimeMappings[i].extension != NULL; i++) {
        if (strcasecmp(file_ext, mimeMappings[i].extension) == 0) {
            return mimeMappings[i].mime_type;
        }
    }
    // If no match is found, return the default MIME type
    return mimeMappings[6].mime_type; // Default MIME type is at index 6
}


//Leveraged wholesale from https://github.com/JeffreytheCoder/Simple-HTTP-Server/blob/939ede03cbfb45a5441da511b62b588962453358/server.c#L96C49-L96C49
void build_http_response(const char *file_name, 
                        const char *file_ext, 
                        char *response, 
                        size_t *response_len) {
    // build HTTP header
    const char *mime_type = get_mime_type(file_ext);
    char *header = (char *)malloc(BUFFER_SIZE * sizeof(char));
    snprintf(header, BUFFER_SIZE,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "\r\n",
             mime_type);

    // if file doesnt exist, response is 404 Not Found
    int file_fd = open(file_name, O_RDONLY);
    if (file_fd == -1) {
        snprintf(response, BUFFER_SIZE,
                 "HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/plain\r\n"
                 "\r\n"
                 "404 Not Found");
        *response_len = strlen(response);
        return;
    }

    // get file size for Content-Length
    struct stat file_stat;
    fstat(file_fd, &file_stat);


    // copy header to response buffer
    *response_len = 0;
    memcpy(response, header, strlen(header));
    *response_len += strlen(header);

    // copy file to response buffer
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, 
                            response + *response_len, 
                            BUFFER_SIZE - *response_len)) > 0) {
        *response_len += bytes_read;
    }
    free(header);
    close(file_fd);
}

void *thread_function(void *thread_param) {

    //Get the params
    thread_data_t *threadData = (thread_data_t *) thread_param;
    int client_socket = threadData->client_socket;
    ssize_t bytes_received;

   
   
    //printf("Thread %d waiting on data!  \r\n", threadData->thread_num);
	//int res;
	
	//Create a socket to the flask server server
	char *flask_response = malloc(BUFFER_SIZE);
	int flask_socket = socket(AF_INET, SOCK_STREAM, 0);
	
	if (flask_socket == -1) {
		perror("Flask socket creation failed");
		close(client_socket);			
	}
	
	 // Connect to the flask server
	if (connect(flask_socket, (struct sockaddr *)&flask_addr, sizeof(flask_addr)) == -1) {
		perror("Flask connection failed");
		close(client_socket);
		close(flask_socket);
	}
			
    //Waits for data
	
	char * recv_buffer = malloc(BUFFER_SIZE) ;

        bytes_received = recv(client_socket, recv_buffer, BUFFER_SIZE, 0);
        if (bytes_received == -1) {
            perror("recv failed");
            close(client_socket);

        } else if (bytes_received == 0) {
            // Connection closed by the client
            syslog(LOG_INFO, "Closed connection from thread %d \r\n", threadData->thread_num);
            printf("Connection closed \r\n");
            close(client_socket);

        }
		else{
			//Send the recieved request to the flask server
			send(flask_socket, recv_buffer, bytes_received, 0);
			
			//Get back the response and send it to the original client
			ssize_t flask_bytes_received;
			while ((flask_bytes_received = recv(flask_socket, flask_response, sizeof(flask_response), 0)) > 0) {
				send(client_socket, flask_response, flask_bytes_received, 0);
			}
			
			
			/*
			//First ensure that we recieved a GET request
			regmatch_t matches[2];
			res = regexec(&http_get_regex, recv_buffer, 2, matches, 0);
			if(res == 0){
			//Matches Get request format
			//We extract the filename being requested from the url if present
            recv_buffer[matches[1].rm_eo] = '\0';
            const char *url_encoded_file_name = recv_buffer + matches[1].rm_so;
			
			//Decode the Url encoded filename
            char *file_name = decode_url(url_encoded_file_name);

            // Get the file extension
            char file_ext[16];
            strcpy(file_ext, get_file_extension(file_name));

            
            // Build the HTTP responses
            char *response = (char *)malloc(BUFFER_SIZE * 2 * sizeof(char));
            size_t response_len;
            build_http_response(file_name, file_ext, response, &response_len);

            // send HTTP response to client
            send(client_socket, response, response_len, 0);

            free(response);
            free(file_name);
			
			}*/
		}
        
   
	free(recv_buffer);
	free(flask_response);
    close(client_socket);  // No need for client socket anymore
	close(flask_socket);
    threadData->thread_complete_success = true;
    return (void *) threadData;
}

int main(int argc, char *argv[]) {
    int thread_count = 0;
    int client_socket;
    struct addrinfo hints, *address_results, *nodes;
    char s[INET6_ADDRSTRLEN];
    struct sockaddr_storage their_addr;
    socklen_t sin_size;;
    int yes = 1;
    bool daemon_mode = false;

    openlog(NULL, 0, LOG_USER);



    // Parse command line arguments
    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = true;
        printf("Daemon Mode enabled \r\n");
    }

    //Set up SIG INT handler
    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        fprintf(stderr, "Cannot setup SIGINT!\n");
        return ERROR_RESULT;
    }

    // Setup SIG TERM Handler
    if (signal(SIGTERM, signal_handler) == SIG_ERR) {
        fprintf(stderr, "Cannot setup SIGTERM!\n");
        return ERROR_RESULT;
    }

	
	//Compile the regex for later use
	regcomp(&http_get_regex, "^GET /([^ ]*) HTTP/1", REG_EXTENDED);
	
	//Setup the flask application address
	flask_addr.sin_family = AF_INET;
    flask_addr.sin_port = htons(FLASK_PORT);
    flask_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Target server is on the same host so this is set localhost ofc




    //First get the available addresses on the host at port 80
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    int rc = getaddrinfo(NULL, PORT, &hints, &address_results);

    if (rc != 0) {
        //Handle Error
        fprintf(stderr, "Failed to get address info Error: %s\n", gai_strerror(rc));
        return ERROR_RESULT;
    }
    //Iterate through and bind
    for (nodes = address_results; nodes != NULL; nodes = nodes->ai_next) {
        if ((server_socket_fd = socket(nodes->ai_family, nodes->ai_socktype,
                                       nodes->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(server_socket_fd, nodes->ai_addr, nodes->ai_addrlen) == -1) {
            close(server_socket_fd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(address_results);

    //Make sure bind was successful
    if (nodes == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        return ERROR_RESULT;
    }

    //since bind was successful now lets start as a daemon
    if (daemon_mode) {
        daemonize();
    }

    //Start listening using socket
    if (listen(server_socket_fd, BACKLOG) == -1) {
        perror("failed to listen to connection");
        return ERROR_RESULT;
    }

    //Waits for connections
    while (1) {  // main accept() loop
        sin_size = sizeof their_addr;
        client_socket = accept(server_socket_fd, (struct sockaddr *) &their_addr, &sin_size);
        if (client_socket == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *) &their_addr),
                  s, sizeof s);
       // printf("server: got connection from %s \r\n", s);
        syslog(LOG_INFO, "Accepted connection from %s \r\n", s);


        //Create the parameters needed by the thread
        thread_data_t *threadData = (thread_data_t *) malloc(sizeof(thread_data_t));


        //Setup threadData
        threadData->client_socket = client_socket;
        threadData->thread_num = thread_count++;

        threadData->thread_complete_success = false;

        //Now we create a new node
        thread_node_t *new_node = malloc(sizeof(thread_node_t));

        //Set a pointer to the thread params within the node
        new_node->thread_params = threadData;

        //Create the new thread to handle the connection
        rc = pthread_create(&new_node->thread_id, NULL, thread_function, (void *) threadData);

        //Now put the node into our linked list
        SLIST_INSERT_HEAD(&threadListHead, new_node, entries);


        //Check all threads to see if any are complete
        // Traverse the list and remove elements safely using SLIST_FOREACH_SAFE.
        thread_node_t *currentElement, *tempElement;
        SLIST_FOREACH_SAFE(currentElement, &threadListHead, entries, tempElement) {
            //Check to see if the thread is completed
            if (currentElement->thread_params->thread_complete_success) {
                //printf("Cleanup of thread/node %d occuring \r\n", currentElement->thread_params->thread_num);
                //Remove the output file
               
                //Join the thread to cleanup its resources
                pthread_join(currentElement->thread_id, NULL);
                // Remove the element safely from the list.
                SLIST_REMOVE(&threadListHead, currentElement, thread_node, entries);
                //Free the thread param data
                free(currentElement->thread_params);
                //Free the node itself
                free(currentElement);

            }
        }
    }

    return 0;

}
