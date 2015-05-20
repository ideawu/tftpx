/**********************************************
 * Author: ideawu(www.ideawu.net)
 * Date: 2007-06
 * File: client.c
 *********************************************/

#include "client.h"

// Socket fd this client use.
int sock;
// Server address.
struct sockaddr_in server;
socklen_t addr_len;
int blocksize = DATA_SIZE;

void help(){
	printf("Usage: cmd  arg0[,arg1,arg2...]\n");
	printf("  -Directory listing:\n");
	printf("    list path\n");
	printf("  -Download a file from the server:\n");
	printf("    get remote_file[ local_file]\n");
	printf("  -Upload a file to the server:\n");
	printf("    put filename\n");
	printf("  -Set blocksize:\n");
	printf("    blocksize size\n");
	printf("  -Quit this programm:\n");
	printf("    quit\n");
}

int main(int argc, char **argv){
	char cmd_line[LINE_BUF_SIZE];
	char *buf;
	char *arg;
	int i;
	char *local_file;
	
	int done = 0;	// Server exit.
	char *server_ip;

	addr_len = sizeof(struct sockaddr_in);	
	
	if(argc < 2){
		printf("Usage: %s server_ip\n", argv[0]);
		return 0;
	}
	help();
	
	server_ip = argv[1];
	
	if((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
		printf("Server socket could not be created.\n");
		return 0;
	}
	
	// Initialize server address
	server.sin_family = AF_INET;
	server.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, server_ip, &(server.sin_addr.s_addr));
	
	// Command line interface.
	while(1){
		printf(">> ");
		memset(cmd_line, 0, LINE_BUF_SIZE);
		buf = fgets(cmd_line, LINE_BUF_SIZE, stdin);
		if(buf == NULL){
			printf("\nBye.\n");
			return 0;
		}
		
		arg = strtok (buf, " \t\n");
		if(arg == NULL){
			continue;
		}
		
		if(strcmp(arg, "list") == 0){
			arg = strtok (NULL, " \t\n");
			if(arg == NULL){
				printf("Error: missing arguments\n");
			}else{
				do_list(sock, arg);
			}
		}else if(strcmp(arg, "get") == 0){
			arg = strtok (NULL, " \t\n");
			local_file = strtok (NULL, " \t\n");
			if(arg == NULL){
				printf("Error: missing arguments\n");
			}else{
				if(local_file == NULL){
					local_file = arg;
				}
				do_get(arg, local_file);
			}
		}else if(strcmp(arg, "put") == 0){
			arg = strtok (NULL, " \t\n");
			if(arg == NULL){
				printf("Error: missing arguments\n");
			}else{
				do_put(arg);
			}
		}else if(strcmp(arg, "blocksize") == 0){
			arg = strtok (NULL, " \t\n");
			if(arg == NULL){
				printf("Error: missing arguments\n");
			}else{
				int blk = atoi(arg);
				if(blk > 0 && blk <= DATA_SIZE){
					blocksize = blk;
				}else{
					printf("Error: blocksize should be > 0 && <= DATA_SIZE\n");
				}
			}
		}else if(strcmp(arg, "quit") == 0){
			break;
		}else{
			printf("Unknow command.\n");
		}
		
	}
	return 0;
}

// Download a file from the server.
void do_get(char *remote_file, char *local_file){
	struct tftpx_packet snd_packet, rcv_packet;
	int next_block = 1;
	int recv_n;
	int total_bytes = 0;
	struct tftpx_packet ack;	
	struct sockaddr_in sender;
		
	int r_size = 0;
	int time_wait_data;
	ushort block = 1;
	
	// Send request.
	snd_packet.cmd = htons(CMD_RRQ);
	sprintf(snd_packet.filename, "%s%c%s%c%d%c", remote_file, 0, "octet", 0, blocksize, 0);
	sendto(sock, &snd_packet, sizeof(struct tftpx_packet), 0, (struct sockaddr*)&server, addr_len);
	
	FILE *fp = fopen(local_file, "w");
	if(fp == NULL){
		printf("Create file \"%s\" error.\n", local_file);
		return;
	}
	
	// Receive data.
	snd_packet.cmd = htons(CMD_ACK);
	do{
		for(time_wait_data = 0; time_wait_data < PKT_RCV_TIMEOUT * PKT_MAX_RXMT; time_wait_data += 20000){
			// Try receive(Nonblock receive).
			r_size = recvfrom(sock, &rcv_packet, sizeof(struct tftpx_packet), MSG_DONTWAIT,
					(struct sockaddr *)&sender,
					&addr_len);
			if(r_size > 0 && r_size < 4){
				printf("Bad packet: r_size=%d\n", r_size);
			}
			if(r_size >= 4 && rcv_packet.cmd == htons(CMD_DATA) && rcv_packet.block == htons(block)){
				printf("DATA: block=%d, data_size=%d\n", ntohs(rcv_packet.block), r_size - 4);
				// Send ACK.
				snd_packet.block = rcv_packet.block;
				sendto(sock, &snd_packet, sizeof(struct tftpx_packet), 0, (struct sockaddr*)&sender, addr_len);
				fwrite(rcv_packet.data, 1, r_size - 4, fp);
				break;
			}
			usleep(20000);
		}
		if(time_wait_data >= PKT_RCV_TIMEOUT * PKT_MAX_RXMT){
			printf("Wait for DATA #%d timeout.\n", block);
			goto do_get_error;
		}
		block ++;
	}while(r_size == blocksize + 4);
	//printf("\nReceived %d bytes.\n", total_bytes);
	
do_get_error:
	fclose(fp);
}


