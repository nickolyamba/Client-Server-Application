// Name: Nikolay Goncharenko
// Email: goncharn@onid.oregonstate.edu
// Class: CS372-400
// Assignment: Project #2 ftserver.c

/*********************************************************************************\
* The program is server side of FTP-like application layer protocol.
* Server listens for a Control Connection from a client and request for action. 
* If command in request is invalid, server sends error message to the client via 
* Control Connection. Client closes Control Connection after receiving the Error.
*
* Command Line Arguments: <server's port>
* 
* -l <port> is a request for a list of files in the server's directory
* 	Action:  Initiates Data Connection on client's <port> and sends the requested list of files
*		Note: there is a limit of BUF_SIZE bytes for the directory list length.
*		Data Connection is closed by serverafter  file transfer is complete.
*
* -g <filename> <port> is a request for a particlar file from the servers directory 
*		Action:  Initiates Data Connection, reads file in chunks of BUF_SIZE and send each chunks
*		immediately after reading it in a buffer of BUF_SIZE. Data Connection is closed by server
*		after  file transfer is complete.
*		If filename doesn't exist, error message is sent on  Control Connection. Client closes 
* 	Control Connection after receiving the Error.
*
* After client, or server  closes connection, server continues listen to 
* incoming connection from a client until SIGINT is given..
**********************************************************************************/

// Sources: http://beej.us/guide/bgnet/output/html/multipage/clientserver.html#simpleserver
// http://www.linuxhowtos.org/C_C++/socket.htm
// http://stackoverflow.com/questions/3060950/how-to-get-ip-address-from-sock-structure-in-c
// ip of the server: ifconfig | awk '/inet addr/{print substr($2,6)}'
// ftserver 38546

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
 
#define BACKLOG 3
#define FILESIZE 100
#define SMALL_BUF_SIZE 100

const int BUF_SIZE = 1024;
//const int SMALL_BUF_SIZE = 100;
const int NUM_PARAMS = 3;

struct Request{
		char command [2];
		int  data_port;
		char filename[SMALL_BUF_SIZE];
		char client_IP[SMALL_BUF_SIZE];
};

// get and validate command-line input
int  get_server_port(int argc, char* argv[]);

// startup
int startup(int, struct Request*);

// Bind Port to listen to a client
int bind_port(struct sockaddr_in *, int);

// Send until all the intended data is sent
int sendall(int, char *, int *);

// Send Message
void send_mess (int, char*);

// Receive Message
int handleRequest(int, char*, struct Request*);

// Send List of files in the directory
int send_directory(struct Request*);

// Send file's content
void send_file(int, struct Request*);

int validate_filename(struct Request*);

void get_filenames_list(char*);

int create_data_connection();

void parseRequest(int, char*, struct Request*);

void validateRequest(int, char[][SMALL_BUF_SIZE], struct Request*);

int validatePort(int, char*);

// Read until all the packet is read
int readall(int fd, char *buf, int *len);

void error(const char *);

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, PORTNUM, isQuit;
	struct Request request;

    // structure containing an internet address, sin_family, sin_port
    // serv_addr for the address of the server,
    struct sockaddr_in serv_addr;

    // Server Name for prompt 
    //char server_name[] = {"Oregon"};
	char requestData[2*SMALL_BUF_SIZE];

    isQuit = 0;

    // get port number from command-line argv
    PORTNUM = get_server_port(argc, argv);
	
    // create socket and bind it to a port
    sockfd = bind_port(&serv_addr, PORTNUM);

    // loop waiting for a clinet to connect until SIGINT issued
    while(1)
    {
		bzero(&request, sizeof(request));
		// listen to incoming connection
		newsockfd = startup(sockfd, &request);
		
        // loops until either server, or client write '\quit'
        while(1)
        {
		   // get message from client
            isQuit = handleRequest(newsockfd, requestData, &request);
			if(isQuit)
				break;
        }//while

        close(newsockfd); // closes connection with the current client
    }//while

    close(sockfd);

    return 0;
}


