/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include "http-request.h"
#include "http-response.h"

using namespace std;

#define MAXHOSTNAME 256
#define MAXBUFFERSIZE 512
#define PORTNUMBER 15577
#define MTU 1500 //Maximum Transmission Unit

/* SOCKET FUNCTIONS */

// Set up socket and listen for connections
// Return socket number
int establishConnection (unsigned short portNumber)
{
  // command line parsing
  char hostName[MAXHOSTNAME+1];
  int mySocket;
  struct sockaddr_in sa;
  struct hostent *hp;
  
  memset(&sa, 0, sizeof(struct sockaddr_in));
  gethostname(hostName, MAXHOSTNAME);
  hp = gethostbyname(hostName);
  if(hp == NULL)
    return(-1);  //no address
  sa.sin_family = hp->h_addrtype;
  sa.sin_port = htons(portNumber);
  if((mySocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return(-1);
  if(bind(mySocket,(struct sockaddr *) &sa, sizeof(struct sockaddr_in)) < 0) {
    close(mySocket);
    return(-1);
  }
  listen(mySocket, 20); // listen to a maximum of 20 connections
  cerr << "socket established";
  return(mySocket);
}

// Accept connections
// Input listening socket
// Return new connected socket
int getConnection(int socket) 
{
  int connectionSocket;
  socklen_t addrLen;
  struct sockaddr_in sa;

  addrLen = sizeof(sa);
  cerr << "socket attempting to accept";
  if((connectionSocket = accept(socket, (struct sockaddr*) &sa, &addrLen)) < 0)
    return(-1);
  cerr << "socket connected";
  return(connectionSocket);
}

// Read request from connected socket
// Input connected socket, buffer, amount to read
// Return bytes read
int readRequest(int socket, char *buf, int numChars)
{
  int charCount = 0;
  int charsRead = 0;
  if((charsRead = recv(socket, buf, numChars-charCount, 0)) < 0)
    return(-1);
  charCount += charsRead;
  cerr << charCount << endl;
  return charCount;
}

// Read request from connected socket
// Input connected socket, buffer, amount to read
// Return bytes read
int readResponse(int socket, char *buf, int numChars)
{
  fd_set fd;
  struct timeval timeout;
  int n, ready;
  int charCount = 0;
  int charsRead = 0;
  
  // Set up select
  FD_ZERO(&fd);
  FD_SET(socket, &fd);
  n = socket + 1;
  timeout.tv_sec = 1;
  timeout.tv_usec = 500000;

  // Read until timeout
  while(charCount < numChars){
    if((ready = select(n, &fd, NULL, NULL, &timeout)) < 0) {
      perror("select");
      return(-1);
    }
    else if(ready == 0)
      return(charCount);
    if((charsRead = read(socket, buf, numChars-charCount)) > 0) {
      charCount += charsRead;
      buf += charsRead;
      cerr << charCount << endl;
    }
    else
      return(-1);
  }
  return charCount;
}

// Write data to connected socket
// Input connected socket, buffer, amount to write
// Return bytes written
int writeData(int socket, char *buf, int numChars)
{
  int charCount = 0;
  int charsWritten = 0;
  cerr << "Trying to write " << numChars << endl;
  while(charCount < numChars) {
    if((charsWritten = send(socket, buf, numChars-charCount, 0)) > 0) {
      charCount += charsWritten;
      buf += charsWritten;
      cerr << charCount << endl;
    }
    else if(charsWritten < 0)
      return(-1);
  }
  return charCount;
}

// Connect to remote server
// Input httprequest
// Return connected socket
int callSocket(HttpRequest request)
{
  struct sockaddr_in sa;
  struct hostent *hp;
  int remoteSocket;
  
  // Get remote server address
  if((hp = gethostbyname(request.GetHost().c_str())) == NULL) {
    errno = ECONNREFUSED;
    return(-1);
  }
  memset(&sa, 0, sizeof(sa));
  memcpy((char*) &sa.sin_addr, hp->h_addr, hp->h_length);
  sa.sin_family = hp->h_addrtype;
  sa.sin_port = htons(request.GetPort());
  
  // Get socket
  if((remoteSocket = socket(hp->h_addrtype, SOCK_STREAM, 0)) < 0)
    return(-1);
  
  if(connect(remoteSocket,(struct sockaddr *) &sa, sizeof(struct sockaddr_in)) < 0){
    close(remoteSocket);
    return(-1);
  }
  return(remoteSocket);
}

// Process connection from connected socket
// Input connected socket
void processConnection(int socket)
{
  char* reqInBuf = new char[MAXBUFFERSIZE];
  char* reqOutBuf;
  char* respInBuf = new char[MTU];
  char* respOutBuf;
  int bytesRead;
  HttpRequest request;
  HttpResponse response;
  int remoteSocket;

  // Read request from socket
  if((bytesRead = readRequest(socket, reqInBuf, MAXBUFFERSIZE)) < 0){
    close(socket);
    exit(1);
  }
  // Read http request
  request.ParseRequest(reqInBuf, bytesRead);
  size_t reqLen = request.GetTotalLength();
  reqOutBuf = new char[reqLen];
  request.FormatRequest(reqOutBuf);
  cerr << reqOutBuf;

  // Forward http request to remote server
  remoteSocket = callSocket(request);
  if(writeData(remoteSocket, reqOutBuf, reqLen) < 0){
    close(remoteSocket);
    exit(1);
  }
  
  // Read response
  if((bytesRead = readResponse(remoteSocket, respInBuf, MTU)) < 0){
    close(remoteSocket);
    exit(1);
  }
  cerr << respInBuf;

  // Send response to client
  if(writeData(socket, respInBuf, bytesRead) < 0) {
    close(remoteSocket);
    exit(1);
  }
}

/* UTILITY FUNCTIONS */
//kill zombie processes
void zombieKiller(int i)
{
  while(waitpid(-1, NULL, WNOHANG) > 0)
    ;
}

int main (int argc, char *argv[])
{
  int ourSocket;
  int connectionSocket;

  if((ourSocket = establishConnection(PORTNUMBER)) < 0){
    perror("establish");
    exit(1);
  }

  signal(SIGCHLD, zombieKiller);

  for(;;){
    if((connectionSocket = getConnection(ourSocket)) < 0){
      if(errno == EINTR)
        continue;
      perror("accept");
      exit(1);
    }
    switch(fork()){
    case -1:
      perror("fork");
      close(ourSocket);
      close(connectionSocket);
      exit(1);
      break;
    case 0:
      cerr << "About to process";
      close(ourSocket);
      processConnection(connectionSocket);
      exit(0);
    default:
      close(connectionSocket);
      continue;
    }
  }
}
