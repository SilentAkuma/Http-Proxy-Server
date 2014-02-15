/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

using namespace std;

#define MAXHOSTNAME 256
#define PORTNUMBER 15577

/* SOCKET FUNCTIONS */

// Set up socket and listen for connections
int establishConnection (unsigned short portNumber)
{
  // command line parsing
  char hostName[MAXHOSTNAME+1];
  int mySocket;
  struct sockaddr_in sa;
  struct hostent *hp;
  
  cerr << portNumber;
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
  cerr << "socket established " << mySocket;
  return(mySocket);
}

//accept connections
int getConnection(int socket) 
{
  int connectionSocket;
  socklen_t addrLen;
  struct sockaddr_in sa;

  addrLen = sizeof(sa);
  cerr << "socket attempting to accept";
  cerr << socket;
  if((connectionSocket = accept(socket, (struct sockaddr*) &sa, &addrLen)) < 0)
    return(-1);
  cerr << "socket connected";
  return(connectionSocket);
}

//read data from connected socket
int readData(int mySocket, char *buf, int numChars)
{
  int charCount = 0;
  int charsRead = 0;
  
  while(charCount < numChars) {
    if((charsRead = read(mySocket, buf, numChars-charCount)) > 0) {
      charCount += charsRead;
      buf += charsRead;
    }
    else if(charsRead < 0)
      return -1;
  }
  cerr << "socket read";
  return charCount;
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
    cerr << "poop";
    if((connectionSocket = getConnection(ourSocket)) < 0){
      if(errno == EINTR)
        continue;
      perror("accept");
      exit(1);
    }
    cerr << "rawr";
    switch(fork()){
    case -1:
      perror("fork");
      close(ourSocket);
      close(connectionSocket);
      exit(1);
      break;
    case 0:
      close(ourSocket);
      exit(0);
    default:
      close(connectionSocket);
      continue;
    }
  }
}
