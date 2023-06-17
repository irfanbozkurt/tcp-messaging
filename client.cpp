#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <time.h>

#define BUFF_SIZE 257
#define SA struct sockaddr
#define TRUE 1

using namespace std;


// CONSTANTS
string
    dest_IP = "127.0.0.1";

int
    dest_PORT = 1234;

// MESSAGE TEMPLATES
string
    CONNECTING =
        string("Connecting..."),
    EXIT = 
        string("Exiting client\n"),
    ERR_SOCKET_CREATION =
        string("FATAL: Socket creation failed"),
    ERR_CONNECTION_FAILED =
        string("FATAL: Connection with the server failed"),
    ERR_SERVER_DOWN =
        string("FATAL: Server down. Aborting"),
    CONNECTION_SUCCESS =
        string("Successful TCP connection over "),
    IP_CHANGED = 
        string("You provided a custom IP address. Make sure the ") +
        string("server runs on this IP address"),
    PORT_CHANGED = 
        string("You provided a custom port. Make sure the server ") +
        string("runs on the port number you provided");


void client_instance(int sockfd)
{
    char buff[BUFF_SIZE];
    int n;

    while(TRUE) {
    	// zero-out the buffer
        bzero(buff, sizeof(buff));

        // write message
        n = 0;
        while ((buff[n++] = getchar()) != '\n');

        // Want exit?
        if (strncmp(buff, "exit", 4) == 0) {
            cout << EXIT;
            break;
        }

        // send the message
        write(sockfd, buff, sizeof(buff));

		// zero-out the buffer & read incoming message
        bzero(buff, sizeof(buff));
        read(sockfd, buff, sizeof(buff));


        if (buff[0] == '\0') {
            cout << ERR_SERVER_DOWN << endl;
            break;
        }
        // CONTENT OF INCOMING MESSAGE
        cout << "\t" << buff << endl;
    }
}

int main(int argc, char** argv)
{
    // Handle sysarg
    if (argc > 1) {
        int arg_num = 1;
        while (arg_num < argc) {
            string new_arg = argv[arg_num];
            if (new_arg == "-p") {
                dest_PORT = stoi(argv[arg_num + 1]);
                cout << PORT_CHANGED << endl;
            } else if (new_arg == "-ip") {
                dest_IP = argv[arg_num + 1];
                cout << IP_CHANGED << endl;
            }
            arg_num += 2;
        }
    }

    int sockfd;
    struct sockaddr_in servaddr, cli;
   
    // create a "listening" socket on TCP
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        cout << ERR_SOCKET_CREATION;
        exit(0);
    }

    // zero-out the server struct
    bzero(&servaddr, sizeof(servaddr));
   
    // IP:port information in server struct
    // We're interested in IPv4
    servaddr.sin_family = AF_INET;
    // server's IP representation(uint) from string to decimal
    servaddr.sin_addr.s_addr = inet_addr(&dest_IP[0]);
    // PORT representation(short) from host bytes to network bytes
    servaddr.sin_port = htons(dest_PORT);
    
    // connect to server's open socket
    cout << CONNECTING << endl;
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        cout << ERR_CONNECTION_FAILED;
        exit(0);
    }
    cout << CONNECTION_SUCCESS << dest_IP << ":" << dest_PORT << endl;

    client_instance(sockfd);

    close(sockfd);
}
