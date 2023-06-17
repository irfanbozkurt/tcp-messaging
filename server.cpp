
#include <unistd.h>
#include <netinet/in.h>
#include <time.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <queue>

#define BUFF_SIZE 257
#define SA struct sockaddr
#define TRUE 1

using namespace std;


// CONSTANTS
string
    PRE_REG_CODE = "3bbadb2b97b5a921e2647b0b3d3612713960b7866ee3599";

int
    listen_PORT = 1234,
    MAX_NUM_OF_CONNECTIONS = 10,
    MIN_NUMBER_OF_INP_TOKENS = 2,
    MAX_NUMBER_OF_INP_TOKENS = 3,
    MAX_NUMBER_OF_EMPTY_MESSAGES = 3,
    MAX_INACTIVE_TIME_PERIOD_SEC = 180;

float
    TIME_EMPTY_MESSAGE_DELTA = 1.0;

unordered_set<string> ALLOWED_MESSAGES;

// DYNAMIC GLOBAL VARIABLES
priority_queue<int, vector<int>, greater<int>> available_server_numbers;
map<int, thread> threads;

// EXPECTED MESSAGE TEMPLATES
string
    PORT_CHANGED =
        string("You provided a custom port. Make sure clients ") +
        string("use this port to connect you"),
    PRE_REG_MSG_TEMPLATE = 
        string("connection established %d"),
    REG_MSG_TEMPLATE = 
        string("welcome %s"),
    DATA_MSG_TEMPLATE = 
        string("%s ack %s"),
    CLOSE_MSG_TEMPLATE = 
        string("goodbye %s"),
    CLOSE_PREREG_MSG_TEMPLATE = 
        string("Connection attempt dropped for user id %d"),
    SERVER_READY = 
        string("Server awake. Ready to accept connections over port "),
    NEW_CLIENT = 
        string("Accepted new client"),
    CLIENT_CLOSED_CONNECTION =
        string("Seems like client closed connection. Freeing the ") +
        string("server so that new clients can use it");

// ERROR MESSAGE TEMPLATES
string
    ERR_CONNECTION_EXPIRED = 
        string("Client hasn't sent any messages for %d seconds. ") + 
        string("Connection terminated. User must reconnect ") +
        string("with another client instance.\n"),
    ERR_TIMEOUT_COULD_NOT_BE_SET =
        string("Timeout option could not be set to the accepted ") +
        string("connection. This client will be messaging the server ") +
        string("until they quit themselves\n"),
    ERR_CONNECTION_COULD_NOT_BE_ACCEPTED =
        string("FATAL: Server accept failed...\n"),
    ERR_LISTEN = 
        string("FATAL: Cannot listen to the socket\n"),
    ERR_SOCKET_CREATION =
        string("FATAL: Socket creation failed\n"),
    ERR_SOCKET_BIND = 
        string("FATAL: Socket bind failed. Is port in use?\n"),
    ERR_FIRST_WORD = 
        string("Only inputs starting with one of the following ") +
        string("words are accepted: connect, name, data, close"),
    ERR_WORD_COUNT_TOO_SMALL =
        string("Your input may not consist of less than %d ") + 
        string("space-seperated words"),
    ERR_WORD_COUNT_TOO_BIG =
        string("Your input may not consist of more than %d ") + 
        string("space-seperated words"),
    ERR_MAX_CONNECTIONS =
        string("Maximum %d connections are allowed"),
    ERR_INVALID_CONNECT_OR_CLOSE_STATEMENT = 
        string("A %s message would only be valid in the ") +
        string("following format: \"%s user_id\""),
    ERR_INVALID_NAME_OR_DATA_STATEMENT = 
        string("A %s message would only be valid in the ") +
        string("following format: \"%s user_id packet_id\""),
    ERR_ALREADY_REGISTERED =
        string("User %s is already registered with given id"),
    ERR_SEND_NAME_MESSAGE = 
        string("User with id %d sent a connect message before. ") +
        string("They may only proceed with a \"name\" message now"),
    ERR_USER_ALREADY_EXISTS = 
        string("Given user id is already taken by user %s "),
    ERR_INVALID_USER_ID = 
        string("Provided user id \"%s\" is not a number"),
    ERR_INVALID_PACKET_ID = 
        string("Provided packet id \"%s\" is not a number"),
    ERR_USER_ID_NOT_FOUND = 
        string("There's no user with given ID. Send a \"connect\" ") +
        string("message first");


