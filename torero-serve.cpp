
// standard C libraries
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

// operating system specific libraries
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

// C++ standard libraries
#include <vector>
#include <thread>
#include <string>
#include <iostream>
#include <system_error>
#include <filesystem>

// Bounded Buffer header file
#include "BoundedBuffer.hpp"

// Extra needed libraries
#include<fstream>
#include<sstream>
#include<regex>
#include<sys/types.h>
#include<dirent.h>



// shorten the std::filesystem namespace down to just fs
namespace fs = std::filesystem;

using std::cout;
using std::string;
using std::vector;
using std::thread;

using std::ostringstream;
using std::ifstream;
using std::stringstream;
using std::filesystem::path;
using std::filesystem::exists;
using std::filesystem::is_regular_file;
using std::filesystem::is_directory;

// This will limit how many clients can be waiting for a connection.
static const int BACKLOG = 10;

// forward declarations
int createSocketAndListen(const int port_num);
void acceptConnections(const int server_sock);
void handleClient(const int client_sock);
void sendData(int socked_fd, const char *data, size_t data_length);
int receiveData(int socked_fd, char *dest, size_t buff_size);
void sendBadRequest(int sock);
void addSocketToBuffer(int sock, BoundedBuffer &b);
void sendFileNotFound(int sock);
void send200Header(int sock, size_t fsize, path file);
void readAndSendFileData(int sock, path file);
void sendFile(int sock, path file);
void sendHTML(int sock, path file);
void consume(BoundedBuffer &buff);

int main(int argc, char** argv) {

	/* Make sure the user called our program correctly. */
	if (argc != 3) {
		// TODO: print a proper error message informing user of proper usage
		cout << "INCORRECT USAGE!\n" << "The correct usage is: ./torero-serve XXXX WWW where XXXX is the port no and WWW is the directory for the resources";
		exit(1);
	}

    /* Read the port number from the first command line argument. */
    int port = std::stoi(argv[1]);

	/* Create a socket and start listening for new connections on the
	 * specified port. */
	int server_sock = createSocketAndListen(port);

	/* Now let's start accepting connections. */
	acceptConnections(server_sock);

    close(server_sock);

	return 0;
}

/**
 * Sends message over given socket, raising an exception if there was a problem
 * sending.
 *
 * @param socket_fd The socket to send data over.
 * @param data The data to send.
 * @param data_length Number of bytes of data to send.
 */
void sendData(int socked_fd, const char *data, size_t data_length) {
	// TODO: Wrap the following code in a loop so that it keeps sending until
	// the data has been completely sent.
	int num_bytes_sent;
	size_t counter = 0;

	while(counter != data_length){
		num_bytes_sent = send(socked_fd, data, data_length, 0);
		counter += num_bytes_sent;
		if (num_bytes_sent == -1) {
			std::error_code ec(errno, std::generic_category());
			throw std::system_error(ec, "send failed");
		}
	}
}

/**
 * Receives message over given socket, raising an exception if there was an
 * error in receiving.
 *
 * @param socket_fd The socket to send data over.
 * @param dest The buffer where we will store the received data.
 * @param buff_size Number of bytes in the buffer.
 * @return The number of bytes received and written to the destination buffer.
 */
int receiveData(int socked_fd, char *dest, size_t buff_size) {
	int num_bytes_received = recv(socked_fd, dest, buff_size, 0);
	if (num_bytes_received == -1) {
		std::error_code ec(errno, std::generic_category());
		throw std::system_error(ec, "recv failed");
	}

	return num_bytes_received;
}

/**
 * Sends error 400 Bad Request response
 *
 * @param sock The socket to send data over
 */
void sendBadRequest(int sock) {
	string badRequest("HTTP/1.0 400 BAD REQUEST\r\n\r\n");
	sendData(sock, badRequest.c_str(), badRequest.length());
}

/**
 * Sends error 404 File Not Found reponse
 *
 * @param sock The socket to send data over
 */
void sendFileNotFound(int sock) {
	string fileNotFound = "HTTP/1.0 404 FILE NOT FOUND\nContent-length: 109\nContent-type: text/html\n\n<html><head><title>Ruh-roh! Page not found!</title></head><body>404 Page Not Found! :'( :'( :'(</body></html>";
	sendData(sock, fileNotFound.c_str(), fileNotFound.length());
}

/**
 * Sends the header of the response for a given file
 * Sets the Content type and length based on the file to be sent
 *
 * @param sock The socket to send data over
 * @param fize The size of the file to be sent
 * @param file The path of the file to be sent
 */
