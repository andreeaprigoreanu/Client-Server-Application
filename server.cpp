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

// structures used to store info:
// map to store pairs of client id and socket
unordered_map<string, int> id_client_and_socket;
// map to store pairs of socket and client id
unordered_map<int, string> socket_and_client_id;
// map to store pairs of topic and vector of subscribers to that topic
unordered_map<string, vector<subscriber>> topic_and_subscribers;
// map to store pairs of id and unsend messages for sf = 1
unordered_map<string, vector<tcp_message>> client_and_unsend_messages;

// function to parse upd packet from buffer to message for a tcp client
tcp_message create_tcp_message(udp_message recv_udp_msg, struct sockaddr_in udp_addr) {
    // check if data type is correct
    DIE(recv_udp_msg.data_type >= 4, "Incorrect data type");

    // new tcp message
    struct tcp_message tcp_msg;
    memset(&tcp_msg, 0, sizeof(tcp_message));

    // copy topic
    memcpy(tcp_msg.topic, recv_udp_msg.topic, strlen(recv_udp_msg.topic) + 1);

    // set ip and port
    strcpy(tcp_msg.ip_udp_client, inet_ntoa(udp_addr.sin_addr));
    tcp_msg.port_udp_client = ntohs(udp_addr.sin_port);

    // interpret payload according to data type
    if (recv_udp_msg.data_type == INT_TYPE) {
        tcp_msg.data_type = INT_TYPE;

        uint32_t recv_num;
        // extract number without sign byte
        memcpy(&recv_num, recv_udp_msg.payload + 1, 4);

        // check sign byte
        if (recv_udp_msg.payload[0] == 1) {
            // number is signed
            sprintf(tcp_msg.payload, "%d", -ntohl(recv_num));
        } else {
            // unsigned number
            sprintf(tcp_msg.payload, "%d", ntohl(recv_num));
        }

        return tcp_msg;
    }

    if (recv_udp_msg.data_type == SHORT_REAL_TYPE) {
        tcp_msg.data_type = SHORT_REAL_TYPE;

        // extract number uint16_t with first two decimals after dot
        double recv_num = ntohs(* (uint16_t*)(recv_udp_msg.payload));
        recv_num /= 100;
        if (floor(recv_num) == ceil(recv_num)) {
            sprintf(tcp_msg.payload, "%d", (int) recv_num);
        } else {
            sprintf(tcp_msg.payload, "%.2f", recv_num);
        }

        return tcp_msg;
    }

    if (recv_udp_msg.data_type == FLOAT_TYPE) {
        tcp_msg.data_type = FLOAT_TYPE;

        // extract number without sign byte
        float recv_num = ntohl(* (uint32_t*)(recv_udp_msg.payload + 1));
        recv_num = recv_num / pow(10, recv_udp_msg.payload[5]);

        // check sign byte
        if (recv_udp_msg.payload[0] == 1) {
            sprintf(tcp_msg.payload, "%lf", -recv_num);
        } else {
            sprintf(tcp_msg.payload, "%lf", recv_num);
        }

        return tcp_msg;
    }

    if (recv_udp_msg.data_type == STRING_TYPE) {
        tcp_msg.data_type = STRING_TYPE;

        // extract string
        memcpy(tcp_msg.payload, recv_udp_msg.payload, strlen(recv_udp_msg.payload));
        // add string terminator
        tcp_msg.payload[strlen(tcp_msg.payload)] = '\0';

        return tcp_msg;
    }
    
    tcp_msg.data_type = 4;
    return tcp_msg;
}

bool tcp_message_is_valid(tcp_message tcp_msg) {
    if (tcp_msg.data_type == 4) {
        return false;
    }

    return true;
}