// Upload a file to the server.
void do_put(char *filename){
	struct sockaddr_in sender;
	struct tftpx_packet rcv_packet, snd_packet;
	int r_size = 0;
	int time_wait_ack;
	
	// Send request and wait for ACK#0.
	snd_packet.cmd = htons(CMD_WRQ);
	sprintf(snd_packet.filename, "%s%c%s%c%d%c", filename, 0, "octet", 0, blocksize, 0);	
	sendto(sock, &snd_packet, sizeof(struct tftpx_packet), 0, (struct sockaddr*)&server, addr_len);	
	for(time_wait_ack = 0; time_wait_ack < PKT_RCV_TIMEOUT; time_wait_ack += 20000){
		// Try receive(Nonblock receive).
		r_size = recvfrom(sock, &rcv_packet, sizeof(struct tftpx_packet), MSG_DONTWAIT,
				(struct sockaddr *)&sender,
				&addr_len);
		if(r_size > 0 && r_size < 4){
			printf("Bad packet: r_size=%d\n", r_size);
		}
		if(r_size >= 4 && rcv_packet.cmd == htons(CMD_ACK) && rcv_packet.block == htons(0)){
			break;
		}
		usleep(20000);
	}
	if(time_wait_ack >= PKT_RCV_TIMEOUT){
		printf("Could not receive from server.\n");
		return;
	}
	
	FILE *fp = fopen(filename, "r");
	if(fp == NULL){
		printf("File not exists!\n");
		return;
	}
	
	int s_size = 0;
	int rxmt;
	ushort block = 1;
	snd_packet.cmd = htons(CMD_DATA);
	// Send data.
	do{
		memset(snd_packet.data, 0, sizeof(snd_packet.data));
		snd_packet.block = htons(block);
		s_size = fread(snd_packet.data, 1, blocksize, fp);
		
		for(rxmt = 0; rxmt < PKT_MAX_RXMT; rxmt ++){
			sendto(sock, &snd_packet, s_size + 4, 0, (struct sockaddr*)&sender, addr_len);
			printf("Send %d\n", block);
			// Wait for ACK.
			for(time_wait_ack = 0; time_wait_ack < PKT_RCV_TIMEOUT; time_wait_ack += 20000){
				// Try receive(Nonblock receive).
				r_size = recvfrom(sock, &rcv_packet, sizeof(struct tftpx_packet), MSG_DONTWAIT,
						(struct sockaddr *)&sender,
						&addr_len);
				if(r_size > 0 && r_size < 4){
					printf("Bad packet: r_size=%d\n", r_size);
				}
				if(r_size >= 4 && rcv_packet.cmd == htons(CMD_ACK) && rcv_packet.block == htons(block)){
					break;
				}
				usleep(20000);
			}
			if(time_wait_ack < PKT_RCV_TIMEOUT){
				// Send success.
				break;
			}else{
				// Retransmission.
				continue;
			}
		}
		if(rxmt >= PKT_MAX_RXMT){
			printf("Could not receive from server.\n");
			return;
		}
		
		block ++;
	}while(s_size == blocksize);
	
	printf("\nSend file end.\n");
	
do_put_error:
	fclose(fp);
	
	return;
}


// Directory listing.
void do_list(int sock, char *dir){
	struct tftpx_packet packet;	
	int next_block = 1;
	int recv_n;
	struct tftpx_packet ack;	
	struct sockaddr_in sender;
	
	ack.cmd = htons(CMD_ACK);
	
	int r_size = 0;
	int time_wait_data;
	ushort block = 1;
	
	// Send request.
	packet.cmd = htons(CMD_LIST);
	strcpy(packet.data, dir);
	sendto(sock, &packet, sizeof(struct tftpx_packet), 0, (struct sockaddr*)&server, addr_len);
	
	printf("type\tsize\tname\n");
	printf("-------------------------------------------------\n");
	
	// Receive data.
	do{
		for(time_wait_data = 0; time_wait_data < PKT_RCV_TIMEOUT * PKT_MAX_RXMT; time_wait_data += 20000){
			// Try receive(Nonblock receive).
			r_size = recvfrom(sock, &packet, sizeof(packet), MSG_DONTWAIT,
					(struct sockaddr *)&sender,
					&addr_len);
			if(r_size > 0 && r_size < 4){
				printf("Bad packet: r_size=%d\n", r_size);
			}
			if(r_size >= 4 && packet.cmd == htons(CMD_DATA) && packet.block == htons(block)){
				block ++;
				ack.block = packet.block;
				sendto(sock, &ack, sizeof(struct tftpx_packet), 0, (struct sockaddr*)&sender, addr_len);
				fwrite(packet.data, 1, r_size - 4, stdout);
				break;
			}
			usleep(20000);
		}
		if(time_wait_data >= PKT_RCV_TIMEOUT * PKT_MAX_RXMT){
			printf("Wait for DATA #%d timeout.\n", block);
			return;
		}
	}while(r_size == blocksize + 4);
}