/************************************************************************************\
* Function: get_server_port()
*
* Purpose:  get  input from command line and validates it   
*
* Receives: 
*           argc: number of command line args + filename
*			 argv*[]: array of pointers to command line args
*
* Returns:  validated port number in the range [1025; 65535]
*
* Pre:    None
*			
*
* Post:    Validated port number is returned
*            
*************************************************************************************/
int  get_server_port(int argc, char* argv[])
{
	int port_num = 0;
	
	if (argc < 2){
		printf("ERROR: no port provided as a command line argument\n");
		exit(EXIT_FAILURE);
	}
	
	else if  (argc > 2){
		printf("ERROR: to many command line argument. Only server port number is required!\n");
		exit(EXIT_FAILURE);
	}
	
	for (char *ptr = argv[1]; *ptr != '\0'; ptr++){
		if (!isdigit(*ptr))
		{
			printf("ERROR: server port number must be a 16 bit number!\n");
			exit(EXIT_FAILURE);
		}
	}
	
	port_num = atoi(argv[1]);
	
	if(port_num > 1024 && port_num <= 65535 )
		return port_num;
	
	else {
		printf("ERROR: server port must be in the range [1025; 65535]\n");
		exit(EXIT_FAILURE);
	}
}


/************************************************************************************\
* Function: bind_port()
*
* Purpose:  This function creates a socket and binds its file descriptor to the port,
*                  provided by a user as a command line argument
*
* Receives: 
*           serv_addr: pointer to struct sockaddr_in instance
*
*           PORTNUM: port provided by a user as a command line argument
*
* Returns:  sockfd - value of the socket file descriptor bind to the PORTNUM.
*
* Pre:      PORTNUM is a valid, available port number, 
*
* Post:     The values in the struct instance pointed by "serv_addr" are filled 
*              with data needed for creating a connection.
*************************************************************************************/
int bind_port(struct sockaddr_in *serv_addr, int PORTNUM){
    int sockfd; // socket file descriptor

    // create new TCP socket, return file descriptor reserved for it
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // if -1, error
    if (sockfd < 0) 
        error("ERROR opening socket");

    // zero out serv_addr struct instance
    bzero((char *) serv_addr, sizeof(*serv_addr));

    serv_addr->sin_family = AF_INET;
    serv_addr->sin_addr.s_addr = INADDR_ANY; // save address of the server
    serv_addr->sin_port = htons(PORTNUM); // convert to big-endain before copying

    // bind socket file descriptor to the server's address
    // ptr to serv_addr needs to be cast
    if (bind(sockfd, (struct sockaddr *) serv_addr, sizeof(*serv_addr)) < 0) 
        error("ERROR on binding");
	else 
		printf("\nServer open on %d\n", PORTNUM);
      
    return sockfd;
}


/************************************************************************************\
* Function: startup()
*
* Purpose: Listens for clients to connect and blocks until there is a succesful connection.   
*         		   
*
* Receives: 
*           sockfd: file descriptor for port on which the server must listen to for clients
*
* Returns:  newsockfd - value of a socket file descriptor bound to new client connection.
*
* Pre:      sockfd is a valid file descriptor
*
* Post:     Connection to a new client is established
*************************************************************************************/
int startup(int sockfd, struct Request* request)
{
	 int newsockfd, ipAddrInt;
    
    // structure containing an internet address, sin_family, sin_port
    // serv_addr for the address of the server,
    // cli_addr contains the address of the client
    struct sockaddr_in cli_addr;
    struct sockaddr_in* ipAddr;
	socklen_t clilen = sizeof(cli_addr); //  the size of the address of the client

    // string representation of ip
    char ip_str[INET_ADDRSTRLEN];
	
	// listen for income connection
	 // BACKLOG - number of connections that 
	 // can wait in queue for this connection to finish
	 printf("\nStarted listening for a new client to connect...\n");
	 listen(sockfd, BACKLOG);      
	 
	 // blocks process, until new connection is accepted on a
	 // new socket that is assigned a newsockfd file descriptor
	 // cli_addr contains address of client after connection is established
	 newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
	 
	 // get IP address of a client
	 //http://stackoverflow.com/questions/3060950/how-to-get-ip-address-from-sock-structure-in-c
	 ipAddr = (struct sockaddr_in*)&cli_addr;
	 ipAddrInt = ipAddr->sin_addr.s_addr;
	 inet_ntop(AF_INET, &ipAddrInt, ip_str, INET_ADDRSTRLEN);
	 
	 printf("\nConnection from: %s\n", ip_str);
	 
	 // save IP in the struct variable request:
	 strcpy(request->client_IP, ip_str);
	 
	 // -1 if error
	 if (newsockfd < 0) 
		  error("ERROR on accept()");
	  
	  return newsockfd;
}