void server_instance(int connfd, int server_no)
{
    unordered_map<int, string> users;
    int registered_count = 0;

    char buff[BUFF_SIZE];
    string buffStr;

    int empty_count = 0;
    time_t first_empty_message_time, now;

    // Set timeout to drop connection after inactive period
    struct timeval timeout;      
    timeout.tv_sec = MAX_INACTIVE_TIME_PERIOD_SEC;
    timeout.tv_usec = 0;
    if (
        setsockopt(
            connfd, SOL_SOCKET, SO_RCVTIMEO, 
            (char *)&timeout, sizeof(timeout)
        ) < 0
    )
        cout << ERR_TIMEOUT_COULD_NOT_BE_SET << endl;

    while(TRUE) {
        {
process:
        	// CLEAR THE BUFFER, READ, clear the buffer again
            bzero(buff, BUFF_SIZE);
            // Check if timeout is reached
            // if so, close the connection (end the thread)
            if (read(connfd, buff, sizeof(buff)) < 0) {
                sprintf(buff, &ERR_CONNECTION_EXPIRED[0], MAX_INACTIVE_TIME_PERIOD_SEC);
                cout << "\tTo client #" + to_string(server_no) + ": " + buff << endl;
                available_server_numbers.push(server_no);
                break;
            }
            buffStr = buff;
            bzero(buff, BUFF_SIZE);

            // client sending multiple empty messages in a 
            // short period = client aborted
            if(buffStr.empty()) {
                if(empty_count == 0)
                    time(&first_empty_message_time);
                time(&now);
                if(
                    empty_count >= MAX_NUMBER_OF_EMPTY_MESSAGES &&
                    difftime(now, first_empty_message_time) >= TIME_EMPTY_MESSAGE_DELTA
                ) {
                    cout << "\tTo client #" + to_string(server_no) + ": " + CLIENT_CLOSED_CONNECTION << endl;
                    available_server_numbers.push(server_no);
                    break;
                }
                empty_count++;
                goto process;
            }
            empty_count = 0;

            // CONSOLE-OUT THE INCOMING MESSAGE
            cout << "From client #" + to_string(server_no) + ": " + buffStr;

            // TOKENIZE INCOMING MESSAGE
            vector<string> words;
            {
                stringstream ss(buffStr);
                string word;
                while (ss >> word)
                    if (words.size() == MAX_NUMBER_OF_INP_TOKENS) {
                        sprintf(buff, &ERR_WORD_COUNT_TOO_BIG[0], MAX_NUMBER_OF_INP_TOKENS);
                        goto respond;
                    } else if (
                        words.size() == 1 &&
                        ALLOWED_MESSAGES.find(words[0]) == ALLOWED_MESSAGES.end()
                    ) {
                        strcpy(buff, &ERR_FIRST_WORD[0]);
                        goto respond;
                    } else
                        words.push_back(word);
                // FURTHER WORD COUNT VALIDATION
                if (words.size() < 2) {
                    sprintf(buff, &ERR_WORD_COUNT_TOO_SMALL[0], MIN_NUMBER_OF_INP_TOKENS);
                    goto respond;
                }
            }

            ////////////////////////////////////////
            // Handle different message types
            ////////////////////////////////////////
            // Retreive user id
            int user_id;
            try {
                user_id = stoi(words[1]);
            }
            catch(exception &err) {
                sprintf(buff, &ERR_INVALID_USER_ID[0], &words[1][0]);
                goto respond;
            }

            /***************************/
            ////////// connect  
            /***************************/
            if(words[0] == "connect") {
                // perform validation
                if (words.size() != 2) {
                    sprintf(buff, &ERR_INVALID_CONNECT_OR_CLOSE_STATEMENT[0], &words[0][0], &words[0][0]);
                    goto respond;
                }
                if (users.find(user_id) != users.end()) {
                    if (users[user_id] != PRE_REG_CODE)
                        sprintf(buff, &ERR_ALREADY_REGISTERED[0], &users[user_id][0]);
                    else
                        sprintf(buff, &ERR_SEND_NAME_MESSAGE[0], user_id);
                    goto respond;
                }
                if (registered_count == MAX_NUM_OF_CONNECTIONS) {
                    sprintf(buff, &ERR_MAX_CONNECTIONS[0], MAX_NUM_OF_CONNECTIONS);
                    goto respond;
                }

                // perform pre-registration
                users[user_id] = PRE_REG_CODE;

                // prepare buff
                sprintf(buff, &PRE_REG_MSG_TEMPLATE[0], user_id);
                goto respond;
            }

            // If not a connect message, user_id must exist in database
            if (users.find(user_id) == users.end()) {
                strcpy(buff, &ERR_USER_ID_NOT_FOUND[0]);
                goto respond;
            }

            /***************************/
            ////////// close  
            /***************************/
            if (words[0] == "close") {
                // perform validation
                if (words.size() != 2) {
                    sprintf(buff, &ERR_INVALID_CONNECT_OR_CLOSE_STATEMENT[0], &words[0][0], &words[0][0]);
                    goto respond;
                }

                // prepare buff
                // User did pre-registration but not registration
                if (users[user_id] == PRE_REG_CODE)
                    sprintf(buff, &CLOSE_PREREG_MSG_TEMPLATE[0], user_id);
                // User did registration
                else {
                    registered_count--;
                    sprintf(buff, &CLOSE_MSG_TEMPLATE[0], &users[user_id][0]);
                }

                // Symbolically close the connection
                users.erase(user_id);

                goto respond;
            }

            // name or data messages can only contain 3 words
            if (words.size() != 3) {
                sprintf(buff, &ERR_INVALID_NAME_OR_DATA_STATEMENT[0], &words[0][0], &words[0][0]);
                goto respond;
            }

            /***************************/
            ////////// name  
            /***************************/
            if (words[0] == "name") {
                // perform validation
                if (users[user_id] != PRE_REG_CODE) {
                    sprintf(buff, &ERR_USER_ALREADY_EXISTS[0], &users[user_id][0]);
                    goto respond;
                }

                if (registered_count == MAX_NUM_OF_CONNECTIONS) {
                    sprintf(buff, &ERR_MAX_CONNECTIONS[0], MAX_NUM_OF_CONNECTIONS);
                    goto respond;
                }

                // perform registration
                users[user_id] = words[2];
                registered_count++;

                // prepare buff
                sprintf(buff, &REG_MSG_TEMPLATE[0], &words[2][0]);
                goto respond;
            }

            /***************************/
            ////////// data  
            /***************************/
            if (words[0] == "data") {
                // perform validation
                if (users[user_id] == PRE_REG_CODE) {
                    sprintf(buff, &ERR_SEND_NAME_MESSAGE[0], user_id);
                    goto respond;
                }
                // is packet_id numerical?
                for (auto it = words[2].begin(); it != words[2].end(); it++)
                    if(!isdigit(*it)) {
                        sprintf(buff, &ERR_INVALID_PACKET_ID[0], &words[2][0]);
                        goto respond;
                    }

                // prepare buff
                sprintf(buff, &DATA_MSG_TEMPLATE[0], &users[user_id][0], &words[2][0]);
                goto respond;
            }
        }

        // SEND THE buff
respond:
        write(connfd, buff, sizeof(buff));
        cout << "\tTo client #" + to_string(server_no) + ": " + buff << endl;
    }

    close(connfd);
}

