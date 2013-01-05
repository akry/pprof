#include <stdio.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <pprof.h>


struct process_struct {
	long sec;
	long usec;
	pid_t pid;	// pid_t is alias of int
	pid_t tgid;
	//int coreid;
	long state;
};

//#define MAX_PAYLOAD 1024 /* maximum payload size*/
#define MAX_PAYLOAD sizeof(struct process_struct) /* maximum payload size*/
struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
int sock_fd;
struct msghdr msg;
FILE *result_binary;

/* 
 * establishing connection with libmonitor
 * libmonitor sends a packet when a thread terminated
 */
// void *connection_from_libmonitor(void *arg)
// {
// 	int sockfd, newsockfd;
// 	int portno=10000;
// 	struct sockaddr_in addr, cli_addr;
// 	socklen_t clilen;
// 
// 	sockfd = socket(AF_INET, SOCK_STREAM, 0);
// 	if(sockfd < 0) {
// 		printf("[Error][netlink_client][connection_from_libmonitor]: cannot create a socket.\n");
// 		return;
// 	}
// 	else {
// 		printf("[netlink_client][connection_from_libmonitor]: socket created.\n");
// 	}
// 
// 	addr.sin_family = AF_INET;
// 	addr.sin_addr.s_addr = INADDR_ANY;
// 	addr.sin_port = htons(portno);
// 	while(1) {	
// 		if(bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
// 			printf("[Error][netlink_client][connection_from_libmonitor]: port %d is in use.\n", portno);
// 			printf("[Error][netlink_client][connection_from_libmonitor]: try another port\n");
// 			portno++;
// 		}
// 		else {
// 			printf("[netlink_client][connection_from_libmonitor]: bind.\n");
// 			break;
// 		}
// 	}
// 	listen(sockfd, 5);
// 	clilen = sizeof(cli_addr);
// 	newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
// 	if(newsockfd < 0) {
// 		printf("[Error][netlink_client][connection_from_libmonitor]: cannot establish the connection to the libmonitor.\n");
// 		printf("[Error][netlink_client][connection_from_libmonitor]: %s\n", strerror(newsockfd));
// 		//return;
// 	}
// 	else {
// 		printf("[netlink_client][connection_from_libmonitor]: accept.\n");
// 	}
// 
// 	//struct process_struct p;
// 	//read(newsockfd, &p, sizeof(p));
// 	//printf("%ld %ld %d %d %ld\n", p.sec, p.usec, p.pid, p.tgid, p.state);
// 	// while(1) {
// 	// 	read(newsockfd, &p, sizeof(p));
// 	// 	printf("%ld %ld %d %d %ld\n", p.sec, p.usec, p.pid, p.tgid, p.state);
// 	// 	//fwrite(&p, sizeof(struct process_struct), 1, result_binary);
// 	// }
// 		
// 
// char buffer[256];
// bzero(buffer, 256);
// int n;
// n = read(newsockfd,buffer,255);
// if(n < 0) printf("error");
// printf("Here is the message: %s\n", buffer);
// n = write(newsockfd,"I got your message",18);
// if(n < 0) printf("error");
// close(newsockfd);
// close(sockfd);
// 	return;
// }

/* 
 * establishing connection with kernel
 * kernel sends packets when the status of threads changed
 */
void *netlink_connection(void *arg)
{
	struct process_struct p;

	sock_fd=socket(PF_NETLINK, SOCK_RAW, NETLINK_USERSOCK);
	if(sock_fd<0) {
		printf("[Error][pprofd][netlink_connection]: inet socket is not created.\n");
		return;
	}
	else {
		printf("[pprofd][netlink_connection]: inet socket created.\n");
	}
	
	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid(); /* self pid */
	
	bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr));
	printf("[pprofd][netlink_connection]: inet bind.\n");
	
	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = 0; /* For Linux Kernel */
	dest_addr.nl_groups = 0; /* unicast */
	
	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;
	printf("[pprofd][netlink_connection]: pprofd_pid = %d\n", nlh->nlmsg_pid);
	
	//strcpy(NLMSG_DATA(nlh), "Hello");
	memcpy(NLMSG_DATA(nlh), &p, sizeof(struct process_struct));
	
	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	msg.msg_name = (void *)&dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	
	printf("Sending message to kernel\n");
	sendmsg(sock_fd,&msg,0);
	
	/* Read message from kernel */
	struct process_struct *pp;
	printf("Waiting for message from kernel\n");
	while(1) {
		recvmsg(sock_fd, &msg, 0);
		pp = NLMSG_DATA(nlh);
		fprintf(result_binary, "%ld %ld %d %d %ld\n", pp->sec, pp->usec, pp->pid, pp->tgid, pp->state);
		//fwrite(pp, sizeof(struct process_struct), 1, result_binary);
	}
	//printf("Received message payload: %s\n", NLMSG_DATA(nlh));
	close(sock_fd);
	return;
}

void main()
{
	result_binary=fopen("result.txt", "w");

	pthread_t libmonitor_connection_thread;
	pthread_t netlink_thread;

	int libmon_status;
	int kernel_status;
	
	// libmon_status = pthread_create(&libmonitor_connection_thread, NULL, connection_from_libmonitor, NULL);
	// if(libmon_status != 0) {
	// 	printf("[Error][netlink_client]: cannot create thread for TCP connection.\n");
	// 	return;
	// }
	// else {
	// 	printf("[netlink_client]: thread created. ID=%d\n", libmonitor_connection_thread);
	// }

	kernel_status = pthread_create(&netlink_thread, NULL, netlink_connection, NULL);
	if(kernel_status != 0) {
		printf("[Error][pprofd]: cannot create netlink_thread.\n");
		return;
	}
	else {
		printf("[pprofd]: thread created. ID=%d\n", netlink_thread);
	}

	// pthread_join(libmonitor_connection_thread, NULL);
	pthread_join(netlink_thread, NULL);


}

