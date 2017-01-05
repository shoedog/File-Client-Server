/**
* Author: Wesley Jinks
* Date: 11/29/2016
* Last Mod: 11/29/2016
* File Name: ftserver.cpp
*
* Overview: Simple file server to list current directory and send available files on request
*
* Input: Usage: ./ftserver <PORT_TO_LISTEN_ON>
*   disconnect: CTRL-C
*
* Output: Error Messages for errors including connect, send, listen, file, and usage errors
*		  Server Listening Message
*		  Server-Client Connect Message
*		  Status Message for request to list directory and sending directory contents
*		  Status Message for getting file to send, file not found, sending file, data amount sent
*
* References:
*   Beej's Guide to Network Programming:
*       http://beej.us/guide/bgnet/
*
*	Linux Gazette: Issue 74: Linux Socket Programming in C++
*   http://www.tldp.org/LDP/LG/issue74/tougher.html
*
*   c++: 
*		http://www.cplusplus.com/reference/
*			<string.h> library
*			<stdio.h> library for file operations
*			free & malloc
*		
*		http://stackoverflow.com/questions/2720858/what-format-specifier-to-use-for-printing-unsigned-long-long-in-c-with-getting
*		http://stackoverflow.com/questions/23119615/coding-ftp-service-over-tcp-in-c-code
*
*   file related:
*		http://www.skorks.com/2010/03/how-to-quickly-generate-a-large-file-on-the-command-line-with-linux/
*		http://www.linuxquestions.org/questions/programming-9/c-list-files-in-directory-379323/
*		http://www.gnu.org/software/libc/manual/html_node/File-System-Interface.html#File-System-Interface
*		http://stackoverflow.com/questions/612097/how-can-i-get-the-list-of-files-in-a-directory-using-c-or-c
*   
*   linux man pages: http://man7.org/linux/man-pages/index.html
*						* getaddrinfo
*						* getnameinfo
*						* stat(1) and stat(2)
*
*   MakeFile Related:
*       http://www.cs.umd.edu/class/fall2002/cmsc214/Tutorial/makefile.html
*       http://www.cprogramming.com/tutorial/makefiles.html
*/

#include <string>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <dirent.h>


#define BACKLOG 10


// Get pointer to ip4 or ip6 using sock_addr_in
void *get_in_addr(struct sockaddr *); 

// Set addrinfo initializer and calls getaddrinfo to fill pointer to addrinfo
int setStructs(char*[], struct addrinfo *, struct addrinfo **);

// Set addrinfo for connecting to a listener
int setStructsOut(char *s, char *port, struct addrinfo *hints, struct addrinfo **servinfo);

// Open socket for listening
int initiateListen(int *, struct addrinfo *);

// Connect to socket for data transfer
int initiateConnect(int *sockfd, struct addrinfo *servinfo);

// Print IP and Port Number when server is listening
void show_hostinfo( int port, struct addrinfo *servinfo );

// Handle Child Process Signals
void sigchld_handler(int s);

// Determine which command to undertake (-l, -g)
int parse_cmd(char *);

// Handle -l CMD
int handle_dircmd(int, char *, int *);

// Handle -g FILENAME command
int handle_getfilecmd(int d_port, int port, char *client, int *client_fd, int *new_fd, char *buf);

