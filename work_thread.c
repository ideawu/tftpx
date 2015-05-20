/**********************************************
 * Author: ideawu(www.ideawu.net)
 * Date: 2007-04, 2007-05
 * File: work_thread.c
 *********************************************/

#include "tftpx.h"
#include "work_thread.h"


// Send an ACK packet. Return bytes sent.
// If error occurs, return -1;
int send_ack(int sock, struct tftpx_packet *packet, int size){
	if(send(sock, packet, size, 0) != size){
		return -1;
	}
	
	return size;
}


// Send data and wait for an ACK. Return bytes sent.
// If error occurs, return -1;
int send_packet(int sock, struct tftpx_packet *packet, int size){
	struct tftpx_packet rcv_packet;
	int time_wait_ack = 0;
	int rxmt = 0;
	int r_size = 0;
	
	for(rxmt = 0; rxmt < PKT_MAX_RXMT; rxmt ++){
		printf("Send block=%d\n", ntohs(packet->block));
		if(send(sock, packet, size, 0) != size){
			return -1;
		}
		for(time_wait_ack = 0; time_wait_ack < PKT_RCV_TIMEOUT; time_wait_ack += 20000){
			usleep(20000);
			// Try receive(Nonblock receive).
			r_size = recv(sock, &rcv_packet, sizeof(struct tftpx_packet), MSG_DONTWAIT);
			if(r_size >= 4 && rcv_packet.cmd == htons(CMD_ACK) && rcv_packet.block == packet->block){
				//printf("ACK: block=%d\n", ntohs(rcv_packet.block));
				// Valid ACK
				break;
			}
		}
		if(time_wait_ack < PKT_RCV_TIMEOUT){
			break;
		}else{
			// Retransmission.
			continue;
		}
	}
	if(rxmt == PKT_MAX_RXMT){
		// send timeout
		printf("Sent packet exceeded PKT_MAX_RXMT.\n");
		return -1;
	}
	
	return size;
}


void *work_thread(void *arg){
	int sock;
	struct tftpx_request *request = (struct tftpx_request *)arg;
	struct sockaddr_in server;
	static socklen_t addr_len = sizeof(struct sockaddr_in);
	
	if(request->size <= 0){
		printf("Bad request.\n");
		return NULL;
	}
	
	printf("work_thread started.\n");
	
	if((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
		printf("work_thread socket could not be created.\n");
		return NULL;
	}
	
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = 0;

	if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0){
		printf("work_thread bind failed.\n");
		return NULL;
	}
	
	if(connect(sock, (struct sockaddr*)&(request->client), addr_len) < 0){
		printf("Can't connect to client.\n");
		return NULL;
	}
	
	// Choose handler
	switch(request->packet.cmd){
		case CMD_RRQ:
			printf("handle_rrq called.\n");
			handle_rrq(sock, request);
			break;
		case CMD_WRQ:
			printf("handle_wrq called.\n");
			handle_wrq(sock, request);
			break;
		case CMD_LIST:
			printf("handle_list called.\n");
			handle_list(sock, request);
			break;
		default:
			printf("Illegal TFTP operation.\n");
			break;
	}
	
	// test{
	// memset(request, 0, sizeof(struct tftpx_request));
	// }
	free(request);
	close(sock);
	return NULL;
}


