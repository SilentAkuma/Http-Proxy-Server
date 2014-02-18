/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/poll.h>
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
/*
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
*/

// Read request from connected socket
// Input connected socket, buffer, amount to read
// Return bytes read
int readData(int socket, char *buf, int numChars)
{
  struct pollfd pfd[1];
  int timeout = 2000; //2 second timeout
  int n, ready;
  int charCount = 0;
  int charsRead = 0;
  
  // Clear buffer
  memset(buf, 0, numChars);

  // Set up poll
  pfd[0].fd = socket;
  pfd[0].events = POLLIN;
  n = 1;

  // Read until timeout
  while(charCount < numChars){
    if((ready = poll(pfd, 1, timeout)) < 0) {
      perror("poll");
      return(-1);
    }
    else if(ready == 0)
      return(charCount);
    else {
      if(pfd[0].revents & POLLHUP) {
        cerr << "HANGUP" << endl;
        break;
      }
      if(pfd[0].revents & POLLNVAL) {
        cerr << "NVAL" << endl;
        break;
      }
      if(pfd[0].revents & POLLERR) {
        cerr << "ERR" << endl;
        break;
      }
      if(pfd[0].revents & POLLIN) {
        cerr << "TRYING TO READ" << endl;
        if((charsRead = recv(socket, buf, numChars-charCount, 0)) >= 0) {
          if(charsRead == 0)
            break;
          charCount += charsRead;
          buf += charsRead;
          cerr << charCount << endl;
          //cerr << (buf-charCount) << endl;
        }
        else
          return(-1);
      }
    }
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
      cerr << "charCount: " << charCount << endl;
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
  
  if(connect(remoteSocket,(struct sockaddr *) &sa, sizeof(struct sockaddr_in)) < 0) {
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

  // Set up poll
  struct pollfd pfd[1];
  int timeout;
  int n, ready;
  pfd[0].fd = socket;
  pfd[0].events = POLLIN;
  n = 1;
  timeout = 10000; //timeout after 10 seconds

  // Read request from socket
  if((bytesRead = readData(socket, reqInBuf, MAXBUFFERSIZE)) < 0) {
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
  if(writeData(remoteSocket, reqOutBuf, reqLen) < 0) {
    close(remoteSocket);
    close(socket);
    exit(1);
  }

  // Read response
  if((bytesRead = readData(remoteSocket, respInBuf, MTU)) < 0) {
    close(remoteSocket);
    close(socket);
    exit(1);
  }
  cerr << respInBuf;

  // Send response to client
  if(writeData(socket, respInBuf, bytesRead) < 0) {
    close(remoteSocket);
    close(socket);
    exit(1);
  }
  
  // Handle Persistent connection
  // Wait for new request
  while((ready = poll(pfd, 1, timeout)) > 0) {
    if(ready == -1) {
      perror("poll");
    }
    else if(ready == 0) // Timed out
      break;
    if(pfd[0].revents & POLLHUP) { // Client hung up
      close(socket);
      break;
    }
    if((bytesRead = readData(socket, reqInBuf, MAXBUFFERSIZE)) < 0) {
      close(socket);
      exit(1);
    }
    if(bytesRead == 0)
       break;
    // Read http request
    request.ParseRequest(reqInBuf, bytesRead);
    size_t reqLen = request.GetTotalLength();
    reqOutBuf = new char[reqLen];
    request.FormatRequest(reqOutBuf);
    cerr << reqOutBuf;

    // Forward http request to remote server
    if(writeData(remoteSocket, reqOutBuf, reqLen) < 0) {
      close(remoteSocket);
      close(socket);
      exit(1);
    }
    
    // Read response
    if((bytesRead = readData(remoteSocket, respInBuf, MTU)) < 0) {
      close(remoteSocket);
      close(socket);
      exit(1);
    }
    cerr << "bytesRead: " << bytesRead << endl;
    cerr << respInBuf;

    // Send response to client
    if(writeData(socket, respInBuf, bytesRead) < 0) {
      close(remoteSocket);
      close(socket);
      exit(1);
    }
  }
  cerr << "OUT" << endl;
  close(socket);
  close(remoteSocket);
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
