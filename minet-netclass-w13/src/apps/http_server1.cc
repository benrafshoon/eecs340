#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUFSIZE 1024
#define FILENAMESIZE 100

int handle_connection(int);
int writenbytes(int,char *,int);
int readnbytes(int,char *,int);
int readLine(int sock, char * buffer, int maxLength);
void printAll(int sock, FILE * printLocation);

int main(int argc,char *argv[])
{
    int server_port;
    int listeningSocket, acceptedSocket;
    struct sockaddr_in listeningSocketAddress, acceptedSocketAddress;
    int rc;

    /* parse command line args */
    if (argc != 3)
    {
    fprintf(stderr, "usage: http_server1 k|u port\n");
    exit(-1);
    }
    server_port = atoi(argv[2]);
    if (server_port < 1500)
    {
    fprintf(stderr,"INVALID PORT NUMBER: %d; can't be < 1500\n",server_port);
    exit(-1);
    }

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
    if((listeningSocket = minet_socket (SOCK_STREAM)) < 0) {
        minet_perror("Error creating socket");
        minet_deinit();
        return -1;
    }

    /* set server address*/
    memset(&listeningSocketAddress, 0, sizeof(listeningSocketAddress));
    listeningSocketAddress.sin_port = htons(server_port);
    listeningSocketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    listeningSocketAddress.sin_family = AF_INET;

    /* bind listening socket */
    if(minet_bind(listeningSocket, &listeningSocketAddress) < 0) {
        minet_perror("Error binding socket");
        minet_deinit();
        return -1;
    }
    //Start listening

    if(minet_listen(listeningSocket, 1) < 0) {
        minet_perror("Error listening on socket");
        minet_deinit();
        return -1;
    }


    /* start listening */

    /* connection handling loop */
    while(1) {
        if((acceptedSocket = minet_accept(listeningSocket, &acceptedSocketAddress)) < 0) {
            minet_perror("Error accepting socket");
            minet_deinit();
            return -1;
        }
        /* handle connections */
        rc = handle_connection(acceptedSocket);
    }


}

int handle_connection(int sock) {


  char fileName[FILENAMESIZE+1];
  int fileSize;
  int rc;
  int fd;
  struct stat filestat;
  char buf[BUFSIZE+1];
  char *headers;
  char *endheaders;
  char *bptr;
  int datalen=0;
  char *ok_response_f = "HTTP/1.0 200 OK\r\n"\
                      "Content-type: text/plain\r\n"\
                      "Content-length: %d \r\n\r\n";
  char ok_response[100];
  char *notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"\
                         "Content-type: text/html\r\n\r\n"\
                         "<html><body bgColor=black text=white>\n"\
                         "<h2>404 FILE NOT FOUND</h2>\n"
                         "</body></html>\n";
  bool ok=true;



    //Parse method, url and version from first line of request
    int lineSize = readLine(sock, buf, BUFSIZE);
    char * method = strtok(buf, " ");
    char * url = strtok(NULL, " ");
    char * version = strtok(NULL, "\0");

    //Copy the filename from url to fileName buffer
    //Copy url+1 because the first character in url is '/' and we want to do a relative file path
    sprintf(fileName, "%s", url + 1);
    printf("File name requested: %s\n", fileName);


    printf("Method: %s\n", method);
    printf("Version: %s\n", version);
    printf("URL: %s\n", url);

    //Go through headers (this isn't really necessary for this project)
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
  /* first read loop -- get request and headers*/

  /* parse request to get file name */
  /* Assumption: this is a GET request and filename contains no spaces*/

    //Open the requested file
    int file = open(fileName, O_RDONLY);
    //Negative file descriptor means the file name is invalid
    if(file < 0) {
        ok = 0;
    } else {
        //Otherwise, get fileStats to check the fileSize
        struct stat fileStats;
        //If stats returns without an error
        if(stat(fileName, &fileStats) >= 0) {
            //Mark the file as ok and record the file size
            ok = 1;
            fileSize = fileStats.st_size;
            printf("File size: %i\n", fileSize);
        }
    }


    /* try opening the file */

  /* send response */
  if (ok)
  {
      //Send the response headers
      int responseLength = sprintf(buf, ok_response_f, fileSize);
      printf("Response: %s\n", buf);
      int stats = writenbytes(sock, buf, responseLength);

      if(stats < 0) {
        printf("Error writing to socket\n");
      }

      //Loop through file BUFSIZE bytes at a time and send BUFSIZE bytes
      int sizeSent = 0;
      while(sizeSent < fileSize) {
          int sizeRead = read(file, buf, BUFSIZE);
          if(sizeRead < 0) {
              printf("Error reading file\n");
          }
          int writeCode = writenbytes(sock, buf, sizeRead);
          if(writeCode < 0) {
              printf("Error writing to socket\n");
          }
          sizeSent += sizeRead;
      }
    /* send headers */

    /* send file */
  }
  else // send error response
  {
      //If the file is invalid, send the invalid (404) response
      writenbytes(sock, notok_response, strlen(notok_response));
  }

  /* close socket and free space */

    minet_close(sock);
  if (ok)
    return 0;
  else
    return -1;
}

int readnbytes(int fd,char *buf,int size)
{
  int rc = 0;
  int totalread = 0;
  while ((rc = minet_read(fd,buf+totalread,size-totalread)) > 0)
    totalread += rc;

  if (rc < 0)
  {
    return -1;
  }
  else
    return totalread;
}

int writenbytes(int fd,char *str,int size)
{
  int rc = 0;
  int totalwritten =0;
  while ((rc = minet_write(fd,str+totalwritten,size-totalwritten)) > 0)
    totalwritten += rc;

  if (rc < 0)
    return -1;
  else
    return totalwritten;
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