int validatePort(int sockfd, char* port){
	int port_num = -1;
	
	for (char *ptr = port; *ptr != '\0'; ptr++){
		if (!isdigit(*ptr))
		{
			printf("ERROR: server DATA_PORT must be a 16 bit number!\n");
			
			//exit(EXIT_FAILURE);
		}
	}
	
	// if got here, then contains digits only
	port_num = atoi(port);
	
	// validate non-reserved ports
	if(port_num > 1024 && port_num <= 65535 )
		return port_num;
	
	else {
		printf("ERROR: DATA_PORT must be in the range [1025; 65535]\n");
		//exit(EXIT_FAILURE);
	}
	
	return port_num;
}


/*****************************************************************************\
* Function: validateRequest()
*
* Purpose:  This function receives request from the parseRequest(). and validates  it
*
* Receives: 
*			 message: char ptr to buffer where to store request
*				
* Returns:  None; stores parsed  data in the global struct variable 'request'
*
* Pre:
*			message  != NULL
*
* Post:     request parsed , validated, and saved in global struct variable 'request'
******************************************************************************/
void validateRequest(int sockfd, char data[][SMALL_BUF_SIZE], struct Request* request){
	char list[] = {"-l"};
	char get[] = {"-g"};
	char error_mess[BUF_SIZE];
	bzero(error_mess, sizeof(error_mess));
	strcpy(error_mess, "ERROR: 3d argument (COMMAND) must be either -l , or -g!\n");
	
	// If command is not -l and not -g, send error message to the client
	if ((strcmp(data[0], list)) && (strcmp(data[0], get)))
	{
		printf("Invalid command by client. Sending ERROR message to %s\n", request->client_IP);
		send_mess(sockfd, error_mess);
	}
	
	// if valid, save data in struct
	else
	{
		strcpy(request->command,  data[0]);
		request->data_port = validatePort(sockfd, data[1]);
		// if -g
		if(!strcmp(data[0], get))
			strcpy(request->filename,  data[2]);
	}
	//printf("Parsed: %s____%d____%s\n", request->command, request->data_port, request->filename);
}


/*****************************************************************************\
* Function: parseRequest()
*
* Purpose:  This function receives request from the handleRequest(). It  parses, validates 
*						and saves the request in the global struct variable 'request'
*
* Receives: 
*			 message: char ptr to buffer where to store request
*				
* Returns:  None; stores parsed  data in the global struct variable 'request'
*
* Pre:
*			message  != NULL
*
* Post:     request parsed , validated, and saved in global struct variable 'request'
******************************************************************************/
void parseRequest(int sockfd, char* message, struct Request* request){
	char *ptr = message;
	char data[NUM_PARAMS][SMALL_BUF_SIZE];
	bzero(data, NUM_PARAMS*SMALL_BUF_SIZE);
	int i = 0, j = 0;
	
	// each while loop reads one parameter
	while(*ptr != '\0')
	{
		// each for loop reads one char from paramater
		for(j = 0; *ptr != '&' && *ptr != '\0'; ptr++, j++)
		{  
			data[i][j] = *ptr;
		}
		i++;
		ptr++;
	}
	//strcpy(request.command,  data[0]);
	//request.data_port = atoi(data[1]);
	//strcpy(request.filename,  data[2]);
	
	//printf("Parsed: %s____%d____%s____%s\n", request.command, request.data_port, request.filename, data[1]);
	validateRequest(sockfd, data, request);
}


