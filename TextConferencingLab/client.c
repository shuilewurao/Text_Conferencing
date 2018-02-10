/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.c
 * Author: chenxu23
 *
 * Created on November 6, 2017, 3:33 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>

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

bool check_is_connected(bool input);
int string_to_int(char* input);

/*
 * 
 */
int main(int argc, char** argv)
{
    struct sockaddr_in si_me, si_other;
    int s = -1;
    int i;
    int slen = sizeof (si_other);
    message_packet* buf = malloc(sizeof (message_packet));
    int n, m;
    fd_set fds;

    struct addrinfo hints, *me, *ptr;
    int readval;
    int port;
    char username[MAX_NAME];
    char password[MAX_NAME];
    bool isconnected = 0;
    bool isinsession = 0;
    char sessionname[MAX_NAME];

    printf("Starting Client \n");
    printf("Command Table: \n");
    printf("1. /login <client-ID> <password> <server-IP> <server-port> \n");
    printf("2. /logout \n");
    printf("3. /joinsession <session ID>\n");
    printf("4. /leavesession \n");
    printf("5. /createsession \n");
    printf("6. /list \n"); //get the list of the connected clients and available sessions
    printf("7. /quit \n");
    printf("8. /invite <client-ID> \n");
    printf("9. <text> \n");
    printf("\n");

    while (1)
    {
        FD_ZERO(&fds);
        FD_SET(fileno(stdin), &fds);

        if (s > 0)
        {
            FD_SET(s, &fds);
            select(s + 1, &fds, NULL, NULL, NULL);
        }
        else
        {
            select(fileno(stdin) + 1, &fds, NULL, NULL, NULL);
        }
        // received packet from server
        if (isconnected && FD_ISSET(s, &fds))
        {
            printf("RECEIVED NEW MESSAGE: \n");
            message_packet* recvbuf = malloc(sizeof (message_packet));
            m = recv(s, recvbuf, sizeof (message_packet), 0);

            if (m == 0)
            {
                printf("Server disconnected\n");
                close(s);
                FD_CLR(s, &fds);
                isconnected = 0;
            }
            else
            {
                //printf("--- type: %d \n", (int) recvbuf->type);

                if (recvbuf->type == INVITE)
                {
                    buf->type = recvbuf->type;
                    strcpy(buf->data, recvbuf->data);
                    strcpy(buf->source, recvbuf->source);
                    buf->size = recvbuf->size;

                    printf("Invitation from %s. \n\n", buf->source);
                    printf("1. /accept \n");
                    printf("2. /decline \n");
                    continue;
                }
                else if (recvbuf->type == IV_ACK)
                {
                    printf("Server: Successfully invited client %s. \n\n", buf->data);
                    continue;
                }
                else if (recvbuf->type == IV_NAK)
                {
                    printf("Server: Invitee declined join request. \n\n");
                    continue;
                }
                else if (recvbuf->type == 0)
                {
                    //printf("\n   %s. \n\n", recvbuf->data);
                    continue;
                }
                else
                {
                    printf("%s: ", recvbuf->source);
                    printf("%s \n\n", recvbuf->data);
                    continue;
                }
            }
            continue;
        }
        // received input from user
        else if (FD_ISSET(fileno(stdin), &fds));
        {
            char input[MAX_DATA];
            char c;
            int count = 0;

            while (1)
            {
                c = getchar();
                if (c == '\r' || c == '\n')
                {
                    input[count] = '\0';
                    break;
                }
                input[count] = c;
                count++;
            }

            char* result[MAX_DATA];
            count = 0;
            char* pch;
            pch = strtok(input, " ,<>");
            while (pch)
            {
                result[count] = pch;
                count++;
                pch = strtok(NULL, " ,<>");

            }

            if (strcmp(result[0], "/login") == 0)
            {
                if (count == 5)
                {
                    if (!isconnected)
                    {
                        port = string_to_int(result[4]);
                        memset(&hints, 0, sizeof (hints));
                        hints.ai_family = AF_UNSPEC;
                        hints.ai_socktype = SOCK_STREAM;

                        readval = getaddrinfo(result[3], result[4], &hints, &me);
                        if (readval)
                        {
                            printf("Error: Failed to get address information. \n\n");
                            return 1;
                        }

                        for (ptr = me; ptr != NULL; ptr = ptr->ai_next)
                        {
                            s = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

                            if (s == -1)
                            {
                                printf("Error: Failed to create socket. \n\n");
                                continue;
                            }

                            if ((connect(s, ptr->ai_addr, ptr->ai_addrlen)) == -1)
                            {
                                printf("Error: Failed to connect. \n\n");
                                continue;
                            }
                            break;
                        }

                        if (ptr == NULL)
                        {
                            printf("Error: Failed to connect. \n\n");
                        }

                        bzero(buf, sizeof (message_packet));
                        buf->type = LOGIN;
                        buf->size = strlen(result[1]) + strlen(result[2]) + 1;
                        strcpy(buf->source, result[1]);
                        strcpy(username, result[1]);
                        strcpy(buf->data, result[1]);
                        strcat(buf->data, " ");
                        strcat(buf->data, result[2]);
                        strcpy(password, result[2]);
                        n = send(s, buf, sizeof (message_packet), 0);

                        //printf("login sent \n");

                        bzero(buf, sizeof (message_packet));
                        m = recv(s, buf, sizeof (message_packet), 0);

                        //printf("log ack received \n");
                        //printf("=== %s \n", buf->data);

                        if (buf->type = LO_ACK)
                        {
                            isconnected = 1;
                            printf("Server: Successfully logged in. \n\n");
                        }
                        else if (buf->type = LO_NAK)
                        {
                            isconnected = 0;
                            printf("Server: Failed to log in. \n\n");
                        }
                    }
                    else
                    {
                        printf("Error: Currently logged in. \n\n");
                    }
                }
                else
                {
                    printf("Client: Invalid number of arguments. \n\n");
                }
            }
            else if (strcmp(result[0], "/logout") == 0)
            {
                if (count == 1)
                {
                    if (check_is_connected(isconnected))
                    {
                        isinsession = 0;
                        isconnected = 0;
                        bzero(buf, sizeof (message_packet));
                        buf->type = EXIT;
                        buf->size = 0;
                        strcpy(buf->source, username);
                        strcpy(buf->data, "");
                        n = send(s, buf, sizeof (message_packet), 0);
                        //printf("logout sent \n");
                        close(s);
                        s = -1;
                        //printf("Successfully logged out. \n\n");
                    }
                }
                else
                {
                    printf("Client: Invalid number of arguments. \n\n");
                }
            }
            else if (strcmp(result[0], "/joinsession") == 0)
            {
                if (count == 2)
                {
                    //printf("in join session \n");
                    if (check_is_connected(isconnected))
                    {
                        if (!isinsession)
                        {
                            bzero(buf, sizeof (message_packet));
                            buf->type = JOIN;
                            strcpy(buf->source, username);
                            strcpy(buf->data, result[1]);
                            strcpy(sessionname, result[1]);
                            buf->size = strlen(buf->data);

                            n = send(s, buf, sizeof (message_packet), 0);

                            //printf("join session sent \n");

                            bzero(buf, sizeof (message_packet));

                            //printf("ready to receive join ack \n");

                            m = recv(s, buf, sizeof (message_packet), 0);
                            //printf("=== %s \n", buf->data);
                            //printf("received join ack \n");
                            //printf("===== ack: %d \n", buf->type);

                            if (buf->type == JN_ACK)
                            {
                                //printf("hi im here \n");
                                isinsession = 1;
                                printf("Server: Successfully joined session %s \n", result[1]);
                            }
                            else if (buf->type == JN_NAK)
                            {
                                isinsession = 0;
                                printf("Server: Failed to join session: %s", result[1]);
                            }
                        }
                        else
                        {
                            printf("Error: currently in session. \n\n");
                        }
                    }
                }
                else
                {
                    printf("Client: Invalid number of arguments. \n\n");
                }
            }
            else if (strcmp(result[0], "/leavesession") == 0)
            {
                if (count == 1)
                {
                    //printf("in leave session \n");
                    if (check_is_connected(isconnected))
                    {
                        if (isinsession)
                        {
                            isinsession = 0;
                            bzero(buf, sizeof (message_packet));
                            buf->type = LEAVE_SESS;
                            buf->size = 0;
                            strcpy(buf->source, username);
                            buf->data[0] = '\0';
                            n = send(s, buf, sizeof (message_packet), 0);
                            //printf("leave session sent \n");
                            strcpy(sessionname, "");
                            printf("Server: Successfully left the session %s. \n\n", sessionname);
                        }
                        else
                        {
                            printf("Error: currently not in session. \n\n");
                        }
                    }
                }
                else
                {
                    printf("Client: Invalid number of arguments. \n\n");
                }
            }
            else if (strcmp(result[0], "/createsession") == 0)
            {

                if (count == 2)
                {
                    //printf("in create session \n");
                    if (check_is_connected(isconnected))
                    {
                        bzero(buf, sizeof (message_packet));
                        buf->type = NEW_SESS;
                        strcpy(buf->source, username);
                        strcpy(buf->data, result[1]);
                        strcpy(sessionname, result[1]);
                        buf->size = strlen(buf->data);
                        //printf("create session sending \n");

                        n = send(s, buf, sizeof (message_packet), 0);

                        //printf("\n=== n: %d \n", n);
                        //printf("create session sent \n");
                        bzero(buf, sizeof (message_packet));

                        //printf("ready to receive ns ack \n");

                        m = recv(s, buf, sizeof (message_packet), 0);

                        //printf("received ns ack \n");
                        //printf("=== %s \n", buf->data);

                        if (buf->type == NS_ACK)
                        {
                            printf("Server: Successfully created session: %s. \n\n", sessionname);
                        }
                        else
                        {
                            printf("Error: Failed to create session: %s. \n\n", sessionname);
                        }
                    }
                }
                else
                {
                    printf("Client: Invalid number of arguments. \n\n");
                }
            }
            else if (strcmp(result[0], "/list") == 0)
            {

                if (count == 1)
                {
                    //printf("in list \n");
                    if (check_is_connected(isconnected))
                    {
                        bzero(buf, sizeof (message_packet));
                        buf->type = QUERY;
                        buf->size = 0;
                        strcpy(buf->source, username);
                        buf->data[0] = '\0';
                        n = send(s, buf, sizeof (message_packet), 0);
                        //printf("list sent \n");
                        bzero(buf, sizeof (message_packet));
                        m = recv(s, buf, sizeof (message_packet), 0);
                        //printf("=== %s \n", buf->data);
                        if (buf->type == QU_ACK)
                        {
                            printf("%s\n", buf->data);
                        }
                        else
                        {
                            printf("Error: Failed to print list. \n\n");
                        }
                    }
                }
                else
                {
                    printf("Client: Invalid number of arguments. \n\n");
                }
            }
            else if (strcmp(result[0], "/quit") == 0)
            {
                //printf("in quit session \n");
                if (count == 1)
                {
                    send(s, "EXIT::", 5, 0);
                    printf("Client: Exiting Chat Room. \n\n");
                    close(s);
                    s = -1;

                    return (EXIT_SUCCESS);
                }
                else
                {
                    printf("Client: Invalid number of arguments. \n\n");
                }
            }
            else if (strcmp(result[0], "/invite") == 0)
            {
                if (count == 2)
                {
                    if (check_is_connected(isconnected))
                    {
                        if (isinsession)
                        {
                            bzero(buf, sizeof (message_packet));
                            buf->type = INVITE;
                            strcpy(buf->source, username);
                            // clientID of invitee
                            //printf("===== %s \n", result[1]);
                            strcpy(buf->data, result[1]);
                            buf->size = strlen(buf->data);
                            n = send(s, buf, sizeof (message_packet), 0);
                            printf("invite sent \n");
                        }
                        else
                        {
                            printf("Error: currently not in session. \n\n");
                        }
                    }
                }
                else
                {
                    printf("Client: Invalid number of arguments. \n\n");
                }
            }
            else if (strcmp(result[0], "/accept") == 0)
            {
                if (count == 1)
                {
                    buf->type = IV_ACK;
                    isinsession = 1;
                    //printf("--- buf data: %s\n", buf->data);
                    send(s, buf, sizeof (message_packet), 0);
                }
                else
                {
                    printf("Client: Invalid number of arguments. \n\n");
                }
            }
            else if (strcmp(result[0], "/decline") == 0)
            {
                if (count == 1)
                {
                    buf->type = IV_NAK;
                    //printf("--- buf data: %s\n", buf->data);
                    send(s, buf, sizeof (message_packet), 0);
                }
                else
                {
                    printf("Client: Invalid number of arguments. \n\n");
                }
            }
            else
            {
                //printf("in message \n");
                if (check_is_connected(isconnected))
                {
                    if (isinsession)
                    {
                        printf("===== text message received.\n");
                        printf("===== total count: %d \n", count);
                        char textmessage[MAX_DATA];

                        strcpy(textmessage, "");

                        int i;
                        for (i = 0; i < count; ++i)
                        {
                            strcat(textmessage, result[i]);
                            strcat(textmessage, " ");
                        }
                        strcat(textmessage, "\n");

                        bzero(buf, sizeof (message_packet));
                        buf->type = MESSAGE;
                        strcpy(buf->source, username);
                        strcpy(buf->data, textmessage);
                        buf->size = strlen(buf->data);
                        n = send(s, buf, sizeof (message_packet), 0);
                        printf("message sent \n");
                    }
                }
            }
        }
    }
    return (EXIT_SUCCESS);
}

bool check_is_connected(bool input)
{
    if (input)
    {
        return 1;
    }
    else
    {
        printf("Error: Currently not logged in. \n\n");
        return 0;
    }
}

int string_to_int(char* input)
{
    int num = 0;
    int i, j, len;
    len = strlen(input);
    for (i = 0; i < len; i++)
    {
        num = num * 10 + (input[i] - '0');
    }
    return num;
}

/*============================================================================*/

// /login u2 123 127.0.0.1 5000
// /login tonysb 123 128.100.13.242 5000


// 0 1 2 3 4 5
// b b   a a a

// sender = 6 - 2
