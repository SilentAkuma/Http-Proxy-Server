/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>

using namespace std;

//#define MAXHOSTNAME = 256

int main (int argc, char *argv[])
{
  // command line parsing
  char hostName[256+1];
  int mySocket;
  struct sockaddr_in sa;
  struct hostent *hp;
  
  memset(&sa, 0, sizeof(struct sockaddr_in));
  gethostname(hostName, 256);
  hp = gethostbyname(hostName);
  if(hp == NULL)
    return(-1);  //no address
  sa.sin_family = hp->h_addrtype;
  sa.sin_port = htons(14886);
  if((mySocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return(-1);
  if(bind(mySocket,(struct sockaddr *) &sa, sizeof(struct sockaddr_in)) < 0) {
    close(mySocket);
    return(-1);
  }
  listen(mySocket, 20);
  
  //return(mySocket);
  
  //return 0;
}

