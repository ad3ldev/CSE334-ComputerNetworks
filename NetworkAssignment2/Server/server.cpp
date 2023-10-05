#include "../packet_header.hpp"
#include "server_helper.hpp"

int port, random_seed;
double PLP;
vector<not_sent_packet> not_sent_packets;
vector<packet> sent_packets;

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
vector<string> read_args()
{
    string fileName = "server.in";
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
    port = stoi(arguments[0]);
    random_seed = stoi(arguments[1]);
    PLP = stod(arguments[2]);
    srand(random_seed);

    int server_socket, client_socket;

    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    int server_addrlen = sizeof(server_address);

    server_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    check(server_socket, "Error: creating server socket !");

    memset(&server_address, 0, sizeof(server_address));
    memset(&client_address, 0, sizeof(client_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;
    memset(&(server_address.sin_zero), '\0', ACK_PACKET_SIZE);

    check(bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)), "Error: binding server!");

    while (true)
    {
        socklen_t client_addrlen = sizeof(struct sockaddr);
        cout << "Waiting For A New Connection ... " << endl;
        char rec_buffer[MAXIMUM_SEGMENT_SIZE];
        ssize_t received_bytes = recvfrom(server_socket, rec_buffer, MAXIMUM_SEGMENT_SIZE, 0, (struct sockaddr *)&client_address, &client_addrlen);
        check(received_bytes - 1, "Error in receiving bytes of the file name!");
        /** forking to handle request **/
        pid_t pid = fork();
        if (pid == -1)
        {
            perror("Error: forking a child process for the client!");
        }
        else if (pid == 0)
        {
            client_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
            check(client_socket, "Error creating a socket for the client!");
            handle_client_request(server_socket, client_socket, client_address, rec_buffer, MAXIMUM_SEGMENT_SIZE);
            exit(0);
        }
    }
    close(server_socket);
    return 0;
}

void handle_client_request(int server_socket, int client_fd, struct sockaddr_in client_addr, char rec_buffer[], int bufferSize)
{

    auto *data_packet = (struct packet *)rec_buffer;
    string file_name = string(data_packet->data);
    cout << "Requested file name from client:\t" << file_name << "\nLength of file name: \t" << file_name.size() << endl;
    int file_size = does_file_exist(file_name);
    if (file_size == -1)
    {
        return;
    }
    int number_of_packets = ceil(file_size * 1.0 / CHUNK_SIZE);
    cout << "File Size:\t" << file_size << " Bytes\nNum. of Chunks:\t" << number_of_packets << endl;
    /** send ack to file name **/
    struct ack_packet ack;
    ack.cksum = 0;
    ack.len = number_of_packets;
    ack.ackno = 0;
    char *buf = new char[MAXIMUM_SEGMENT_SIZE];
    memset(buf, 0, MAXIMUM_SEGMENT_SIZE);
    memcpy(buf, &ack, sizeof(ack));
    ssize_t bytes_sent = sendto(client_fd, buf, MAXIMUM_SEGMENT_SIZE, 0, (struct sockaddr *)&client_addr, sizeof(struct sockaddr));
    check(bytes_sent, "Error Sending The Ack!");
    cout << "Ack of file name is sent successfully" << endl;
    /** read data from file **/
    vector<string> data_packets = read_file(file_name);
    if (data_packets.size() == number_of_packets)
    {
        cout << "File Data is read successfully" << endl;
    }
    /** start sending data and handling congestion control using the SM **/
    send_and_handle_congestion(client_fd, client_addr, data_packets);
}

