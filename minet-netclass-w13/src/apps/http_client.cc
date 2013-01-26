#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>

#define BUFSIZE 1024

int write_n_bytes(int fd, char * buf, int count);
int readLine(int sock, char * buffer, int maxLength);
void printAll(int sock, FILE * printLocation);

int main(int argc, char * argv[]) {
    char * server_name = NULL;
    int server_port = 0;
    char * server_path = NULL;

    int sock = 0;
    int returnCode = -1;
    bool ok = true;
    struct sockaddr_in socketAddress;
    struct hostent * site = NULL;
    char * request = NULL;

    char buf[BUFSIZE + 1];
    char lineSplitBuffer[BUFSIZE + 1];
    struct timeval timeout;
    fd_set toWatch;

    /*parse args */
    if (argc != 5) {
	fprintf(stderr, "usage: http_client k|u server port path\n");
	exit(-1);
    }

    server_name = argv[2];
    server_port = atoi(argv[3]);
    server_path = argv[4];



    /* initialize minet */
    if (toupper(*(argv[1])) == 'K') {
	minet_init(MINET_KERNEL);
    } else if (toupper(*(argv[1])) == 'U') {
	minet_init(MINET_USER);
    } else {
	fprintf(stderr, "First argument must be k or u\n");
	exit(-1);
    }

    /* create socket */
    if((sock = minet_socket (SOCK_STREAM)) < 0) {
        minet_perror("Error creating socket");
        minet_deinit();
        return -1;
    }

    // Do DNS lookup
    /* Hint: use gethostbyname() */


    site = gethostbyname(server_name);
     if(site == NULL) {
        fprintf(stderr, "DNS could not resolve server name\n");
        minet_deinit();
        return -1;
    }

    //http://linux.die.net/man/3/gethostbyname
    /* set address */
    memset(&socketAddress, 0, sizeof(socketAddress));
    socketAddress.sin_port=htons(server_port);
    struct in_addr * ipAddressFromDNS = (struct in_addr *)(site->h_addr_list[0]);
    socketAddress.sin_addr = *ipAddressFromDNS;
    socketAddress.sin_family = AF_INET;

    if(minet_connect(sock, &socketAddress) < 0) {
        minet_perror("Error connecting socket");
        minet_deinit();
        return -1;
    }

    /* send request */
    char * requestHeader = (char * )"GET ";
    char * requestFooter = (char * )" HTTP/1.0\r\n\r\n";
    returnCode = write_n_bytes(sock, requestHeader, strlen(requestHeader)) == -1;
    returnCode = write_n_bytes(sock, server_path, strlen(server_path)) == -1 || returnCode;
    returnCode = write_n_bytes(sock, requestFooter, strlen(requestFooter)) == -1 || returnCode;
    if(returnCode) {
        fprintf(stderr, "Request could not be sent\n");
        minet_deinit();
        return -1;
    }

    FD_ZERO(&toWatch);
    FD_SET(sock, &toWatch);

    returnCode = minet_select(sock + 1, &toWatch, NULL, NULL, NULL);
    if(returnCode < 0) {
        minet_perror("Error in select");
        minet_deinit();
        return -1;
    }
    if(FD_ISSET(sock, &toWatch)) {
        int lineSize = readLine(sock, buf, BUFSIZE);
        char * version = strtok(buf, " ");
        char * statusCodeString = strtok(NULL, " ");
        char * phrase = strtok(NULL, "\0");
        int statusCode = atoi(statusCodeString);
        if(statusCode == 200) {
            while((lineSize = readLine(sock, buf, BUFSIZE)) != 0) {
                char * headerFieldName = strtok(buf, ":");
                char * headerValue = strtok(NULL, "\0");
                //The first character in headerValue is a space, so increment headerValue
                headerValue++;
                printf("%s | %s\n", headerFieldName, headerValue);
                //Pull data until the end of the line if the line is larger than the buffer
                while(lineSize == -1) {
                    lineSize = readLine(sock, buf, BUFSIZE);
                }
            }
            printAll(sock, stdout);
        } else {
            printf("%s %s %s\n", version, statusCodeString, phrase);
            printAll(sock, stderr);
            minet_deinit();
            return -1;
        }
    } else {
        fprintf(stderr, "The socket is not ready somehow (this shouldn't ever happen\n");
        minet_deinit();
        return -1;
    }




    /* wait till socket can be read */
    /* Hint: use select(), and ignore timeout for now. */

    /* first read loop -- read headers */

    /* examine return code */
    //Skip "HTTP/1.0"
    //remove the '\0'
    // Normal reply has return code 200

    /* print first part of response */

    /* second read loop -- print out the rest of the response */

    /*close socket and deinitialize */
    minet_deinit();
    return 0;
}

int write_n_bytes(int fd, char * buf, int count) {
    int rc = 0;
    int totalwritten = 0;

    while ((rc = minet_write(fd, buf + totalwritten, count - totalwritten)) > 0) {
	totalwritten += rc;
    }

    if (rc < 0) {
	return -1;
    } else {
	return totalwritten;
    }
}

//Reads characters into a buffer until cflf
//The buffer is filled with the current line
//Returns the length of the string
//Returns -1 if there is more to be read than can fit in the buffer
//maxLength is the max number of bytes read
//The buffer must be of size maxlen + 1
int readLine(int sock, char * buffer, int maxLength) {
    int bytesRead = 0;
    int totalBytesRead = 0;
    char * currentPosition = buffer;
    int amountToRead = maxLength;
    int crFound = 0;
    //bytesRead goes to 0 at end of file
    //amountToRead goes to 0 when we fill the buffer
    while(amountToRead > 0) {
        bytesRead = minet_read(sock, currentPosition, 1);
        if(bytesRead) {
            totalBytesRead += bytesRead;
            //If we find a cr, make a note
            if(*currentPosition == '\r') {
                crFound = 1;
            } else if(*currentPosition == '\n') {
                //If we found a line feed and the previous character was a carriage return, null terminate the string at the carriage return
                if(crFound) {
                    currentPosition[-1] = 0;
                    return totalBytesRead - 2;
                //If we just found a carriage return, that is not end of line according to http, so continue looking for crlf
                } else {
                    crFound = 0;
                }
            }
            currentPosition += bytesRead;
            amountToRead -= bytesRead;

        } else {
            //If bytesRead == 0, we have reached end of file
            currentPosition[0] = 0;
            return bytesRead;
        }
    }
    currentPosition[1] = 0;
    return -1;
}

void printAll(int sock, FILE * printLocation) {
    char b;
    int error;
    while((error = minet_read(sock, &b, 1)) != 0) {
        if(error < 0) {
            minet_perror("Error reading ");
            return;
        } else {
            fprintf(printLocation, "%c", b);
        }
    }
}
