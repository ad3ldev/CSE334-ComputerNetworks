#if !defined(PACKET_HEADER_HPP)
#define PACKET_HEADER_HPP

#include <utility>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <string>
#include <thread>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <chrono>
#include <bits/stdc++.h>

static const int MAXIMUM_SEGMENT_SIZE = 508;
static const int ACK_PACKET_SIZE = 8;
static const int CHUNK_SIZE = 499;

using namespace std;

struct packet
{
    uint16_t cksum;
    uint16_t len;
    uint32_t seqno;
    char data[500];
};

struct not_sent_packet
{
    int seqno;
    chrono::time_point<chrono::system_clock> timer;
    bool done;
};

struct ack_packet
{
    uint16_t cksum;
    uint16_t len;
    uint32_t ackno;
};

#endif // PACKET_HEADER_HPP