/*****************************************************************************\
* Function: handleRequest()
*
* Purpose:  This function receives request from the client by calling linux 
*			 system call  read(). Then it calls parseRequest() function  that
* 		 parses the request, validates it, and saves data in the global struct variable 'request'
*
* Receives: 
*           sockfd: file descriptor of the socket that accepted connection
*							 from the client
*			 message: char ptr to buffer where to store request
*				
*
* Returns:  isQuit = 0 if read more than 0 bytes from the socket.
*           isQuit = 1 if read no bytes are read from the socket and client 
*			closed connection
*
* Pre:      sockfd is a valid file descriptor of the socket that accepted
*				 connection from the client
*				 message  != NULL
*
* Post:     request read,  parsed , validated, and saved in global struct variable 'request'
******************************************************************************/
int handleRequest(int sockfd, char* message, struct Request* request){
    int num_read;

    bzero(message, 2*SMALL_BUF_SIZE);

    num_read = read(sockfd, message, BUF_SIZE);
	//printf ("Received: %d\n", num_read);
	if(num_read==0)
    {
        // closes serving socket after client closes  Control Connection
		//printf("Client terminated connection\n");
        return 1; // to exit while loop and close the socket
    }
    else if(num_read < 0)
    {
        perror("Read error from client\n");
        exit(EXIT_FAILURE);
    }
	
    // display the Client message
    //printf ("Message from client: %s\n", message);
	
	// parse, validate and save the request in a struct
	parseRequest(sockfd, message, request);
	
	if (!strcmp(request->command, "-l"))
	{
		printf("\nList directory requested on port %d\n", request->data_port);
		send_directory(request);
	}
				
	else if(!strcmp(request->command, "-g"))
	{
		printf("File \"%s\" requested on port %d\n", request->filename, request->data_port);
		send_file(sockfd, request);
	}
    
    return 0;
}



/************************************************************************************\
* Function: create_data_connection()
*                      resources used:  // http://www.linuxhowtos.org/C_C++/socket.htm
* Purpose: Creates socket and initiates Data Connection on this socket  
*         		   
*
* Receives: 
*           None
*
* Returns:  sockfd - value of a socket file descriptor bound to new Data Connection.
*
* Pre:      request->data_port and request->client_IP are valid.
*
* Post:     Data Connection to a  client is established
*************************************************************************************/
int create_data_connection(struct Request* request)
{
	int sockfd;
    struct sockaddr_in servaddr;
	
    // connection
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    // connect to this address: &servaddr
    // to this port: SERV_PORT
    // using: AF_INET   
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(request->data_port);
    
    // converts IPv4 and IPv6 addresses from text xxx.xxx.xxx.xxx
    // to binary form inet_pton(3)
    inet_pton(AF_INET, request->client_IP, &servaddr.sin_addr);
    
    // connect sockets
    if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
		error("ERROR connecting on DATA_PORT");
	
	return sockfd;
}



/*****************************************************************************\
* Function: send_directory()
*
* Sources Used: 
* http://en.wikibooks.org/wiki/C_Programming/POSIX_Reference/dirent.h
*  http://stackoverflow.com/questions/4204666/how-to-list-files-in-a-directory-in-a-c-program
*
* Purpose:  This function reads filenames in the current program directory
*						and sends the list of filenames to the client on new Data Connection
*
* Receives:  None
*
* Returns:  1 on success;
*
* Pre:      sockfd is a valid file descriptor of the socket that accepted connection from the client
*              server_name != NULL 
*
* Post:     The list of filenames is sent to the client on Data Connection
******************************************************************************/
int send_directory(struct Request* request)
{
	int data_socket;
	char file_list[BUF_SIZE];
	bzero(file_list, BUF_SIZE);
	
	// fill with filenames
	get_filenames_list(file_list);
	
	// create Data Connection
	data_socket = create_data_connection(request);
	
	//send to the client
	printf("Sending directory contents to %s:%d\n", request->client_IP, request->data_port);
	send_mess(data_socket, file_list);
	
	close(data_socket);
	
	return 1;
}


