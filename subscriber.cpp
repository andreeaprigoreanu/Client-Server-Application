#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <iostream>

#include "helper.h"

// function to convert data type to string
string covert_data_type_to_string(int data_type) {
	switch (data_type) {
        case INT_TYPE:
            return "INT";
            break;

        case SHORT_REAL_TYPE:
            return "SHORT_REAL";
            break;
        case FLOAT_TYPE:
            return "FLOAT";
            break;
        
        case STRING_TYPE:
            return "STRING";
            break;
        
        default:
            return "";
            break;
	}
}

int main (int argc, char *argv[]) {
    DIE(argc < 4, "Not enough arguments\n");\
    // check if port number is in bounds
    int portno = atoi(argv[3]);
    DIE(portno == 0, "Incorrect port number\n");

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    char buffer[BUFLEN];

    int ret;            // return value
    fd_set read_fds;	// set of read fds used by select()
	fd_set tmp_fds;		// temporary set of fds
	int fd_max;			// maximum fd from set read_fds

    // empty read_fds and tmp_fds
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    // open tcp socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sock_fd < 0, "Couldn't open socket\n");
    // disable neagle algorithm for tcp socket
    int no_neagle = 1;
    ret = setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &no_neagle, sizeof(int));
    DIE(ret == -1, "Couldn't disable neagle\n");

    // server socket fields
    struct sockaddr_in serv_addr;
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "Incorrect IP address\n");

    // connect to server
    ret = connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect failed\n");

    memset(buffer, 0, BUFLEN);
    strcpy(buffer, argv[1]);

    // send id to server
	ret = send(sock_fd, buffer, BUFLEN, 0);
	DIE(ret < 0, "send failed\n");

    FD_SET(sock_fd, &read_fds);
	FD_SET(STDIN, &read_fds);
	fd_max = sock_fd;

    while (1) {
        tmp_fds = read_fds;

        // select
        ret = select(fd_max + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select failed\n");

        if (FD_ISSET(sock_fd, &tmp_fds)) {
            // received tcp message
			tcp_message tcp_msg;
            ret = recv(sock_fd, &tcp_msg, sizeof(tcp_message), 0);
			DIE(ret < 0, "recv from server failed\n");

            if (strncmp(tcp_msg.topic, "stop", 4) == 0) {
                break;
            }

            printf("%s:%hu - %s - %s - %s\n", tcp_msg.ip_udp_client, tcp_msg.port_udp_client,
				    tcp_msg.topic, covert_data_type_to_string(tcp_msg.data_type).c_str(), tcp_msg.payload);
        }

        if (FD_ISSET(STDIN, &tmp_fds)) {
            // command from stdin
            memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN - 1, stdin);
			buffer[strlen(buffer) - 1] = '\0';

            if (strncmp(buffer, "exit", 4) == 0) {
                break;
            }

            if (strncmp(buffer, "subscribe", 9) == 0) {
                server_message server_msg;
                strcpy(server_msg.client_id, argv[1]);
                strcpy(server_msg.command, "subscribe");

				// extract topic from buffer
				char *token = strtok(buffer, " ");
                DIE(!token, "invalid command\n");
                token = strtok(NULL, " ");
                DIE(!token, "invalid command\n");
                strcpy(server_msg.topic, token);

                // extract SF
                token = strtok(NULL, " ");
                DIE(!token, "invalid command\n");
                server_msg.SF = atoi(token);

                // send message to server
				ret = send(sock_fd, &server_msg, sizeof(server_message), 0);
				DIE(ret < 0, "send failed\n");

                printf("Subscribed to topic.\n");

                continue;
            }

            if (strncmp(buffer, "unsubscribe", 11) == 0) {
                server_message server_msg;
                strcpy(server_msg.client_id, argv[1]);
                strcpy(server_msg.command, "unsubscribe");

                // extract topic from buffer
				char *token = strtok(buffer, " ");
                DIE(!token, "invalid command\n");
                token = strtok(NULL, " ");
                DIE(!token, "invalid command\n");
                strcpy(server_msg.topic, token);

                // send message to server
				ret = send(sock_fd, &server_msg, sizeof(server_message), 0);
				DIE(ret < 0, "send failed\n");

                printf("Unsubscribed from topic.\n");

                continue;
            }
        }
    }

    close(sock_fd);

    return 0;
}
