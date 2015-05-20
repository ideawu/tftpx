# TFTP的扩展和实现

这个项目是我在大学时学习时开发的, 后来成为几乎所有人的课堂作业. [http://www.ideawu.net/person/tftpx/](http://www.ideawu.net/person/tftpx/)

用C语言开发的在Linux平台上的TFTP([RFC1380](http://www.ietf.org/rfc/rfc2348.txt))服务器端和客户端. 支持目录列表, 可变块大小([RFC2348](http://www.ietf.org/rfc/rfc2348.txt)). 传输模式只支持二进制模式.

停止等待机制作为数据传输的基本机制, 是网络编程必须的掌握的技能. TFTP 协议使用基于UDP的停止等待机制来实现文件的可靠传输.

在查看 tftpx 的源码之前, 你最好先阅读 W.Richard.Stevens 的 TCP/IP Illustrated Volume 1: The Protocols(TCP/IP详解 卷1:协议).

tftpx 使用这样的代码来实现停止等待机制:

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