/*****************************************************************************\
* Function: get_filenames_list()
*
* Sources Used: 
* http://en.wikibooks.org/wiki/C_Programming/POSIX_Reference/dirent.h
*  http://stackoverflow.com/questions/4204666/how-to-list-files-in-a-directory-in-a-c-program
*
* Purpose:  This function reads filenames in the current program directory
*						saves them in the char string pointed by parameter
*
* Receives:  file_list - ptr to cahr string
*
* Returns:  none;
*
* Pre:      file_list!=NULL, l
*				 Total length  of filenames < BUF_SIZE
*
* Post:     List of filenames in the current dir written into char string.
*					Filenames are  delimited by "\n"
******************************************************************************/
void get_filenames_list(char* file_list)
{	
	DIR *d;
	struct dirent *dir;
	
	d = opendir(".");
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			if(strcmp(dir->d_name, ".") && strcmp(dir->d_name, ".."))	
			{	strcat(file_list, dir->d_name);
				strcat(file_list, "\n");
				//printf("%s\n", dir->d_name);
			}
		}
		closedir(d);
	}

	// delete the last "\n" char
	file_list[strlen(file_list)-1] = '\0';
}


/*****************************************************************************\
* Function: send_file()
*
* Purpose:  Send content of the filename requested by client if filename is valid
*						Send error to a client if filename is invalid
*						File is sent via Data Connection
*						Error is sent via Control Connection
* Sources: http://linux.die.net/man/2/stat
*					 http://man7.org/tlpi/code/online/dist/sockets/read_line.c.html
*					 http://man7.org/tlpi/code/online/dist/fileio/copy.c.html
*
* Receives:  Request struct ptr
*						 int: Data Connection socket
*
* Returns:  None
*
* Pre:      request !=NULL, sockfd is valid Data Connection socket
*				 
*
* Post:   either file, or error sent to a client 
******************************************************************************/
void send_file(int sockfd, struct Request* request)
{
	int input_fd;
	struct stat st; // to get size of the file
	off_t file_size, num_read; // 2^x on x-bit machine
	int data_socket;
	char error_mess[BUF_SIZE];
	char file_buffer[BUF_SIZE];
	char size_of_file[BUF_SIZE];
	bzero(size_of_file, sizeof(size_of_file));
	bzero(error_mess, sizeof(error_mess));
	//off_t total_read;
	
	strcpy(error_mess, "ERROR: FILE NOT FOUND!");
	//strcpy(error_mess, (char*)(request->filename));
	//strcpy(error_mess, " ");
	
	// send error if invalid
	if (!validate_filename(request))
	{
		printf("File %s not found. Sending error message to %s:%d!\n", 
			request->filename, request->client_IP, request->data_port);
		send_mess(sockfd, error_mess);
		return;
	}
	
	// create Data Connection socket, connect to the client
	data_socket = create_data_connection(request);
	
	input_fd = open(request->filename, O_RDONLY);
	if (input_fd == -1) 
    {
        printf("Error opening file %s!\n", request->filename);
        close(data_socket);
		return;
    }
    //printf("\nStart reading: %s...\n", request->filename);
	
	// get file size
	fstat(input_fd, &st);
	file_size = st.st_size;
	
	sprintf(size_of_file, "%lld", (long long)file_size);
	//printf("\nFile Size: %s \n", size_of_file);
	 
	 // get first read and number of num_read
	 bzero(file_buffer, sizeof(file_buffer));
		if ((num_read = read(input_fd, file_buffer, sizeof(file_buffer))) == -1)
			error("read file error");
	
	// send to Data Connection and continue read until until EOF
	while(num_read > 0)
	{
		send_mess(data_socket, file_buffer);
		
		//total_read = total_read + num_read;
		
		bzero(file_buffer, sizeof(file_buffer));
		if ((num_read = read(input_fd, file_buffer, sizeof(file_buffer))) == -1)
			error("read file error");
	}
	
	//printf("File Size: %zd   Total Read %zd\n", file_size, total_read);
	
	close(data_socket);
	close(input_fd);
}


