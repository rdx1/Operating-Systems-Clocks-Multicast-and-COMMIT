#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <pthread.h>
#include <cstdlib>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string>
#include <queue>

char sendBuffer[1024];
char recvBuffer[1024];
int portlist[3] = {9003, 9002, 9004};

using namespace std;

int vector_clock[4] = {0, 0, 0, 0}; // Initialize vector clock
uint64_t process_id = 0;

struct process_params
{
    uint64_t id;
    uint64_t port;
    char *message;
};

int print_clockstate(int process_id)
{
    cout << "[ ";
    for (int i; i < 4; ++i)
    {
        cout << vector_clock[i] << ", ";
    }
    cout << "]";
    return 0;
}
/* Here we check the condition for Rule 2, we need to check all other values of the vector clock contained in the received message from the sender
 * */
bool check_causality(int M[], int VC[], int sender_id, int array_size)
{
    bool result = true; // we'll assume it satisfies causality.
    for (int i = 0; i < array_size; i++)
    {
        if (i != sender_id)
        {
            if (M[i] > VC[i])
            { // if any of sender's VC values is NOT 'equal to/less than' host process's VC values, rule is broken, set to false.
                result = false;
                cout << "Causality is not satisfied! will have to buffer!" << endl;
            }
        }
    }
    return result;
}

void *start_server(void *parameters)
{
    process_params *params = (process_params *)parameters;
    uint64_t process_id = params->id;
    uint64_t port = params->port;
    char *sendersm;
    queue<string> bufferqueue;

    const int CONN_BACKLOG_NUM = 5;
    /* Initiate Server Parameters */
    struct sockaddr_in myAddr;
    memset(&myAddr, '\0', sizeof(struct sockaddr_in));

    myAddr.sin_family = AF_INET;
    myAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myAddr.sin_port = htons(port);
    while (1)
    {
        const int TRUE = 1;
        int sockfd = -1;
        int opt = TRUE;
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        //set master socket to allow multiple connections , this is just a good habit, it will work without this
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

        //inform user of socket number - used in send and receive commands
        printf("\nNew connection, socket fd is %d , ip is : %s , port : %d \n", sockfd, inet_ntoa(myAddr.sin_addr), ntohs(myAddr.sin_port));

        bind(sockfd, (struct sockaddr *)&myAddr, sizeof(myAddr));

        listen(sockfd, CONN_BACKLOG_NUM);

        // pass a ref to the processes clock to update later.
        printf("\t::Waiting for New Request data\n");

        /* The accept call blocks until a connection is found */
        int connfd = accept(sockfd, (struct sockaddr *)NULL, NULL);

        int numBytes = read(connfd, recvBuffer, sizeof(recvBuffer) - 1); // This is where server gets input from client

        char *sender = strtok(recvBuffer, " "); // Get the sending process's ID,
        cout << "Sender's Process ID: " << sender << endl;
        int sender_id = atoi(sender) - 1; // transform into array index format

        // msg[] is an array to store the sender's vector clock values
        int msg[100];
        int indexx = 0;
        char *senders_vclock = strtok(NULL, "-"); // Get Vector clock array values
        cout << "senders vectorclock:" << " " << '[';
        while (senders_vclock != NULL)
        {
            if (*senders_vclock >= 97 && *senders_vclock <= 122 || *senders_vclock >= 65 && *senders_vclock <= 90)
            {
                sendersm = senders_vclock;
                break;
            }
            else
            {

                cout << senders_vclock << ',';
                msg[indexx] = atoi(senders_vclock);
                senders_vclock = strtok(NULL, "-"); // point to next item in recvBuffer
                indexx++;
            }
            
        }
        cout << ']' << endl;
        cout << "User Message:" << sendersm << endl;
        if (msg[sender_id] == vector_clock[sender_id] + 1 && (check_causality(msg, vector_clock, sender_id, 4)))
        {
            vector_clock[sender_id] = msg[sender_id];
            cout << "Delivered" << endl;
        }
        else
        {
            bufferqueue.push(sendersm);
            cout << "Buffering.." << endl;
        }
        if (!bufferqueue.empty())
        {
            cout << "Buffer Contents are :";
            while (!bufferqueue.empty())
            {
                cout << bufferqueue.front() << " ";
                bufferqueue.pop();
            }
            cout << endl;
        }
        else
        {
            cout << "Buffer is empty" << endl;
        }
        close(sockfd);
    }
}
void *start_client(void *cparameters)
{
    process_params *params = (process_params *)cparameters;
    uint64_t process_id = params->id;
    uint64_t curr_port = params->port;
    char *clientmessage = params->message;

    int sendto = 0;
    cout << "I am the Process :" << process_id << endl;
    cout << "Enter 1 to send a multicast to other processes" << endl;
    cin >> sendto;
    /* Preparing parameters to send request to server. */
    if (sendto == 1)
    {
        for (int i = 0; i < 3; i++)
        {

            if (portlist[i] == curr_port)
            {
                continue;
            }
            else
            {
                memset(sendBuffer, '\0', sizeof(sendBuffer));

                sprintf(sendBuffer, "%lu", process_id); // value to be sent

                int indexx = 0;
                char temp[128];
                string vcstring = "";
                for (int i = 0; i < 4; ++i)
                {
                    vcstring.push_back('0' + vector_clock[i]);
                    if (i != 4 - 1)
                    {
                        vcstring.push_back('-');
                    }
                }
                char *cstring = new char[vcstring.length() + 1];
                std::strcpy(cstring, vcstring.c_str());

                strcat(sendBuffer, " ");
                strcat(sendBuffer, cstring);
                strcat(sendBuffer, "-");
                strcat(sendBuffer, clientmessage);

                if (portlist[i] == 9002 && curr_port == 9004)
                {
                    sleep(10); //delay process to show causality
                }

                struct sockaddr_in serverAddress;
                memset(&serverAddress, '\0', sizeof(struct sockaddr_in));

                serverAddress.sin_family = AF_INET;
                serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
                serverAddress.sin_port = htons(portlist[i]);

                const int TRUE = 1;
                int sockfd = -1;
                int opt = TRUE;
                sockfd = socket(AF_INET, SOCK_STREAM, 0);
                //set master socket to allow multiple connections , this is just a good habit, it will work without this
                setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
                memset(&serverAddress, '\0', sizeof(serverAddress));

                serverAddress.sin_family = AF_INET;
                serverAddress.sin_port = htons(portlist[i]);

                inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

                connect(sockfd, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
                write(sockfd, sendBuffer, strlen(sendBuffer)); // sending operation
                printf("Request: sent!\n");
            }
        }
    }
}

int main(int argc, char *argv[])
{
    process_id = atoi(argv[2]);
    char *usermessage = argv[3];

    struct process_params input_params;
    input_params.port = atoi(argv[1]);
    input_params.id = process_id;
    input_params.message = usermessage;

    vector_clock[process_id - 1] = vector_clock[process_id - 1] + 1;
    std::cout << " Process: " << process_id << " "
              << " clock is: ";
    print_clockstate(process_id);
    cout << "\n";

    pthread_t tid_server, tid_client;

    if (pthread_create(&tid_server, NULL, start_server, (void *)&input_params))
    {
        printf("Error in spawning server thread");
    }

    if (pthread_create(&tid_client, NULL, start_client, (void *)&input_params))
    {
        printf("Error in spawning client thread");
        exit(-1);
    }
    pthread_join(tid_server, NULL);
    pthread_join(tid_client, NULL);

    return 0;
}
