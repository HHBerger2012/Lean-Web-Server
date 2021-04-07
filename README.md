# Lean-Web-Server
A lean web server written in C++

- This program takes two arguments
  - The port number on which to bind and listen for connections
  - The directory out of which to serve files. (WWW)
 
- To start the web server, use the Makefile by typing 'Make' into your terminal
- This will create a file called torero-serve
- Example run: "$ ./torero-serve 7105 WWW"

- At this point, you will be able to navigate to the web server through your browser 
- The multithreading allows for multiple concurrent users 
