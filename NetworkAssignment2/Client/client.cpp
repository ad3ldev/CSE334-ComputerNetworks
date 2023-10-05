#include "../packet_header.hpp"

void check(int code, string message)
{
    int length = message.length();
    char *msg = new char[length + 1];
    strcpy(msg, message.c_str());
    if (code < 0)
    {
        perror(msg);
        exit(1);
    }
}
packet create_packet(string file_name)
{
    struct packet p;
    strcpy(p.data, file_name.c_str());
    p.seqno = 0;
    p.cksum = 0;
    p.len = file_name.length() + sizeof(p.cksum) + sizeof(p.len) + sizeof(p.seqno);
    return p;
}
uint16_t get_ack_checksum(uint16_t len, uint32_t ackNo)
{
    uint32_t sum = 0;
    sum += len;
    sum += ackNo;
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    uint16_t CSum = (uint16_t)(~sum);
    return CSum;
}
uint16_t get_data_checksum(string content, uint16_t len, uint32_t seqNo)
{
    uint32_t sum = 0;
    sum += len;
    sum += seqNo;
    int n;
    n = content.length();
    char arr[n + 1];
    strcpy(arr, content.c_str());
    for (int i = 0; i < n; i++)
    {
        sum += arr[i];
    }
    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    uint16_t oc_sum = (uint16_t)(~sum);
    return oc_sum;
}

void send_ack(int client_socket, struct sockaddr_in server_address, int seqNum)
{
    struct ack_packet ack;
    ack.ackno = seqNum;
    ack.len = sizeof(ack);
    ack.cksum = get_ack_checksum(ack.len, ack.ackno);
    char *ack_buf = new char[MAXIMUM_SEGMENT_SIZE];
    memset(ack_buf, 0, MAXIMUM_SEGMENT_SIZE);
    memcpy(ack_buf, &ack, sizeof(ack));
    ssize_t bytesSent = sendto(client_socket, ack_buf, MAXIMUM_SEGMENT_SIZE, 0, (struct sockaddr *)&server_address, sizeof(struct sockaddr));
    check(bytesSent, "Error in sending the ack!");
    cout << "Ack for packet seq. Num " << seqNum << " is sent." << endl;
}

void writeFile(string fileName, string content)
{
    ofstream f_stream(fileName.c_str());
    f_stream.write(content.c_str(), content.length());
}

vector<string> read_args()
{
    string fileName = "client.in";
    vector<string> commands;
    string line;
    ifstream f;
    f.open(fileName);
    while (getline(f, line))
    {
        commands.push_back(line);
    }
    return commands;
}

int main(int argc, char const *argv[])
{
    vector<string> arguments = read_args();
    string IP_Address = arguments[0];
    int port = stoi(arguments[1]);
    string fileName = arguments[2];
    struct sockaddr_in server_address;
    int client_socket;
    memset(&client_socket, '0', sizeof(client_socket));
    client_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    check(client_socket, "Failed");
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);
    cout << "File Name is:\t" << fileName << "\nThe length of the Name:\t" << fileName.size() << endl;
    struct packet fileName_packet = create_packet(fileName);
    char *buffer = new char[MAXIMUM_SEGMENT_SIZE];
    memset(buffer, 0, MAXIMUM_SEGMENT_SIZE);
    memcpy(buffer, &fileName_packet, sizeof(fileName_packet));
    ssize_t bytesSent = sendto(client_socket, buffer, MAXIMUM_SEGMENT_SIZE, 0, (struct sockaddr *)&server_address, sizeof(struct sockaddr));
    check(bytesSent, "Error in sending the file name!");
    cout << "Client Sent The file Name ." << endl;
    char rec_buffer[MAXIMUM_SEGMENT_SIZE];
    socklen_t addrlen = sizeof(server_address);
    ssize_t Received_bytes = recvfrom(client_socket, rec_buffer, MAXIMUM_SEGMENT_SIZE, 0, (struct sockaddr *)&server_address, &addrlen);
    check(Received_bytes, "Error in receiving file name ack.");
    auto *ackPacket = (struct ack_packet *)rec_buffer;
    cout << "Number of packets " << ackPacket->len << endl;
    long numberOfPackets = ackPacket->len;
    string fileContents[numberOfPackets];
    bool recieved[numberOfPackets] = {false};
    int i = 1;
    int expectedSeqNum = 0;
    while (i <= numberOfPackets)
    {
        memset(rec_buffer, 0, MAXIMUM_SEGMENT_SIZE);
        ssize_t bytesReceived = recvfrom(client_socket, rec_buffer, MAXIMUM_SEGMENT_SIZE, 0, (struct sockaddr *)&server_address, &addrlen);
        if (bytesReceived == -1)
        {
            perror("Error receiving data packet.");
            break;
        }
        auto *data_packet = (struct packet *)rec_buffer;
        cout << "packet " << i << " received" << endl;
        cout << "Sequence Number : " << data_packet->seqno << endl;
        int len = data_packet->len;
        for (int j = 0; j < len; j++)
        {
            fileContents[data_packet->seqno] += data_packet->data[j];
        }
        if (get_data_checksum(fileContents[data_packet->seqno], data_packet->len, data_packet->seqno) != data_packet->cksum)
        {
            cout << "corrupted data packet !" << endl;
        }
        send_ack(client_socket, server_address, data_packet->seqno);
        i++;
    }
    string content = "";
    for (int i = 0; i < numberOfPackets; i++)
    {
        content += fileContents[i];
    }
    writeFile(fileName, content);
    cout << "File is received successfully . " << endl;
    return 0;
}
