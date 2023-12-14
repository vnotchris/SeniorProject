// modified from: https://pastebin.com/44u1gAQU

#include <stdio.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <linux/errqueue.h>
#include <execinfo.h>
#include <unistd.h>
#define MAX 409600000
#define PORT 8080
#define SA struct sockaddr


/* Obtain a backtrace and print it to stdout. */
void
print_trace (int sig)
{
	void *array[10];
	size_t size;
	char **strings;
	size_t i;

	size = backtrace (array, 10);
	strings = backtrace_symbols (array, size);

	printf ("Obtained %zd stack frames.\n", size);

	for (i = 0; i < size; i++)
		printf ("%s\n", strings[i]);

	free (strings);
	exit(1);
}




 
int funcwaitzc(int sockfd, int zeroCopyCounter)
{
    int currentProgress = 0;
    int ret;
 
    while (currentProgress < zeroCopyCounter)
    {
	printf("current progress: %d || zc counter: %d\n", currentProgress, zeroCopyCounter);
        struct msghdr msg = {};
        char msgData[512] = {};
	struct iovec iov[1];
	char buffer[80];
	memset(iov, 0, sizeof(struct iovec));
 
	iov[0].iov_base = buffer;
	iov[0].iov_len = 80;
        msg.msg_control = msgData; 
        msg.msg_controllen = sizeof(msgData) - sizeof(struct cmsghdr);
 
        ret = recvmsg(sockfd, &msg, MSG_ERRQUEUE);
 
        if (ret == -1 && errno == EAGAIN)
        {
            // keep going
            printf("keep going\n");
            sleep(1);
        }
        else if (ret == -1)
        {
            printf("error recvmsg notification\n");
            return -1;
        }
        else if (msg.msg_flags & MSG_CTRUNC)
        {
            printf("error recvmsg notification: truncated\n");
            return -1;
        }
        else
        {
            printf("this is ret, not -1 or error %d\n", ret);
//            printf("%.*s\n", msg.msg_iov->iov_len, msg.msg_iov->iov_base);
            struct cmsghdr *cm = CMSG_FIRSTHDR(&msg);
 
            if (cm->cmsg_level != SOL_IP)
            {
                printf("error SOL_IP\n");
                return -1;
            }
 
            if (cm->cmsg_type != IP_RECVERR)
            {
                printf("error IP_RECVERR\n");
                return -1;
            }
        
            struct sock_extended_err *serr = (struct sock_extended_err *) CMSG_DATA(cm);
        
            if (serr->ee_errno != 0)
            {
                printf("error serr->ee_errno\n");
                return -1;
            }

	    printf("about to check ZEROCOPY\n"); 
            if (serr->ee_origin != SO_EE_ORIGIN_ZEROCOPY)
            {
                printf("error SO_EE_ORIGIN_ZEROCOPY\n");
                return -1;
            }
	    printf("ZEROCPY check fine\n");
 
            printf("Waiting (%i, %i, %i, %i)\n", currentProgress, serr->ee_data, serr->ee_info, zeroCopyCounter);
 
	    currentProgress = currentProgress > serr->ee_data ? currentProgress : serr->ee_data;
        }
    }
 
    return 0;
}
  
// Function designed for chat between client and server.
void funcwrite(int sockfd)
{
    char* buff = (char*)malloc(MAX);
    memset(buff, 'b', MAX);
    int counter = 0;
 
    // infinite loop for chat
    for (int t = 0; t < 1; t++) 
    {
        printf("Send...\n");
        if (send(sockfd, buff, MAX, MSG_ZEROCOPY) < 0)
        {
            printf("Send error %s", strerror(errno));
            return;
        }
 
        counter++;
        
        if (funcwaitzc(sockfd, counter) < 0)
        {
            printf("ZC counter error");
            return;
        }        
    }
}
  
// Driver function
int main()
{
    int sockfd, connfd;
    socklen_t len;
    struct sockaddr_in servaddr, cli;
    int optValue = 1;
 
    setbuf(stdout, NULL);
  
    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) 
    {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
    {
        printf("Socket successfully created..\n");
    }
 
    // Setting the socket option only works when the socket is in its initial (TCP_CLOSED) state. 
    // Trying to set the option for a socket returned by accept(), for example, will lead to an EBUSY error. 
    // In this case, the option should be set to the listening socket and it will be inherited by the accepted sockets.
    if (setsockopt(sockfd, SOL_SOCKET, SO_ZEROCOPY, &optValue, sizeof(optValue)) != 0)
    {
        printf("zero copy flag failed...\n");
        exit(0);   
    }
    else
    {
        printf("zero copy enabled...\n");
    }
 
    bzero(&servaddr, sizeof(servaddr));
  
    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
  
    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) 
    {
        printf("socket bind failed (%s)...\n", strerror(errno));
        exit(0);
    }
    else
    {
 
        printf("Socket successfully binded..\n");
    }
 
 
    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0) 
    {
        printf("Listen failed...\n");
        exit(0);
    }
    else
    {
        printf("Server listening..\n");
    }
 
 
    len = sizeof(cli);
  
    // Accept the data packet from client and verification
    connfd = accept(sockfd, (SA*)&cli, &len);
    if (connfd < 0) 
    {
        printf("server acccept failed...\n");
        exit(0);
    }
    else
    {
        printf("server acccept the client...\n");
    }
  
    // Function for chatting between client and server
    funcwrite(connfd);
}