int main(int argc, char** argv)
{

    // Handle sysarg
    if (argc > 1) {
        int arg_num = 1;
        while (arg_num < argc) {
            string new_arg = argv[arg_num];
            if (new_arg == "-p") {
                listen_PORT = stoi(argv[arg_num + 1]);
                cout << PORT_CHANGED << endl;
            }
            arg_num += 2;
        }
    }

    int sockfd, newConn;
    socklen_t len;
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
    // IP representation(uint) from host bytes to network bytes
    // server might have multiple network interfaces
    // it must be collecting everything on given port
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    // PORT representation(short) from host bytes to network bytes
    servaddr.sin_port = htons(listen_PORT);
   
    // Bind server struct to the socket
    if (::bind(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        cout << ERR_SOCKET_BIND;
        exit(0);
    }

    // Test if port can be listened
    if (listen(sockfd, 5) != 0) {
        cout << ERR_LISTEN;
        exit(0);
    }

    // Define valid dictionary
    ALLOWED_MESSAGES.insert("data");
    ALLOWED_MESSAGES.insert("name");
    ALLOWED_MESSAGES.insert("connect");
    ALLOWED_MESSAGES.insert("close");

    cout << SERVER_READY << listen_PORT << endl;

    // Handle multiple clients
    int server_no;
    while (TRUE) {

        // accept incoming connection
        len = sizeof(cli);
        newConn = accept(sockfd, (SA*)&cli, &len);
        if (newConn < 0) {
            cout << ERR_CONNECTION_COULD_NOT_BE_ACCEPTED;
            continue;
        }

        // Determine server number
        if (available_server_numbers.empty())
            server_no = threads.size() + 1;
        else {
            server_no = available_server_numbers.top();
            available_server_numbers.pop();
        }

        threads[server_no] = thread(&server_instance, newConn, server_no);
        threads[server_no].detach();

        cout << "\tTo client #" + to_string(server_no) << ": " << NEW_CLIENT << endl;
    }

    close(sockfd);
}
