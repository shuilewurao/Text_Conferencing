#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

//#define PORT "9034"   // port we're listening on

// get sockaddr, IPv4 or IPv6:

#define MAX_NAME    64
#define MAX_DATA    1024

#define LOGIN       1
#define LO_ACK      2
#define LO_NAK      3
#define EXIT        4
#define JOIN        5
#define JN_ACK      6
#define JN_NAK      7
#define LEAVE_SESS  8
#define NEW_SESS    9
#define NS_ACK      10
#define MESSAGE     11
#define QUERY       12
#define QU_ACK      13

//section 2
#define INVITE      14
#define IV_ACK      15
#define IV_NAK      16

typedef struct lab3message
{
    unsigned int type; //type of the message
    unsigned int size; //length of the data
    unsigned char source[MAX_NAME]; //ID of the client
    unsigned char data[MAX_DATA];
} message_packet;

typedef struct client_list
{
    int socket;
    char username[MAX_NAME];
    char password[MAX_NAME];
    bool isconnected;
    bool isinsession;
    char sessionname[MAX_NAME];
} Client;

typedef struct session_list
{
    char sessionname[MAX_NAME];
    unsigned int usernum;
} Session;

enum type
{
    CLIENT, SESSION
};

struct list
{
    void* data;
    struct list* next;
};

struct list* prepend(struct list* head, void* nd);
struct list* traverse(struct list* head, char* buf, enum type t);
struct list* remove_any(struct list* head, struct list* nd);


void login(int _socket, message_packet* buf);
void create_session(int _socket, message_packet* buf);
void join_session(int _socket, message_packet* buf);
void leave_session(int _socket, message_packet* buf);
void logout(int _socket, message_packet* buf);
void invite(int _socket, message_packet* buf);
void inviteack(message_packet* buf);
void invitenak(message_packet* buf);

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

struct list* client_head;
struct list* session_head;

