/**********************************************
 * Author: ideawu(www.ideawu.net)
 * Date: 2007-04
 * File: tftpx.h
 *********************************************/

#ifndef TFTPX_H
#define TFTPX_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <dirent.h>


#define CMD_RRQ (short)1
#define CMD_WRQ (short)2
#define CMD_DATA (short)3
#define CMD_ACK (short)4
#define CMD_ERROR (short)5
#define CMD_LIST (short)6
#define CMD_HEAD (short)7


// Without a '/' at the end.
char *conf_document_root;


#define SERVER_PORT 10220
// Max request datagram size
#define MAX_REQUEST_SIZE 1024
// TFTPX_DATA_SIZE
#define DATA_SIZE 512
//
#define LIST_BUF_SIZE (DATA_SIZE * 8)


// Max packet retransmission.
#define PKT_MAX_RXMT 3
// usecond
#define PKT_SND_TIMEOUT 12*1000*1000
#define PKT_RCV_TIMEOUT 3*1000*1000
// usecond
#define PKT_TIME_INTERVAL 5*1000


struct tftpx_packet{
	ushort cmd;
	union{
		ushort code;
		ushort block;
		// For a RRQ and WRQ TFTP packet
		char filename[2];
	};
	char data[DATA_SIZE];
};

struct tftpx_request{
	int size;
	struct sockaddr_in client;
	struct tftpx_packet packet;
};

#endif

/*
Error Codes

   Value     Meaning

   0         Not defined, see error message (if any).
   1         File not found.
   2         Access violation.
   3         Disk full or allocation exceeded.
   4         Illegal TFTP operation.
   5         Unknown transfer ID.
   6         File already exists.
   7         No such user.
*/
