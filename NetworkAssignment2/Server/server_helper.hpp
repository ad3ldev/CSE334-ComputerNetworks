#if !defined(SERVER_HELPER_HPP)
#define SERVER_HELPER_HPP

#include "../packet_header.hpp"

void handle_client_request(int server_socket, int client_fd, sockaddr_in client_addr, char rec_buffer[], int bufferSize);
long does_file_exist(string fileName);
vector<string> read_file(string fileName);
void send_and_handle_congestion(int client_fd, struct sockaddr_in client_addr, vector<string> data);
packet create_packet_data(string packetString, int seqNum);
bool send_packet(int client_fd, struct sockaddr_in client_addr, string temp_packet_string, int seqNum);
bool drop_datagram();
bool corrupt_datagram();
uint16_t get_data_checksum(string content, uint16_t len, uint32_t seqno);
uint16_t get_ack_checksum(uint16_t len, uint32_t ackno);

enum fsm_state
{
    slow_start,
    congestion_avoidance,
    fast_recovery
};

#endif // SERVER_HELPER_HPP