void handle_list(int sock, struct tftpx_request *request){
	char fullpath[256];
	char data_buf[LIST_BUF_SIZE + 256 + 32];
	char *r_path = request->packet.data;	// request path
	int data_size;	// Size of data in data_buf[]
	struct tftpx_packet packet;
	
	if(strlen(r_path) + strlen(conf_document_root) > sizeof(fullpath) - 1){		
		printf("Request path too long. %d\n", strlen(r_path) + strlen(conf_document_root));
		return;
	}
	
	// build fullpath
	memset(fullpath, 0, sizeof(fullpath));
	strcpy(fullpath, conf_document_root);
	if(r_path[0] != '/'){
		strcat(fullpath, "/");
	}
	strcat(fullpath, r_path);
	
	printf("List %s\n", fullpath);
		
	struct stat stat_buf;
	struct dirent *dirent;
	DIR *dp;
	
	stat(fullpath, &stat_buf);
	if(!S_ISDIR(stat_buf.st_mode)){
		printf("\"%s\" is not a directory.\n", fullpath);
		packet.cmd = htons(CMD_ERROR);
		packet.code = 0;
		sprintf(packet.data, "\"%s\" is not a directory!\n", r_path);
		send_packet(sock, &packet, strlen(packet.data) + 4);
		return;
	}	
	
	char *ptr = fullpath + strlen(fullpath);
	data_size = 0;
	dp = opendir(fullpath);
	// fill dir list buffer
	while((dirent = readdir(dp)) != NULL){
		if(strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0){
			continue;
		}
		ptr[0] = '/';
		strcpy(ptr + 1, dirent->d_name);
		*(ptr + 1 + strlen(dirent->d_name)) = '\0';
		stat(fullpath, &stat_buf);
		
		char mod = S_ISDIR(stat_buf.st_mode)? 'd' : 'f';	//
		data_size += sprintf(data_buf + data_size,
				"%c\t%d\t%s\t\n",
				mod, (int)stat_buf.st_size, dirent->d_name);
		if(data_size >= LIST_BUF_SIZE){
			// Too much files in a directory!
			printf("SEND_BUF_SIZE too small!\n");
			return;
		}
	}
	//printf("%s", data_buf);
	//return;
	
	ushort block;
	packet.cmd = htons(CMD_DATA);
	for(block=1; block<data_size/DATA_SIZE + 1; block++){
		// Network byte order.
		packet.block = htons(block);
		memcpy(packet.data, data_buf + DATA_SIZE * (block - 1), DATA_SIZE);
		if(send_packet(sock, &packet, DATA_SIZE + 4) == -1){
			printf("Error occurs when sending Packet %d.", block);
			return;
		}
		usleep(PKT_TIME_INTERVAL);
	}
	// Now, send the last packet.
	packet.block = htons(block);
	memcpy(packet.data, data_buf + DATA_SIZE * (block - 1), data_size - DATA_SIZE * (block - 1));
	if(send_packet(sock, &packet, data_size - DATA_SIZE * (block - 1) + 4) == -1){
		printf("Error occurs when sending the last packet.");
		return;
	}
	
	printf("Sent %d packet(s).\n", block);
}



void handle_rrq(int sock, struct tftpx_request *request){
	struct tftpx_packet snd_packet;
	char fullpath[256];
	char *r_path = request->packet.filename;	// request file
	char *mode = r_path + strlen(r_path) + 1;
	char *blocksize_str = mode + strlen(mode) + 1;
	int blocksize = atoi(blocksize_str);

	if(blocksize <= 0 || blocksize > DATA_SIZE){
		blocksize = DATA_SIZE;
	}

	if(strlen(r_path) + strlen(conf_document_root) > sizeof(fullpath) - 1){		
		printf("Request path too long. %d\n", strlen(r_path) + strlen(conf_document_root));
		return;
	}
	
	// build fullpath
	memset(fullpath, 0, sizeof(fullpath));
	strcpy(fullpath, conf_document_root);
	if(r_path[0] != '/'){
		strcat(fullpath, "/");
	}
	strcat(fullpath, r_path);

	printf("rrq: \"%s\", blocksize=%d\n", fullpath, blocksize);
	
	//if(!strncasecmp(mode, "octet", 5) && !strncasecmp(mode, "netascii", 8)){
	//	// send error packet
	//	return;
	//}
	
	FILE *fp = fopen(fullpath, "r");
	if(fp == NULL){
		printf("File not exists!\n");
		// send error packet
		//snd_packet.cmd = htons(CMD_ERROR);
		//snd_packet.code = htons(1);
		//strcpy(snd_packet.data, "File not found!");
		//send_ack(sock, &snd_packet, 4 + strlen(snd_packet.data) + 1);
		return;
	}
	
	int s_size = 0;
	ushort block = 1;
	snd_packet.cmd = htons(CMD_DATA);
	do{
		memset(snd_packet.data, 0, sizeof(snd_packet.data));
		snd_packet.block = htons(block);
		s_size = fread(snd_packet.data, 1, blocksize, fp);
		if(send_packet(sock, &snd_packet, s_size + 4) == -1){
			fprintf(stderr, "Error occurs when sending packet.block = %d.\n", block);
			goto rrq_error;
		}
		block ++;
	}while(s_size == blocksize);
	
	printf("\nSend file end.\n");
	
rrq_error:
	fclose(fp);
	
	return;
}