int main(int argc, char*argv[]){

    int sockfd;								// socket file descriptor
	int new_fd;								// new server  connection on new file descriptor
	int client_fd;							// new client connection on new file descriptor
	int port;								// port number to use
	int d_port;								// port number to use
	int numbytes;							// recv size
    struct addrinfo addr;					// address info initializer
    struct addrinfo *addr_ptr;				// pointer to getaddrinfo() results
	struct sockaddr_storage client_addr;	// client address
	socklen_t sin_size;
    char buf[1024];							// buffer
	char data_port[10];						// data_port number to use
	char client[1024];
	char service[20];    
	char s[INET6_ADDRSTRLEN];

	// Validate Port, Make sure it isn't in the well known port range	
    if (argc == 2 ) {
		port = atoi(argv[1]);
		if ( port < 1024 || port > 49151  ){
			fprintf( stderr, "invalid port: %s, must be greater than 1024 and less than 49151 to avoid well known ports.\n", argv[1] );
			exit(1);
		}
    } else if ( argc != 2 ) {
		fprintf(stderr, "\n]>USAGE: server <SERVER_PORT>\n");
		exit(1);
	}

    memset(&addr, 0, sizeof(addr));        // make sure struct is empty
	setStructs(argv, &addr, &addr_ptr );   // set addr_info structs
	initiateListen(&sockfd, addr_ptr);    // bind to socket and listen
	show_hostinfo(port, addr_ptr);		  // print server listening message with address and port of server

	freeaddrinfo(addr_ptr);	

	// main accept() loop
	while(1) {
		memset(&buf, '\0', sizeof buf);
		
		// accept connection on new socket file descriptor
        sin_size = sizeof client_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

		// Get client address to print connection message
        inet_ntop(client_addr.ss_family,
            get_in_addr((struct sockaddr *)&client_addr),
            s, sizeof s);
		getnameinfo((struct sockaddr *)&client_addr, sizeof client_addr, client, sizeof client, service, sizeof service, 0);
		printf("\nConnection from: %s\n", client); 
		
		// Get Data Port from client
		if ( (numbytes = recv( new_fd, buf, sizeof buf-1, 0 )) == -1 ) {
			perror("recieving port number");
		}
		printf("recv port\n");
		buf[numbytes] = '\0'; 
		strcpy(data_port, buf);
		
		// Recieve Command from Client
		if ( (numbytes = recv( new_fd, buf, sizeof buf-1, 0 )) == -1 ) {
			perror("receiving command");
		}
		
		printf("recv command\n");
		// Parse Command, if invalid send message to client, otherwise fork
		if ( parse_cmd(buf) == -1 ){
			printf( "error: invalid command" );
			if (send(new_fd, "ERROR: Invalid Command \nUSAGE: (-l) or (-g FILENAME)", 51, 0) == -1)
				perror("send");
		} else {
		
			// Fork to open data connection for valid command
			pid_t cpid;
			if ((cpid = fork() ) < 0 )
				perror("fork error");
		
			if (cpid == 0) { // in child	
				close(sockfd); // child doesn't need
				
				// Create data connection on data_port
				memset(&addr, 0, sizeof(addr));        
				setStructsOut(s, data_port, &addr, &addr_ptr );	
				sleep(1);
				printf("connecting\n");
				if ( initiateConnect(&client_fd, addr_ptr) == -1 )
					exit(0);

				// read client data_port after connecting for messages
				socklen_t len;
				len = sizeof addr;
				getpeername(client_fd, (struct sockaddr *)&addr, &len);
				struct sockaddr_in *s = (struct sockaddr_in *)&addr;
				d_port = ntohs(s->sin_port);

				// Parse Command: list directory structure
				if ( parse_cmd(buf) == 1 ) {
					handle_dircmd(d_port, client, &client_fd);
				
				// Parse CMD: Send File
				} else if ( parse_cmd(buf) == 2 ) {
					handle_getfilecmd(d_port, port, client, &client_fd, &new_fd, buf);
				}

				close(client_fd); // done with data connection
				close(new_fd);	  // done with command connection
				exit(0);
			}
		}
	    close(new_fd);  // parent doesn't need this
    }

	close(sockfd); //close sock
	return 0;
}