int main (int argc, char *argv[]) {
    DIE(argc < 2, "Not enough arguments\n");
    // check if port number is in bounds
    int portno = atoi(argv[1]);
    DIE(portno < 1024, "Incorrect port number\n");
    

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    char buffer[BUFLEN];

    int ret;            // return value
    fd_set read_fds;	// set of read fds used by select()
	fd_set tmp_fds;		// temporary set of fds
	int fdmax;			// maximum fd from set read_fds

    // tcp, udp and client sockets
    struct sockaddr_in tcp_addr, udp_addr, cli_addr;
    // socklen_t sock_len = sizeof(struct sockaddr_in);
    socklen_t sock_len = sizeof(struct sockaddr);

    // open tcp socket
    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_sock < 0, "Couldn't open tcp socket\n");
    // disable neagle algorithm for tcp socket
    int no_neagle = 1;
    ret = setsockopt(tcp_sock, IPPROTO_TCP, TCP_NODELAY, &no_neagle, sizeof(int));
    DIE(ret == -1, "Couldn't disable neagle\n");

    // open udp socket
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_sock < 0, "Couldn't open udp socket\n");

    // tcp socket fields
    memset((char *) &tcp_addr, 0, sizeof(tcp_addr));
	tcp_addr.sin_family = AF_INET;
	tcp_addr.sin_port = htons(portno);
	tcp_addr.sin_addr.s_addr = INADDR_ANY;
    // udp socket fields
    memset((char *) &udp_addr, 0, sizeof(udp_addr));
	udp_addr.sin_family = AF_INET;
	udp_addr.sin_port = htons(portno);
	udp_addr.sin_addr.s_addr = INADDR_ANY;


    // empty read_fds and tmp_fds
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    // bind and listen for tcp socket
    ret = bind(tcp_sock, (struct sockaddr *) &tcp_addr, sock_len);
	DIE(ret < 0, "bind for tcp failed\n");

    ret = listen(tcp_sock, MAX_CLIENTS);
	DIE(ret < 0, "liten failed for tcp\n");

    FD_SET(tcp_sock, &read_fds);

    // bind and listen for udp socket
    ret = bind(udp_sock, (struct sockaddr *) &udp_addr, sock_len);
	DIE(ret < 0, "bind for udp failed\n");

    FD_SET(udp_sock, &read_fds);

    FD_SET(STDIN, &read_fds);

    // set fdmax
    fdmax = max(tcp_sock, udp_sock);
    
    while (1) {
        tmp_fds = read_fds;

        // select
        ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select failed\n");

        // check for stdin command
        if (FD_ISSET(STDIN, &tmp_fds)) {
            // citire comanda
            memset(buffer, 0, BUFLEN);
            fgets(buffer, BUFLEN - 1, stdin);

            if (strncmp(buffer, "exit", 4) == 0) {
                // send end connection tcp message to all active clients
                for (int i = 5; i <= fdmax; i++) {
                    if (FD_ISSET(i, &read_fds)) {
                        tcp_message stop_tcp_msg;
                        memset(stop_tcp_msg.topic, 0, TOPIC_SIZE);
                        strcpy(stop_tcp_msg.topic, "stop");

                        ret = send(i, (char *) &stop_tcp_msg, sizeof(tcp_message), 0);
                        DIE(ret < 0, "send failed\n");
                    }
                }

                break;
            }
        }

        for (int i = 1; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmp_fds)) {
                if (i == udp_sock) {
                    // received message from udp client
                    udp_message recv_udp_msg;
                    memset(&recv_udp_msg, 0, sizeof(udp_message));
                    ret = recvfrom(udp_sock, &recv_udp_msg,
                        sizeof(udp_message), 0, (sockaddr *) &udp_addr, &sock_len);
                    DIE(ret < 0, "recvfrom udp failed\n");

                    // create tcp message from udp message
                    tcp_message tcp_msg = create_tcp_message(recv_udp_msg, udp_addr);

                    if (!tcp_message_is_valid(tcp_msg)) {
                        continue;
                    }

                    if (topic_and_subscribers.find(recv_udp_msg.topic) != topic_and_subscribers.end()) {
                        // topic exists
                        auto topic_entry = topic_and_subscribers.find(recv_udp_msg.topic);

                        // check all subscribers to current topic
                        for (subscriber subscriber : topic_entry->second) {
                            int subscriber_sock = id_client_and_socket[subscriber.id_client];

                            // check if current subscriber is active
                            if (subscriber_sock >= 5) {
                                // send tcp message
                                ret = send(subscriber_sock, &tcp_msg, sizeof(tcp_msg), 0);
                                DIE(ret < 0, "send to subscriber failed\n");
                            } else {
                                if (subscriber_sock < 0) {
                                    // add message to queue if SF is set
                                    if (subscriber.SF) {
                                        client_and_unsend_messages[subscriber.id_client].push_back(tcp_msg);
                                    }
                                }
                            }
                        }
                    } else {
                        continue;
                    }
                } else {
                    if (i == tcp_sock) {
                        // connection request
                        int client_sock = accept(tcp_sock, (struct sockaddr *) &cli_addr, &sock_len);
                        DIE(client_sock < 0, "accept failed\n");

                        no_neagle = 1;
                        ret = setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, &no_neagle, sizeof(int));
                        DIE(ret == -1, "Couldn't disable neagle\n");

                        // update read_fds
                        FD_SET(client_sock, &read_fds);
                        // update fdmax
                        fdmax = max(fdmax, client_sock);

                        // receive id from client
                        memset(buffer, 0, BUFLEN);
                        ret = recv(client_sock, buffer, BUFLEN, 0);
                        DIE(ret < 0, "recv from client failed\n");

                        // extract new client id
                        string new_client_id(buffer);
                        char new_client_id_copy[CLIENT_ID_SIZE];
                        strcpy(new_client_id_copy, buffer);

                        if (id_client_and_socket.find(new_client_id) == id_client_and_socket.end()) {
                            socket_and_client_id[client_sock] = new_client_id;
                            id_client_and_socket[new_client_id] = client_sock;

                            // print new connection message
                            printf("New client %s connected from %s:%hu.\n", new_client_id_copy,
                                    inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                        } else {
                            // extract current socket
                            int current_client_sock = id_client_and_socket.find(new_client_id)->second;
                            if (current_client_sock >= 5) {
                                // subscriber with this id already exists and is active
                                printf("Client %s already connected.\n", new_client_id_copy);

                                // send message to close client
                                tcp_message stop_tcp_msg;
                                memset(stop_tcp_msg.topic, 0, TOPIC_SIZE);
                                strcpy(stop_tcp_msg.topic, "stop");

                                ret = send(client_sock, (char *)&stop_tcp_msg, sizeof(tcp_message), 0);
                                DIE(ret < 0, "send failed\n");

                                FD_CLR(client_sock, &read_fds);
                                close(client_sock);

                                continue;
                            }
                            // old client wants to reconnect
                            id_client_and_socket[new_client_id] = client_sock;
                            socket_and_client_id[client_sock] = new_client_id;

                            // print new connection message
                            printf("New client %s connected from %s:%hu.\n", new_client_id_copy,
                                    inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

                            // send messages from queue
                            for (auto tcp_msg : client_and_unsend_messages[new_client_id]) {
                                ret = send(client_sock, (char*) &tcp_msg, sizeof(tcp_msg), 0);
                                DIE(ret < 0, "send tcp message failed\n");
                            }
                            client_and_unsend_messages[new_client_id].clear();
                        }
                    } else {
                        // received command from tcp client
                        ret = recv(i, buffer, BUFLEN - 1, 0);
                        DIE(ret < 0, "recv failed\n");

                        if (ret == 0) {
                            // client disconnected
                            printf("Client %s disconnected.\n", socket_and_client_id[i].c_str());
                            id_client_and_socket[socket_and_client_id[i]] = -1;
                            close(i);
                            FD_CLR(i, &read_fds);

                            continue;
                        }

                        // extract server message
                        server_message server_msg;
                        memcpy(&server_msg, &buffer, sizeof(server_message));

                        string client_id(server_msg.client_id), topic(server_msg.topic);

                        if (strncmp(server_msg.command, "subscribe", 9) == 0) {
                            // subscribe request
                            if (topic_and_subscribers.find(topic) == topic_and_subscribers.end()) {
                                // new topic
                                subscriber new_subscriber;
                                new_subscriber.id_client = client_id;
                                new_subscriber.SF = server_msg.SF;

                                vector <subscriber> subscribers;
                                subscribers.push_back(new_subscriber);

                                topic_and_subscribers[topic] = subscribers;
                            } else {
                                // add subscriber
                                bool found;
                                auto topic_entry = topic_and_subscribers.find(topic);
                                for (subscriber subscriber : topic_entry->second) {
                                    if (subscriber.id_client == client_id) {
                                        subscriber.SF = server_msg.SF;
                                        found = true;
                                        break;
                                    }
                                }

                                if (!found) {
                                    subscriber new_subscriber;
                                    new_subscriber.id_client = client_id;
                                    new_subscriber.SF = server_msg.SF;

                                    topic_entry->second.push_back(new_subscriber);
                                }
                            }

                            continue;
                        }

                        if (strncmp(server_msg.command, "unsubscribe", 11) == 0) {
                            if (topic_and_subscribers.find(topic) == topic_and_subscribers.end()) {
                                continue;
                            }

                            auto topic_entry = topic_and_subscribers.find(topic);
                            for (size_t j = 0; j < topic_entry->second.size(); j++) {
                                if (topic_entry->second[j].id_client == client_id) {
                                    topic_entry->second.erase(topic_entry->second.begin() + j);
                                    break;
                                }
                            }
                            continue;
                        }
                    }
                }
            }
        }
    }

    // close sockets
    close(tcp_sock);
    close(udp_sock);

    return 0;
}