void send_and_handle_congestion(int client_fd, struct sockaddr_in client_addr, vector<string> data)
{
    ofstream file_handler;
    // File Open
    file_handler.open("CWND.txt");

    int cwnd_base = 0;
    double cwnd = 1;
    file_handler << cwnd << endl;
    int base_packet_number = 0;
    int sst = 128;
    int seq_num = 0;
    long sent_packets_not_acked = 0;
    fsm_state st = slow_start;
    long num_dup_acks = 0;
    int last_acked_seq_num = -1;
    bool still_exist_acks = true;
    char rec_buf[MAXIMUM_SEGMENT_SIZE];
    socklen_t client_addrlen = sizeof(struct sockaddr);
    int total_packets = data.size();
    int already_sent_packets = 0;

    while (true)
    {

        // Sending the first datagram
        while (cwnd_base < cwnd && already_sent_packets + not_sent_packets.size() < total_packets)
        {
            seq_num = base_packet_number + cwnd_base;
            string temp_packet_string = data[seq_num];
            // In case error simulated won't send the packet so the seq_number will not correct at the receiver so will send duplicate ack.
            bool is_sent = send_packet(client_fd, client_addr, temp_packet_string, seq_num);
            if (is_sent == false)
            {
                perror("Error sending data packet!");
            }
            else
            {
                sent_packets_not_acked++;
                already_sent_packets++;
                cout << "Sent Seq Num:\t" << seq_num << endl;
            }
            cwnd_base++;
        }

        // Receiving ACKs
        if (sent_packets_not_acked > 0)
        {
            still_exist_acks = true;
            while (still_exist_acks)
            {
                cout << "waiting ack " << endl;
                ssize_t received_bytes = recvfrom(client_fd, rec_buf, ACK_PACKET_SIZE, 0, (struct sockaddr *)&client_addr, &client_addrlen);
                check(received_bytes, "Error receiving bytes!");
                if (received_bytes != ACK_PACKET_SIZE)
                {
                    cout << "Expecting Ack Got Something Else" << endl;
                    exit(1);
                }
                else
                {
                    auto ack = (ack_packet *)malloc(sizeof(ack_packet));
                    memcpy(ack, rec_buf, ACK_PACKET_SIZE);
                    cout << "Ack. " << ack->ackno << " Received." << endl;
                    if (get_ack_checksum(ack->len, ack->ackno) != ack->cksum)
                    {
                        cout << "Corrupt Ack. received" << endl;
                    }
                    int ack_seqno = ack->ackno;
                    if (last_acked_seq_num == ack_seqno)
                    {
                        num_dup_acks++;
                        sent_packets_not_acked--;
                        if (st == fast_recovery)
                        {
                            cwnd++;
                            file_handler << cwnd << endl;
                        }
                        else if (num_dup_acks == 3)
                        {
                            sst = cwnd / 2;
                            cwnd = sst + 3;
                            cout << "=================== Triple duplicate Ack ===========================" << endl;
                            file_handler << cwnd << endl;
                            st = fast_recovery;
                            // Retransmit the lost packet
                            seq_num = ack_seqno;
                            bool found = false;
                            for (int j = 0; j < not_sent_packets.size(); j++)
                            {
                                not_sent_packet n_s_pkt = not_sent_packets[j];
                                if (n_s_pkt.seqno == seq_num)
                                {
                                    found = true;
                                    string temp_packet_string = data[seq_num];
                                    struct packet data_packet = create_packet_data(temp_packet_string, seq_num);
                                    char send_buffer[MAXIMUM_SEGMENT_SIZE];
                                    memset(send_buffer, 0, MAXIMUM_SEGMENT_SIZE);
                                    memcpy(send_buffer, &data_packet, sizeof(data_packet));
                                    ssize_t bytes_sent = sendto(client_fd, send_buffer, MAXIMUM_SEGMENT_SIZE, 0, (struct sockaddr *)&client_addr, sizeof(struct sockaddr));
                                    check(bytes_sent, "Error re-sending data packet!");
                                    sent_packets_not_acked++;
                                    already_sent_packets++;
                                    not_sent_packets.erase(not_sent_packets.begin() + j);
                                    break;
                                }
                            }

                            // Handle checksum error
                            if (!found)
                            {
                                for (int j = 0; j < sent_packets.size(); j++)
                                {
                                    packet s_pkt = sent_packets[j];
                                    if (s_pkt.seqno == seq_num)
                                    {
                                        found = true;
                                        string temp_packet_string = data[seq_num];
                                        struct packet data_packet = create_packet_data(temp_packet_string, seq_num);
                                        char send_buffer[MAXIMUM_SEGMENT_SIZE];
                                        memset(send_buffer, 0, MAXIMUM_SEGMENT_SIZE);
                                        memcpy(send_buffer, &data_packet, sizeof(data_packet));
                                        ssize_t bytes_sent = sendto(client_fd, send_buffer, MAXIMUM_SEGMENT_SIZE, 0, (struct sockaddr *)&client_addr, sizeof(struct sockaddr));
                                        check(bytes_sent, "Error re-sending data packet!");
                                        already_sent_packets++;
                                        sent_packets.erase(sent_packets.begin() + j);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    else if (last_acked_seq_num < ack_seqno)
                    {
                        //  New ack : compute new base and packet no. and handling congestion control FSM
                        cout << "newAck " << endl;
                        num_dup_acks = 0;
                        last_acked_seq_num = ack_seqno;
                        int advance = last_acked_seq_num - base_packet_number;
                        cwnd_base = cwnd_base - advance;
                        base_packet_number = last_acked_seq_num;
                        if (st == slow_start)
                        {
                            if (cwnd * 2 >= sst)
                            {
                                st = congestion_avoidance;
                                cwnd++;
                            }
                            else
                            {
                                cwnd = cwnd * 2;
                            }
                            file_handler << cwnd << endl;
                            if (cwnd >= sst)
                            {
                                st = congestion_avoidance;
                            }
                        }
                        else if (st == congestion_avoidance)
                        {
                            cwnd++;
                            file_handler << cwnd << endl;
                        }
                        else if (st == fast_recovery)
                        {
                            st = congestion_avoidance;
                            cwnd = sst;
                            file_handler << cwnd << endl;
                        }
                        sent_packets_not_acked--;
                    }
                    else
                    {
                        sent_packets_not_acked--;
                    }
                    if (sent_packets_not_acked == 0)
                    {
                        still_exist_acks = false;
                    }
                }
            }
        }

        // Handle Time Out
        bool entered = false;
        for (int j = 0; j < not_sent_packets.size(); j++)
        {
            not_sent_packet n_s_pkt = not_sent_packets[j];
            chrono::time_point<chrono::system_clock> current_time = chrono::system_clock::now();
            chrono::duration<double> elapsed_time = current_time - n_s_pkt.timer;
            if (elapsed_time.count() >= 2)
            {
                entered = true;
                cout << "Timed Out ! " << endl;
                cout << "Re-transmitting the packet " << endl;
                seq_num = n_s_pkt.seqno;
                string temp_packet_string = data[seq_num];
                struct packet data_packet = create_packet_data(temp_packet_string, seq_num);
                char send_buffer[MAXIMUM_SEGMENT_SIZE];
                memset(send_buffer, 0, MAXIMUM_SEGMENT_SIZE);
                memcpy(send_buffer, &data_packet, sizeof(data_packet));
                ssize_t bytes_sent = sendto(client_fd, send_buffer, MAXIMUM_SEGMENT_SIZE, 0, (struct sockaddr *)&client_addr, sizeof(struct sockaddr));
                check(bytes_sent, "Error resending the data packet!");
                sent_packets_not_acked++;
                already_sent_packets++;
                not_sent_packets.erase(not_sent_packets.begin() + j);
                j--;
                cout << "Sent Seq Num : " << seq_num << endl;
            }
        }
        if (entered)
        {
            entered = false;
            cwnd = 1;
            st = slow_start;
            file_handler << cwnd << endl;
        }
    }
    file_handler.close();
}

bool send_packet(int client_fd, struct sockaddr_in client_addr, string temp_packet_string, int seq_num)
{
    char send_buffer[MAXIMUM_SEGMENT_SIZE];
    struct packet data_packet = create_packet_data(temp_packet_string, seq_num);
    bool corrupt = corrupt_datagram();
    if (corrupt)
    {
        data_packet.cksum = data_packet.cksum - 1;
    }
    memset(send_buffer, 0, MAXIMUM_SEGMENT_SIZE);
    memcpy(send_buffer, &data_packet, sizeof(data_packet));
    if (!drop_datagram() && !corrupt)
    {
        ssize_t bytes_sent = sendto(client_fd, send_buffer, MAXIMUM_SEGMENT_SIZE, 0, (struct sockaddr *)&client_addr, sizeof(struct sockaddr));
        if (bytes_sent == -1)
        {
            return false;
        }
        else
        {
            sent_packets.push_back(data_packet);
            return true;
        }
    }
    else
    {
        cout << "======================================Drop data" << endl;
        struct not_sent_packet n_s_packet;
        n_s_packet.seqno = seq_num;
        n_s_packet.done = false;
        n_s_packet.timer = chrono::system_clock::now();
        not_sent_packets.push_back(n_s_packet);
        return false;
    }
}

packet create_packet_data(string packet_string, int seq_num)
{

    struct packet p;
    memset(p.data, 0, 500);
    strcpy(p.data, packet_string.c_str());
    p.seqno = seq_num;
    p.len = packet_string.size();
    p.cksum = get_data_checksum(packet_string, p.len, p.seqno);
    return p;
}

long does_file_exist(string file_name)
{
    ifstream file(file_name.c_str(), ifstream::ate | ifstream::binary);
    if (!file.is_open())
    {
        cout << "Error Opening the requested file" << endl;
        return -1;
    }
    cout << "The file opened successfully" << endl;
    long len = file.tellg();
    file.close();
    return len;
}

vector<string> read_file(string file_name)
{

    vector<string> data_packets;
    string temp = "";
    ifstream fin;
    fin.open(file_name);
    if (fin)
    {
        char c;
        int char_counter = 0;
        while (fin.get(c))
        {
            if (char_counter < CHUNK_SIZE)
            {
                temp += c;
            }
            else
            {
                data_packets.push_back(temp);
                temp.clear();
                temp += c;
                char_counter = 0;
                continue;
            }
            char_counter++;
        }
        if (char_counter > 0)
        {
            data_packets.push_back(temp);
        }
    }
    fin.close();
    return data_packets;
}

uint16_t get_data_checksum(string content, uint16_t len, uint32_t seqno)
{
    uint32_t sum = 0;
    sum += len;
    sum += seqno;
    int n = content.length();
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

bool drop_datagram()
{
    int res = rand() % 100;
    double is_dropped = res * PLP;
    cout << "Is Dropped Value:\t" << is_dropped << endl;
    if (is_dropped >= 5.9)
    {
        return true;
    }
    return false;
}

bool corrupt_datagram()
{
    int res = rand() % 100;
    double is_corrupt = res * PLP;
    cout << "Is Corrupt Value:\t " << is_corrupt << endl;
    if (is_corrupt >= 5.9)
    {
        return true;
    }
    return false;
}

uint16_t get_ack_checksum(uint16_t len, uint32_t ackno)
{
    uint32_t sum = 0;
    sum += len;
    sum += ackno;
    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    uint16_t oc_sum = (uint16_t)(~sum);
    return oc_sum;
}
