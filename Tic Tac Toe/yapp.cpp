#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip_icmp.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <ctime>
#include <iostream>
#include <chrono> 
#include <vector>

using namespace std;
#define current_time std::chrono::high_resolution_clock::now()
#define clock_duration chrono::duration<double, milli>

#define PORT 8080
#define TIMEOUT_THRESHOLD 10

#define PACKET_SIZE 64
struct ping_pkt
{
	struct icmphdr hdr;
	char msg[PACKET_SIZE - sizeof(struct icmphdr)];
};

bool checkIP(char*);
bool further_check(char*);
void yapp(int, char*);


// Check if the given IP address follows IPv4 format
bool further_check(char* IP){

	vector<string> bytes;
	string current = "";
	int dots = 0;

	for(int i=0; i<strlen(IP); i++){
		if(IP[i] == '.'){
			bytes.push_back(current);
			current = "";
			dots++;
		} else {
			if(IP[i] < '0' || IP[i] > '9') return false;		// non-digit characters
			current += IP[i];
		}
	}
	if(current.length() > 0) bytes.push_back(current);

	if(bytes.size() != 4 || dots != 3) return false;			// (format should be x1.x2.x3.x4)
	for(string byte : bytes){
		if(byte.length() > 1 && byte[0] == '0') return false;	// no leading zeros
		int byte_int = stoi(byte);
		if(byte_int < 0 || byte_int > 255) return false;		// 0 <= x1, x2, x3, x4 <= 255
	}

	return 1;
}

bool checkIP(char *IP){
	struct in_addr temp;
	int is_valid = inet_aton(IP, (struct in_addr*) &temp);		// check with inet_aton
	//bool flag = further_check(IP);
	//is_valid &= flag;
	return is_valid;
}

void yapp(int sockfd, char * IP){
	struct sockaddr_in dst ; 
	dst.sin_family = AF_INET;
	dst.sin_port = htons (PORT);
	dst.sin_addr.s_addr = inet_addr(IP);

	struct timeval tv_out;
	tv_out.tv_sec = TIMEOUT_THRESHOLD;
	tv_out.tv_usec = 0;

	struct ping_pkt send;
	bzero(&send, sizeof(send));

	send.hdr.type = ICMP_ECHO;

	// Set receive timeout
	int err = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv_out, (int)sizeof(tv_out));
	if(err < 0){
		cout << "Failed to set socket options\n";
		return;
	}

	auto send_time = current_time;

	// send packet to the server
	if(sendto(sockfd, &send, sizeof(send), 0, (struct sockaddr*)&dst,sizeof(dst)) <= 0){
		cout << "Request timed out or host unreacheable.\n";
		return;
	}
	
	struct ping_pkt rec;
	int rec_len = sizeof(rec);
	
	// receive packet from the server
	if(recvfrom(sockfd, &send, sizeof(send), 0, (struct sockaddr*)&rec, (socklen_t*)&rec_len) <= 0){
		cout << "Request timed out or host unreacheable.\n";
		return;
	}

	auto rec_time = current_time;
	double RTT = clock_duration(rec_time - send_time).count();
	cout << "Reply from " + string(IP) + ". RTT = " + to_string(RTT) + " ms\n";
}

int main(int argc, char *argv[])
{
	if(argc != 2){
		cout << "Usage Format: ./yapp <IP Address>\n";
		return 0;
	}

	char *IP = argv[1];
	int is_valid = -1;

	// Verify if the IP is valid/not
	if((is_valid = checkIP(IP)) == 0){
		cout << "Bad hostname (" + (string)IP + ")\n";
		return 0;
	}
	
	// Create a socket & send ping
	int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
	yapp(sockfd, IP);
	return 0;
}