/******************************************************************************
*   Function: *get_in_addr
*
*   Description: Get pointer to ip4 or ip6 using sock_addr_in
*
*   Entry: Socket to reference
*
*   Exit: Socket with sin_addr or sin_addr6 set
*
*   Purpose: Determine if IP4 or IP6 and fill sin_addr appropriately
*
******************************************************************************/
void *get_in_addr(struct sockaddr *sa) {

    // Fill IP4 address
    if ( sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    // Fill IP6 address
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/******************************************************************************
*   Function: setStructs
*
*   Description: Sets addrinfo initializer, and calls getaddrinfo to fill
*       pointer to addrinfo for external connection
*
*   Entry: argv[]: array of arguments: [1]: IP Address, [2]: PORT
*          *hints: initial addrinfo options
*          **servinfo: addrinfo reference to initialize
*
*   Exit: 0 on success, with **servinfo addrinfo initialized
*         1 with error message on error
*
*   Purpose: Fill address info for listening
*
******************************************************************************/
int setStructs(char*argv[], struct addrinfo *hints, struct addrinfo **servinfo){
    int status;     // status of filling addrinfo with getaddrinfo
	char hostname[128];

    //Set header ints
    hints->ai_family = AF_UNSPEC;      // allow IPv4 or IPv6
    hints->ai_socktype = SOCK_STREAM;  // TCP
	hints->ai_flags = AI_PASSIVE;	   // use IP of server

	gethostname(hostname, sizeof hostname);
    //Call get Address Info
    if ((status = getaddrinfo(hostname, argv[1], hints, servinfo )) != 0 ) {
        fprintf( stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    return 0;
}

/******************************************************************************
*   Function: setStructsOut
*
*   Description: Sets addrinfo initializer, and calls getaddrinfo to fill
*       pointer to addrinfo for external connection
*
*   Entry: argv[]: array of arguments: [1]: IP Address, [2]: PORT
*          *hints: initial addrinfo options
*          **servinfo: addrinfo reference to initialize
*
*   Exit: 0 on success, with **servinfo addrinfo initialized
*         1 with error message on error
*
*   Purpose: Fill address info to create external connection
*
******************************************************************************/
int setStructsOut(char *s, char *port, struct addrinfo *hints, struct addrinfo **servinfo){
    int status;     // status of filling addrinfo with getaddrinfo
	
    //Set header ints
    hints->ai_family = AF_UNSPEC;      // allow IPv4 or IPv6
    hints->ai_socktype = SOCK_STREAM;  // TCP

    //Call get Address Info
    if ((status = getaddrinfo(s, port, hints, servinfo )) != 0 ) {
        fprintf( stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    return 0;
}

/******************************************************************************
*   Function: initiateListen
*
*   Description: Initializes socket and listens
*
*   Entry: *sockfd: the socket file descriptor to initialize
*          *servinfo: the addrinfo for the host to initialize socket & connect
*
*   Exit: returns 0 on success with connection message
*         exits on failure with error message
*
*   Purpose: Binds and Opens socket for listening
*
******************************************************************************/
int initiateListen(int *sockfd, struct addrinfo *servinfo){
    struct addrinfo  *p;        // address info pointer to loop over servinfo
    struct sigaction sa;		// signal action handler for processes
	int yes=1;
	

    // Loop over and connect to bind to first possible result
    for ( p = servinfo; p != NULL; p = p -> ai_next ) {

        // Fill socket file descriptor: domain, type, protocol
        if ( ( *sockfd = socket(
            p->ai_family, p->ai_socktype, p->ai_protocol) )
            == -1 ) {
                perror("server: socket");
                continue;
            }

        // setsockopt
        if ( setsockopt( *sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int )) == -1 ) {
            perror("setsockopt");
            exit(1);
        }

		// bind socket
		if (bind(*sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(*sockfd);
			perror("server: bind socket");
			continue;
		}

        break;
    }

    // Print error message on connect failure
    if ( p == NULL ) {
        fprintf(stderr, "server failed to bind\n");
        exit(1);
    }

	// Listen
	if (listen(*sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}
	
	sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

	return 0;
}

/******************************************************************************
*   Function: initiateConnect
*
*   Description: Initializes socket connection to external host
*
*   Entry: *sockfd: the socket file descriptor to initialize
*          *servinfo: the addrinfo for the host to initialize socket & connect
*
*   Exit: returns 0 on success with connection message
*         exits on failure with error message
*
*   Purpose: Connect over socket to external address
*
******************************************************************************/
int initiateConnect(int *sockfd, struct addrinfo *servinfo){
    struct addrinfo  *p;        // address info pointer to loop over servinfo
	char s[INET6_ADDRSTRLEN];	// address string length	

    // Loop over and connect to first possible result
    for ( p = servinfo; p != NULL; p = p -> ai_next ) {

        // Fill socket file descriptor: domain, type, protocol
        if ( ( *sockfd = socket(
            p->ai_family, p->ai_socktype, servinfo->ai_protocol) )
            == -1 ) {
                perror("client: connect");
                continue;
            }

        // connect
        if ( connect( *sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1 ) {
            close(*sockfd);
			perror("client connect");
            continue;
        }

        break;
    }

    // Print error message on connect failure
    if ( p == NULL ) {
        fprintf(stderr, "server failed to connect to client for data transfer\n");
		return -1;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	//printf("server: connected to %s for data transfer\n", s);

	return 0;
}

/******************************************************************************
*   Function: show_hostinfo
*
*   Description: Get host address to display message
*
*   Entry: Port, filled addrinfo for server that is bound to socket and listening
*
*   Exit: Prints message that server is open on ADDRESS and PORT
*
*   Purpose: Display listening servers address and port
*
*******************************************************************************/
void show_hostinfo( int port, struct addrinfo *servinfo ) {
    struct addrinfo  *p;        // address info pointer to loop over servinfo
	char ipstr[INET6_ADDRSTRLEN];
	char host[1024];
	char service[20];   
 
	// Loop over addrinfo struct to first possible result
    for ( p = servinfo; p != NULL; p = p -> ai_next ) {
		void *addr;
		
		// get pointer to address
		if ( p->ai_family == AF_INET) {
			struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
			addr = &(ipv4->sin_addr);
			getnameinfo((struct sockaddr *)p, sizeof ipv4, host, sizeof host, service, sizeof service, 0);
		} else {
			struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
			addr = &(ipv6->sin6_addr);
			getnameinfo((struct sockaddr *)p, sizeof ipv6, host, sizeof host, service, sizeof service, 0);
		}

		inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
		//printf("Server Listening at: %s on port %i\n", ipstr, port); 
		printf("\nServer open: %s on port %i\n", host, port); 	
	}
} 


/******************************************************************************
*   Function: sigchld_handler
*
*   Description: Handle errors in child fork
*
*   Entry: error to save
*
*   Exit: resets error
*
*   Purpose: Make sure errors in child aren't overwritten and ignored
*
*******************************************************************************/
void sigchld_handler(int s) {
    //waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;
    
    while(waitpid(-1, NULL, WNOHANG) > 0);
    
    errno = saved_errno;
}


/******************************************************************************
*   Function: parse_cmd
*
*   Description: Determines if command is "-l", "-g", or invalid
*
*   Entry: char * with command to check
*
*   Exit: Returns 1 for "-1", 2 for "-g", -1 for invalid command
*
*   Purpose: Check which command we should proces
*
*******************************************************************************/
int parse_cmd(char *cmd) {
	char *valid;

	if( (valid = strstr(cmd, "-l")) != NULL ) {
		return 1;
	} else if ( (valid = strstr(cmd, "-g")) != NULL ) {
		return 2;
	} else {
		return -1;
	}
}


/******************************************************************************
*   Function: handle_dircmd
*
*   Description: Opens and reads directory contents and sends to client
*
*   Entry: data_port, client name to print messages
*		 data file descriptor to send directory contents
*
*   Exit: Returns 0 on success, -1 for error
*
*   Purpose: Handle -l command from client to list directory contents
*
*******************************************************************************/
int handle_dircmd(int d_port, char *client, int *client_fd) {
	char buf[1024];	
	DIR *dp;			// point to directory 
	struct dirent *ep;	// pointer to directory entry	

	printf("List directory requested on port %d\n", d_port );
	memset(buf, '\0', sizeof(buf));

	// read directory to buffer to send
	if( (dp = opendir(".")) == NULL ){
		perror("Failed to open directory.");
		return -1;
	}

	// Loop to get entire directory, ignoring "." and ".."
	while ( (ep = readdir (dp)) != NULL) {
		if ( strlen(ep->d_name) > 1 && strncmp(ep->d_name, "..", 2) != 0 ){
			strcat(buf, ep->d_name);
			strcat(buf, "\n");
		}
	}
	closedir(dp);
		
	// Send Directory Contents
	printf( "Sending directory contents to %s:%d\n", client, d_port);
	if (send(*client_fd, buf, sizeof buf, 0) == -1){
		perror("send");
		return -1;
	}

	return 0;
}


/******************************************************************************
*   Function: handle_getfilecmd
*
*   Description: Opens and reads file and sends to client
*
*   Entry: data_port, command port, client addr to print messages
*		 data and command file descriptor to send directory contents
*		 buf that holds -g FILENAME
*
*   Exit: Returns 0 on successful send or file not found, otherwise exits on error
*
*   Purpose: Handle file requests from client
*
*******************************************************************************/
int handle_getfilecmd(int d_port, int port, char *client, int *client_fd, int *new_fd, char *buf) {
	
	char *fn;			//pointer to the filename string in buffer
	char *filename;		// filename string
	struct stat e;		// unix stat for file existence check 
	FILE *fp;			// file pointer
	unsigned long long size;	//handle large files
	char *filebuff;				//file read buffer
	size_t read_result;			// test file read to buffer success
	unsigned long long sent = 0;			// file data sent
	unsigned long long remaining;			//remaining data to send	
	long long n;							// data sent in one loop
	
	// get filename from command string
	fn = strtok(buf, "-g");
	fn++;
	filename = (char*) malloc (sizeof(char)*strlen(fn)+2);
	strcpy(filename, fn);
	printf( "File \"%s\" requested on port %d\n", filename, d_port);

	// check for file: file not found
	if ( stat(filename, &e) != 0 ){
		printf("File \"%s\" not found. Sending error message to %s:%d: ", filename, client, port);
		if (send(*new_fd, "FILE NOT FOUND", 14, 0) == -1)
			perror("sending FILE NOT FOUND");
	} else {
	
		// File found!	
		fp = fopen(filename, "r");
		
		//set file size
		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
		rewind(fp);
		
		// allocate memory for file
		filebuff = (char*) malloc (sizeof(char)*size);
		if ( filebuff == NULL ) {
			perror("Memory Error file buffer alloc");
			exit(2);
		}

		// copy file to buffer
		read_result = fread(filebuff, 1, size, fp);
		if ( read_result != size ) {
			perror("File read error");
			exit(3);
		}
			
		printf( "sending file \"%s\" to %s:%d\n", filename, client, d_port);
			

		// loop to send entire file
		remaining = size;
		while (sent < size) {
			n = send(*client_fd, filebuff+sent, remaining, 0);
			if ( n == -1 ){
				perror("Failed while sending FILE");
				break;
			} 
			sent += n;
			remaining -= n;
		}
		printf("Sent %llu of %llu\n", sent, size);
		
		free(filebuff);
	}
	free(filename);

	return 0;
}