/*****************************************************************************\
* Function: validate_filename()
*
* Purpose:  This function validates the requested by client filename
*
* Receives:  Request struct ptr
*
* Returns:  1 if filename valid, 0 if invalid
*
* Pre:      request !=NULL
*				 
*
* Post:   filename stored in a struct is validated
******************************************************************************/
int validate_filename(struct Request* request)
{
	char file_list[BUF_SIZE];
	bzero(file_list, sizeof(file_list));
	char* ptr = file_list;
	char filename[SMALL_BUF_SIZE];
	int i=0, j=0;
	
	// get the list of filenames in the current dir
	get_filenames_list(file_list);
	
	//printf("Filenames in validate_filename: ");
	// each while loop reads one parameter
	while(*ptr != '\0')
	{
		bzero(filename, sizeof(filename));
		// each for loop reads one char from paramater
		for(j = 0; *ptr != '\n' && *ptr != '\0'; ptr++, j++)
		{  
			filename[j] = *ptr;
		}
		filename[strlen(filename)] = '\0';
		//printf("%s ", filename);
		if (!strcmp(filename, request->filename))
			return 1;
		i++;
		ptr++;
	}
	
	return 0;
}


/*****************************************************************************\
* Function: sendall() borrowed from Beej's guide 7.3
*  http://beej.us/guide/bgnet/output/html/multipage/advanced.html
*
* Purpose:  This function sends message from the server to the client 
*                  by calling linux system call function write(). It  continues send
*					in a loop intil all the intended data has been sent
*
* Receives: 
*                fd: file descriptor  of socket
*                buf:  ptr to buffer contining the data to send
*                len:  ptr to length of  data in the buffer
*
* Returns:  return -1 on failure, 0 on succes
*                    return ptr to the length of bytes sent 
*
* Pre:     buf and len are non NULL ptr
*              
*
* Post:     All the data is sent via socket 
******************************************************************************/
int sendall(int fd, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = send(fd, buf+total, bytesleft, 0);
        if (n == -1) 
			break;
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
} 

 
/*****************************************************************************\
* Function: send_mess()
*
* Purpose:  This function sends message from the server to the client 
*                  by calling linux system call function write(). The message created by build_reply()
* Receives: 
*                sockfd: file descriptor of the socket that accepted connection from the client
*                server_name: const char pointer on address of the name of the server
*                that is hard-coded in main()
*
* Returns:  None
*
* Pre:      sockfd is a valid file descriptor of the socket that accepted connection from the client
*              server_name != NULL 
*
* Post:     None
******************************************************************************/
void send_mess(int sockfd, char* message){
    int length;
    //char reply[BUF_SIZE];
    //bzero(reply, BUF_SIZE);
    //printf("\nMESSAGE: %d bytes:\n%s\n", (int)strlen(message), message);
    //build_reply(reply, message);

	length = BUF_SIZE; //strlen(message);
	if (sendall(sockfd, message, &length) == -1) {
		perror("sendall()");
		printf("Sent %d bytes because of the error!\n", length);
	} 
        
}


/*****************************************************************************\
* Function: readall() borrowed from Beej's guide
*  http://beej.us/guide/bgnet/output/html/multipage/advanced.html
*
* Purpose:  This function sends message from the server to the client 
*                  by calling linux system call function write(). It  continues send
*					in a loop intil all the intended data has been sent
*
* Receives: 
*                fd: file descriptor  of socket
*                buf:  ptr to buffer contining the data to send
*                len:  ptr to length of  data in the buffer
*
* Returns:  return -1 on failure, 0 on succes
*                    return ptr to the length of bytes sent 
*
* Pre:     buf and len are non NULL ptr
*              
*
* Post:     All the data is sent via socket 
******************************************************************************/
int readall(int fd, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = read(fd, buf+total, bytesleft);
        if (n == -1) 
			break;
        total += n;
        bytesleft -= n;
    }
	 //num_read = read(sockfd, message, BUF_SIZE);

    *len = total; // return number actually sent here
	

    return n==-1?-1:0; // return -1 on failure, 0 on success
	
	
	/* insert  this in the parent function handleRequest()
	length = SMALL_BUF_SIZE;
	if (readall(sockfd, message, &length) == -1) {
		perror("readall");
		printf("We only read %d bytes because of the error!\n", length);
	} */
} 


/*****************************************************************************\
* Function: error()
*
* Purpose:  This function prints error message in stdout via perror() and exits with failure
*                  
* Receives: 
*                msg: const char pointer to address of the error message
*
* Returns:  None
*
* Pre:        msg != NULL
*               
*
* Post:     None
******************************************************************************/
void error(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}


/*
if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
    }
*/