void send200Header(int sock, size_t fsize, path file) {
	string result = "HTTP/1.0 200 OK\nContent-Length: ";
	result += std::to_string(fsize);
	result += "\nContent-Type: ";

	// Append Content-Type based on the type of file	
	if(file.extension() == ".html") {
		result += "text/html\n\n";
	} else if (file.extension() == ".jpg") {
		result += "image/jpeg\n\n";
	} else if (file.extension() == ".txt") {
		result += "text/plain\n\n";
	} else if (file.extension() == ".pdf") {
		result += "application/pdf\n\n";
	} else if (file.extension() == ".png") {
		result += "image/png\n\n";
	} else if (file.extension() == ".gif") {
		result += "image/gif\n\n";
	}
	else if (file.extension() == ".css") {
		result += "text/css\n\n";
	} else {
		cout << "ERROR: The requested file extension is not supported" << "\n";
	}
	
	sendData(sock, result.c_str(), result.length());		
}

/**
 * Sends the file to SendData function to send the data
 *
 * @param sock The socket to send data over
 * @param file The path of the file to be sent
 */
void readAndSendFileData(int sock, path file){
	ifstream inFile;
    inFile.open(file);
    stringstream strStream;
    strStream << inFile.rdbuf();
    string str = strStream.str();
 	sendData(sock, str.c_str(), str.length());
}

/**
 * Main function that uses the two helper functions to send the file
 * header and data
 *
 * @param sock The socket to send data over
 * @param File the path of the file to be sent
 */
void sendFile(int sock, path file) {
	size_t fsize = fs::file_size(file);
	send200Header(sock, fsize, file);
	readAndSendFileData(sock, file);		
}

/**
 * Sends generated HTML file when requested file is a directory w/o an
 * index.html file
 *
 * @param sock The socket to send data over
 * @param file The path of the file to be sent
 */
void sendHTML(int sock, path file){
	string baseHTML = "<html>\n<body>\n<ul>\n";
	string tempHTML;
	
	// Vector to hold the names of the files in the given directory
	vector<string> filenames;
	string tempName = file;
	
	// Searches the given directory and adds all files and directories to the
	// vector
	for (auto& entry: fs::directory_iterator(tempName)) {
		if (is_directory(entry.path())) {
			string directory_name = entry.path().filename();
			directory_name += "/";
			filenames.push_back(directory_name);
		}
		else {
			filenames.push_back(entry.path().filename());
		}
	}	

	// Adds generated links to HTML from the vector
	for(string s : filenames){
		tempHTML = "<li><a href=\"" + s + "\">" + s + "</a></li>\n";
		baseHTML += tempHTML;
	}

	// Adds rest of generated HTML and sends header and data	
	baseHTML += "</ul>\n</body>\n</html>";
	size_t baseLength = baseHTML.length();
	string header = "HTTP/1.0 200 OK\nContent-Length: ";
	header += std::to_string(baseLength);
	header += "\nContent-Type: text/html\n\n";
	sendData(sock, header.c_str(), header.length());
	sendData(sock, baseHTML.c_str(), baseHTML.length());
}


/**
 * Receives a request from a connected HTTP client and sends back the
 * appropriate response.
 *
 * @note After this function returns, client_sock will have been closed (i.e.
 * may not be used again).
 *
 * @param client_sock The client's socket file descriptor.
 */
void handleClient(const int client_sock) {
	// Step 1: Receive the request message from the client
	char received_data[2048];
	int bytes_received = receiveData(client_sock, received_data, 2048);

	// Turn the char array into a C++ string for easier processing.
	string request_string(received_data, bytes_received);

	// Grab URL of request message	
	std::istringstream f(request_string);
	string url;
	getline(f, url, ' ');
	getline(f, url, ' ');

	string fullPath = "./WWW";
	fullPath += url;
	path urlFullPath = fullPath;
	// Checks if URL from GET request exists
	if (exists(urlFullPath)) {
		// Case: URL is a file
		if(fs::is_regular_file(urlFullPath)) {
			sendFile(client_sock, fullPath);	
		}
		// Case: URL is a directory
		else {
			// Case: Directory has index.html in it
			string temp = fullPath;
			temp += "index.html";
			path dirPath = temp;
			if(std::filesystem::is_regular_file(dirPath)) {
				sendFile(client_sock, temp);
			}
			// Case: Directory that exists w/o index.html
			else {
				sendHTML(client_sock, fullPath);
			}
		}
	}
	// File does not exist
	else {
		sendFileNotFound(client_sock);
	}
	
	// Close client socket 	
	close(client_sock);
}