int main(int narg, char** args)
{
    printf("Start Server - Welcome to iFaQ\n");
    client_head = NULL;
    session_head = NULL;
    int port = atoi(args[1]);
    fd_set master; // master file descriptor list
    fd_set read_fds; // temp file descriptor list for select()
    int fdmax; // maximum file descriptor number

    int listener; // listening socket descriptor
    int newfd; // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    message_packet buf; // buffer for client data
    int nbytes;

    char remoteIP[INET_ADDRSTRLEN];

    int yes = 1; // for setsockopt() SO_REUSEADDR, below
    int i, j, rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master); // clear the master and temp sets
    FD_ZERO(&read_fds);

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, args[1], &hints, &ai)) != 0)
    {
        fprintf(stderr, "SERVER: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = ai; p != NULL; p = p->ai_next)
    {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0)
        {
            continue;
        }

        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0)
        {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL)
    {
        fprintf(stderr, "SERVER: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1)
    {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // main loop
    while (1)
    {
        read_fds = master; // copy it
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for (i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &read_fds))
            { // we got one!!
                if (i == listener)
                {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                            (struct sockaddr *) &remoteaddr,
                            &addrlen);

                    if (newfd == -1)
                    {
                        perror("accept");
                    }
                    else
                    {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax)
                        { // keep track of the max
                            fdmax = newfd;
                        }
                        printf("SERVER: new connection from %s on "
                                "socket %d\n",
                                inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*) &remoteaddr),
                                remoteIP, INET_ADDRSTRLEN),
                                newfd);
                    }
                }
                else
                {
                    // handle data from a client
                    if ((nbytes = recv(i, &buf, sizeof (message_packet), 0)) <= 0)
                    {
                        // got error or connection closed by client
                        if (nbytes == 0)
                        {
                            // connection closed
                            printf("\n-----\nFOURCE QUIT\n");
                            printf("SERVER: socket %d hung up\n", i);
                            logout(i, NULL);
                            close(i); // bye!
                            FD_CLR(i, &master); // remove from master set
                            // nbytes = 1;
                        }
                        else
                        {
                            perror("recv");
                        }

                    }
                    else
                    {
                        // we got some data from a client
                        //printf("packet type %d\n", buf.type);
                        printf("client: %s\n", buf.source);
                        switch (buf.type)
                        {
                            case LOGIN:
                                printf("LOGIN\n");
                                login(i, &buf);
                                //printf("SERVER:client %d connected - send packet with type: %d\n", i, buf.type);
                                if (buf.type == LO_NAK)
                                {
                                    close(i); // bye!
                                    FD_CLR(i, &master); // remove from master set
                                }
                                send(i, &buf, sizeof (message_packet), 0);
                                break;
                            case EXIT:
                                printf("LOGOUT 886 %s\n", buf.source);
                                logout(i, &buf);
                                close(i); // bye!
                                FD_CLR(i, &master); // remove from master set
                                break;
                            case JOIN:
                                printf("JOIN session name: %s\n", buf.data);
                                join_session(i, &buf);
                                //printf("send packet with type: %d\n", buf.type);
                                send(i, &buf, sizeof (message_packet), 0);
                                break;
                            case LEAVE_SESS:
                                printf("LEAVE_SESS session name: %s\n", buf.data);
                                leave_session(i, &buf);
                                break;
                            case NEW_SESS:
                                printf("NEW_SESS session name: %s\n", buf.data);
                                create_session(i, &buf);
                                //printf("send packet with type: %d\n", buf.type);
                                send(i, &buf, sizeof (message_packet), 0);
                                break;
                            case MESSAGE:
                                printf("MESSAGE: %s", buf.data);
                                Client* tempclient = traverse(client_head, buf.source, CLIENT)->data;
                                struct list* cur = client_head;
                                while (cur)
                                {
                                    if (strcmp(((Client*) cur->data)->sessionname, tempclient->sessionname) == 0)
                                    {
                                        // send to everyone!
                                        int j = ((Client*) cur->data)->socket;
                                        if (FD_ISSET(j, &master))
                                        {
                                            // except the listener and ourselves
                                            if (j != listener && j != i)
                                            {
                                                printf("send %sto %d\n", buf.data, j);
                                                if (send(j, &buf, sizeof (message_packet), 0) == -1)
                                                {
                                                    perror("send");
                                                }
                                            }
                                        }
                                    }
                                    cur = cur->next;
                                }
                                break;
                            case QUERY:
                                printf("QUERY\n");
                                query(i, &buf);
                                send(i, &buf, sizeof (message_packet), 0);
                                break;
                            case INVITE:
                                printf("INVITE\n");
                                invite(i, &buf);
                                break;
                            case IV_ACK:
                                printf("IV_ACK\n");
                                inviteack(&buf);
                                break;
                            case IV_NAK:
                                printf("IV_NAK\n");
                                invitenak(&buf);
                                break;
                            default:
                                printf("UNKNOWN COMMAND: %d \n", (int) buf.type);
                                break;
                        }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    return 0;
}

struct list* prepend(struct list* head, void* nd)
{
    struct list* temp = head;
    head = malloc(sizeof (struct list));
    head->next = temp;
    head->data = nd;
    return head;
}

struct list* traverse(struct list* head, char* buf, enum type t)
{
    struct list* cur = head;
    if (t == CLIENT)
    {
        while (cur != NULL)
        {
            if (strcmp(buf, ((struct client_list*) cur->data)->username) == 0)
            {
                return cur;
            }
            cur = cur->next;
        }
    }
    else
    {
        while (cur != NULL)
        {
            if (strcmp(buf, ((struct session_list*) cur->data)->sessionname) == 0)
            {
                return cur;
            }
            cur = cur->next;
        }
    }
    return NULL;
}

struct list* remove_any(struct list* head, struct list* nd)
{
    //if no element exist
    if (head == NULL || nd == NULL)
    {
        printf("Nothing to be deleted\n");
        return NULL;
    }
    // if only one element
    struct list* cur = head->next;

    if (head == nd)
    {
        head = head->next;
        free(nd->data);
        free(nd);
        return head;
    }

    // if more than 1 element
    struct list* pre = head;
    while (cur)
    {
        if (cur == nd)
        {
            pre->next = cur->next;
            free(nd->data);
            free(nd);
            return head;
        }
        pre = cur;
        cur = cur->next;
    }
    return head;
}

void login(int _socket, message_packet* buf)
{
    struct list* client = traverse(client_head, buf->source, CLIENT);
    if (client)
    {
        buf->type = LO_NAK;
        return;
    }
    // add client;
    Client* tempClient = malloc(sizeof (Client));
    tempClient->isconnected = 1;
    tempClient->isinsession = 0;
    tempClient->socket = _socket;
    strcpy(tempClient->username, buf->source);
    int len = buf->size - strlen(buf->source);
    int i;
    int count = 0;
    for (i = len; i < buf->size; ++i)
    {
        tempClient->password[count] = buf->data[i];
        count++;
    }
    client_head = prepend(client_head, tempClient);
    buf->type = LO_ACK;
}

void create_session(int _socket, message_packet* buf)
{
    struct list* client = traverse(client_head, buf->source, CLIENT);
    // client is logged in
    if (client != NULL)
    {
        Client* tempclient = client->data;
        struct list* session = traverse(session_head, buf->data, SESSION);
        // session exists
        if (session != NULL)
        {
            printf("session already exits\n");
            buf->type = 0;
            strcpy(buf->data, "Server: Session exists. \n");
            buf->size = strlen(buf->data);
        }
        else
        {
            Session* tempSession = malloc(sizeof (Session));
            strcpy(tempSession->sessionname, buf->data);
            tempSession->usernum = 0;
            session_head = prepend(session_head, tempSession);
            buf->type = NS_ACK;
            strcpy(buf->data, "Successfully created session.\n");
            buf->size = strlen(buf->data);
        }
    }
    else
    {
        printf("client not exits\n");
        buf->type = 0;
        strcpy(buf->data, "Server: Command unavailable (create).\n");
        buf->size = strlen(buf->data);
    }
}

void join_session(int _socket, message_packet* buf)
{
    struct list* client = traverse(client_head, buf->source, CLIENT);
    if (client != NULL)
    {
        Client* tempClient = client->data;
        struct list* session = traverse(session_head, buf->data, SESSION);
        if (session != NULL)
        {
            tempClient->isinsession = 1;
            strcpy(tempClient->sessionname, buf->data);
            ((Session*) session->data)->usernum++;
            buf->type = JN_ACK;
        }
        else
        {
            buf->type = JN_NAK;
            strcpy(buf->data, "Server: Session does not exist.\n");
            buf->size = strlen(buf->data);
        }
    }
    else
    {
        buf->type = 0;
        strcpy(buf->data, "Server: Command unavailable (join). \n");
        buf->size = strlen(buf->data);
    }
}

void leave_session(int _socket, message_packet* buf)
{
    /*
     User is logged on
     */
    struct list* client = traverse(client_head, buf->source, CLIENT);
    if (client)
    {
        Client* tempClient = client->data;
        if (tempClient->isinsession)
        {
            struct list* session = traverse(session_head, tempClient->sessionname, SESSION);
            Session* tempSession = session->data;

            tempClient->isinsession = 0;
            strcpy(tempClient->sessionname, "");
            tempSession->usernum--;

            if (tempSession->usernum == 0)
            {
                session_head = remove_any(session_head, session);
            }
            strcpy(buf->data, "Successfully left session. \n");
        }
        else
        {
            buf->type = 0;
            strcpy(buf->data, "Server: Command unavailable. \n");
            buf->size = strlen(buf->data);
        }
    }
    else
    {
        buf->type = 0;
        strcpy(buf->data, "Server: Command unavailable (leave). \n");
        buf->size = strlen(buf->data);
    }
}

void logout(int _socket, message_packet* buf)
{
    if (!buf)
    {
        struct list* cur = client_head;

        while (cur)
        {
            if (((Client*) cur->data)->socket == _socket)
            {
                break;
            }
            cur = cur->next;
        }
        message_packet tbuf;
        strcpy(tbuf.source, ((Client*) cur->data)->username);
        client_head = remove_any(client_head, cur);
        leave_session(_socket, &tbuf);
        return;
    }

    struct list* client = traverse(client_head, buf->source, CLIENT);
    if (client)
    {
        client_head = remove_any(client_head, client);
        leave_session(_socket, buf);
    }
    else
    {
        buf->type = 0;
        strcpy(buf->data, "Server: Command unavailable (logout). \n");
        buf->size = strlen(buf->data);
    }
}

void query(int _socket, message_packet* buf)
{
    char result[MAX_DATA];
    strcpy(result, "");
    struct list* cursor = client_head;
    Client* tempclient;
    while (cursor)
    {
        tempclient = cursor->data;
        strcat(result, tempclient->username);
        if (tempclient->isinsession)
        {
            strcat(result, " - ");
            strcat(result, tempclient->sessionname);
        }
        strcat(result, "\n");
        cursor = cursor->next;
    }

    buf->type = QU_ACK;
    strcpy(buf->data, result);
    buf->size = strlen(result);
}

void invite(int _socket, message_packet* buf)
{
    struct list* sender = traverse(client_head, buf->source, CLIENT);

    if (sender)
    {

        struct list* temp = traverse(client_head, buf->data, CLIENT);

        // invitee exists
        if (temp != NULL)
        {
            Client* invitee = temp->data;

            // invitee is already in session
            if (invitee->isinsession)
            {
                // reply back to initial sender
                buf->type = 0;
                strcpy(buf->data, "\n   Server: Client is already in session. \n");
                buf->size = strlen(buf->data);
                send(_socket, buf, sizeof (message_packet), 0);
            }
            else
            {
                // send to invitee
                //printf("--- buf data: %s\n", buf->data);
                send(invitee->socket, buf, sizeof (message_packet), 0);
                printf("server invite sent \n");

            }
        }
        else
        {
            buf->type = 0;
            strcpy(buf->data, "\n   Server: Invitee does not exist. \n");
            buf->size = strlen(buf->data);
            send(_socket, buf, sizeof (message_packet), 0);
        }
    }
    else
    {
        buf->type = 0;
        strcpy(buf->data, "Server: Command unavailable (invite). \n");
        buf->size = strlen(buf->data);
    }
}

void inviteack(message_packet* buf)
{
    /*
        printf("   data: %s \n", buf->data);
        printf("\n   invitee: %s \n", buf->data);
        printf("\n   sender: %s \n", buf->source);
     */
    struct list* client = traverse(client_head, buf->source, CLIENT);

    if (client)
    {
        Client* sender = client->data;

        strcpy(buf->source, buf->data);
        strcpy(buf->data, sender->sessionname);

        struct list* client = traverse(client_head, buf->source, CLIENT);
        Client* tempClient = client->data;
        struct list* session = traverse(session_head, buf->data, SESSION);
        if (session != NULL)
        {
            tempClient->isinsession = 1;
            strcpy(tempClient->sessionname, buf->data);
            ((Session*) session->data)->usernum++;
        }


        int s = sender->socket;
        send(s, buf, sizeof (message_packet), 0);
    }
}

void invitenak(message_packet* buf)
{
    struct list* client = traverse(client_head, buf->source, CLIENT);

    if (client)
    {
        Client* sender = client->data;
        strcpy(buf->source, buf->data);
        strcpy(buf->data, sender->sessionname);

        join_session(NULL, buf);

        int s = sender->socket;
        send(s, buf, sizeof (message_packet), 0);
    }
}

// /login u1 123 127.0.0.1 5000
// /login u2 123 127.0.0.1 5000