void handle_wrq(int sock, struct tftpx_request *request){
	struct tftpx_packet ack_packet, rcv_packet;
	char fullpath[256];
	char *r_path = request->packet.filename;	// request file
	char *mode = r_path + strlen(r_path) + 1;
	char *blocksize_str = mode + strlen(mode) + 1;	
	int blocksize = atoi(blocksize_str);

	if(blocksize <= 0 || blocksize > DATA_SIZE){
		blocksize = DATA_SIZE;
	}

	if(strlen(r_path) + strlen(conf_document_root) > sizeof(fullpath) - 1){		
		printf("Request path too long. %d\n", strlen(r_path) + strlen(conf_document_root));
		return;
	}
	
	// build fullpath
	memset(fullpath, 0, sizeof(fullpath));
	strcpy(fullpath, conf_document_root);
	if(r_path[0] != '/'){
		strcat(fullpath, "/");
	}
	strcat(fullpath, r_path);

	printf("wrq: \"%s\", blocksize=%d\n", fullpath, blocksize);
	
	//if(!strncasecmp(mode, "octet", 5) && !strncasecmp(mode, "netascii", 8)){
	//	// send error packet
	//	return;
	//}
	
	FILE *fp = fopen(fullpath, "r");
	if(fp != NULL){
		// send error packet
		fclose(fp);
		printf("File \"%s\" already exists.\n", fullpath);
		return;
	}
	
	fp = fopen(fullpath, "w");
	if(fp == NULL){
		printf("File \"%s\" create error.\n", fullpath);
		return;
	}
	
	ack_packet.cmd = htons(CMD_ACK);
	ack_packet.block = htons(0);
	if(send_ack(sock, &ack_packet, 4) == -1){
		fprintf(stderr, "Error occurs when sending ACK = %d.\n", 0);
		goto wrq_error;
	}	

	int s_size = 0;
	int r_size = 0;
	ushort block = 1;
	int time_wait_data;
	do{		
		for(time_wait_data = 0; time_wait_data < PKT_RCV_TIMEOUT * PKT_MAX_RXMT; time_wait_data += 20000){
			// Try receive(Nonblock receive).
			r_size = recv(sock, &rcv_packet, sizeof(struct tftpx_packet), MSG_DONTWAIT);
			if(r_size > 0 && r_size < 4){
				printf("Bad packet: r_size=%d, blocksize=%d\n", r_size, blocksize);
			}
			if(r_size >= 4 && rcv_packet.cmd == htons(CMD_DATA) && rcv_packet.block == htons(block)){
				printf("DATA: block=%d, data_size=%d\n", ntohs(rcv_packet.block), r_size - 4);
				// Valid DATA
				fwrite(rcv_packet.data, 1, r_size - 4, fp);
				break;
			}
			usleep(20000);
		}
		if(time_wait_data >= PKT_RCV_TIMEOUT * PKT_MAX_RXMT){
			printf("Receive timeout.\n");
			goto wrq_error;
			break;
		}
		
		ack_packet.block = htons(block);
		if(send_ack(sock, &ack_packet, 4) == -1){
			fprintf(stderr, "Error occurs when sending ACK = %d.\n", block);
			goto wrq_error;
		}
		printf("Send ACK=%d\n", block);
		block ++;
	}while(r_size == blocksize + 4);
		
	printf("Receive file end.\n");
	
wrq_error:
	fclose(fp);
	
	return;
}