/**
 * Creates a new socket and starts listening on that socket for new
 * connections.
 *
 * @param port_num The port number on which to listen for connections.
 * @returns The socket file descriptor
 */
int createSocketAndListen(const int port_num) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Creating socket failed");
        exit(1);
    }

    /* 
	 * A server socket is bound to a port, which it will listen on for incoming
     * connections.  By default, when a bound socket is closed, the OS waits a
     * couple of minutes before allowing the port to be re-used.  This is
     * inconvenient when you're developing an application, since it means that
     * you have to wait a minute or two after you run to try things again, so
     * we can disable the wait time by setting a socket option called
     * SO_REUSEADDR, which tells the OS that we want to be able to immediately
     * re-bind to that same port. See the socket(7) man page ("man 7 socket")
     * and setsockopt(2) pages for more details about socket options.
	 */
    int reuse_true = 1;

	int retval; // for checking return values

    retval = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse_true,
                        sizeof(reuse_true));

    if (retval < 0) {
        perror("Setting socket option failed");
        exit(1);
    }

    /*
	 * Create an address structure.  This is very similar to what we saw on the
     * client side, only this time, we're not telling the OS where to connect,
     * we're telling it to bind to a particular address and port to receive
     * incoming connections.  Like the client side, we must use htons() to put
     * the port number in network byte order.  When specifying the IP address,
     * we use a special constant, INADDR_ANY, which tells the OS to bind to all
     * of the system's addresses.  If your machine has multiple network
     * interfaces, and you only wanted to accept connections from one of them,
     * you could supply the address of the interface you wanted to use here.
	 */
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_num);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* 
	 * As its name implies, this system call asks the OS to bind the socket to
     * address and port specified above.
	 */
    retval = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (retval < 0) {
        perror("Error binding to port");
        exit(1);
    }

    /* 
	 * Now that we've bound to an address and port, we tell the OS that we're
     * ready to start listening for client connections. This effectively
	 * activates the server socket. BACKLOG (a global constant defined above)
	 * tells the OS how much space to reserve for incoming connections that have
	 * not yet been accepted.
	 */
    retval = listen(sock, BACKLOG);
    if (retval < 0) {
        perror("Error listening for connections");
        exit(1);
    }

	return sock;
}

/**
 * Sit around forever accepting new connections from client.
 *
 * @param server_sock The socket used by the server.
 */
void acceptConnections(const int server_sock) {
	// Create shared buffer for threads to access
	const size_t buff_size = 8;
    BoundedBuffer buff(buff_size);

	// Create multiple consumer threads to read from shared buffer
	for (size_t i = 0; i< 8; i++) {
		std::thread consumer(consume, std::ref(buff));
		consumer.detach();
	}
	while (true) {
        // Declare a socket for the client connection.
        int sock;

        /* 
		 * Another address structure.  This time, the system will automatically
         * fill it in, when we accept a connection, to tell us where the
         * connection came from.
		 */
        struct sockaddr_in remote_addr;
        unsigned int socklen = sizeof(remote_addr); 

        /* 
		 * Accept the first waiting connection from the server socket and
         * populate the address information.  The result (sock) is a socket
         * descriptor for the conversation with the newly connected client.  If
         * there are no pending connections in the back log, this function will
         * block indefinitely while waiting for a client connection to be made.
         */
        sock = accept(server_sock, (struct sockaddr*) &remote_addr, &socklen);
        if (sock < 0) {
            perror("Error accepting connection");
            exit(1);
        }

        /* 
		 * At this point, you have a connected socket (named sock) that you can
         * use to send() and recv(). The handleClient function should handle all
		 * of the sending and receiving to/from the client.
		 *
		 * TODO: You shouldn't call handleClient directly here. Instead it
		 * should be called from a separate thread. You'll just need to put sock
		 * in a shared buffer that is synchronized using condition variables.
		 * You'll implement this shared buffer in one of the labs and can use
		 * it directly here.
		 */
				
		buff.putItem(sock);
	}
}

/**
 * Consumer method which waits until there is a socket fd in the shared
 * buffer, then grabs that fd and sends it to handleClient
 *
 * @param &buffer The shared buffer full of socket fd's
 */
void consume(BoundedBuffer &buffer) {
	while(true) {
		int fd = buffer.getItem();
		handleClient(fd);		
	}
}
