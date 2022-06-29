#ifndef HELPER_H
#define HELPER_H

#include <bits/stdc++.h>
#include <string>

using namespace std;

#define BUFLEN 1600         // maximum buffer size
#define TOPIC_SIZE 50       // maximum topic size
#define IP_SIZE 16
#define CMD_SIZE 12
#define CLIENT_ID_SIZE 11
#define MAX_PAYLOAD 1500    // maximum payload size
#define STDIN 0
#define MAX_CLIENTS 50      // maximum waiting clients

// data type for tcp messages
#define INT_TYPE 0
#define SHORT_REAL_TYPE 1
#define FLOAT_TYPE 2
#define STRING_TYPE 3

// macro for checking errors
#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

// received udp message
struct udp_message {
	char topic[TOPIC_SIZE];
    uint8_t data_type;
	char payload[MAX_PAYLOAD];
};

// message sent from server to tcp client
struct tcp_message {
	char ip_udp_client[IP_SIZE];
    uint16_t port_udp_client;
	char topic[TOPIC_SIZE];
	uint8_t data_type;
	char payload[MAX_PAYLOAD];
};

struct subscriber {
    string id_client;
	int SF;
};

// message sent from tcp client to server
struct server_message {
    char command[CMD_SIZE];
    char topic[TOPIC_SIZE];
    int SF;
    char client_id[CLIENT_ID_SIZE];
};

#endif
