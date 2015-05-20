/**********************************************
 * Author: ideawu(www.ideawu.net)
 * Date: 2007-04
 * File: work_thread.h
 *********************************************/

void *work_thread(void *arg);

int send_ack(int sock, struct tftpx_packet *packet, int size);
int send_packet(int sock, struct tftpx_packet *packet, int size);

// Handlers
void handle_list(int sock, struct tftpx_request *request);
void handle_head(int sock, struct tftpx_request *request);

void handle_rrq(int sock, struct tftpx_request *request);
void handle_wrq(int sock, struct tftpx_request *request);
