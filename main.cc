#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <poll.h>
#include <iostream>
#include <algorithm>

#define POLL_TIMEOUT 500
#define MAX_MSG_SIZE 16

int acceptor = -1;
int connection = -1;
bool connected = false;
char buf[MAX_MSG_SIZE + 1];

void initializeAcceptorSocket();
void acceptConnection();
void readMessage();

int main()
{
  std::cout << "Setting up accepting socket..." << std::endl;
  initializeAcceptorSocket();
  while(1)
  {
    if (connected)
    {
      readMessage();
    }
    else
    {
      acceptConnection();
    }
  }
  return 0;
}

void initializeAcceptorSocket()
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;
  hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
  const char* hostname = "localhost";
  const char* portname = "30222";

  struct addrinfo* resolved;
  int err = getaddrinfo(hostname, portname, &hints, &resolved);
  if (err!=0)
  {
    std::cerr << "Could not resolve address (Error " << err << ")" << std::endl;
    exit(1);
  }

  acceptor = socket(resolved->ai_family, resolved->ai_socktype, resolved->ai_protocol);
  if (acceptor == -1)
  {
    std::cerr << strerror(errno) << std::endl;
    exit(1);
  }

  int reuseaddr=1;
  if (setsockopt(acceptor, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr))== -1)
  {
    std::cerr << strerror(errno) << std::endl;
    exit(1);
  }

  long save_fd;
  save_fd = fcntl(acceptor, F_GETFL);
  save_fd |= O_NONBLOCK;
  if (fcntl(acceptor, F_SETFL, save_fd) == -1)
  {
    std::cerr << "Failed to set accepting socket to nonblocking (Error " << errno << ")" << std::endl;
    exit(1);
  }

  if (bind(acceptor, resolved->ai_addr, resolved->ai_addrlen) == -1)
  {
    std::cerr << strerror(errno) << std::endl;
    exit(1);
  }

  freeaddrinfo(resolved);

  if (listen(acceptor, SOMAXCONN)) {
    std::cerr << "Failed to listen for connections (Error " << errno << ")" << std::endl;
    exit(1);
  }
}

void acceptConnection()
{
  struct pollfd pollDescription;
  pollDescription.fd = acceptor;
  pollDescription.events = POLLIN;
  int pollstatus = poll(&pollDescription, 1, POLL_TIMEOUT);
  if (pollstatus == -1)
  {
    std::cerr << "Accept poll failed (Error " << errno << ")" << std::endl;
  }
  else if (pollstatus == 0)
  {
    std::cout << "Accept poll timed out" << std::endl;
  }
  else
  {
    std::cout << "Accept poll success" << std::endl;
    connection = accept(acceptor, 0, 0);
    if (connection == -1)
    {
      if (errno == EWOULDBLOCK || errno == EINTR)
      {
        return;
      }
      else
      {
        std::cerr << "Accept failed (Error " << errno << ")" << std::endl;
      }
    }
    else
    {
      std::cout << "Connected" << std::endl;
      connected = true;

      long save_fd;
      save_fd = fcntl(connection, F_GETFL);
      save_fd |= O_NONBLOCK;
      if (fcntl(connection, F_SETFL, save_fd) == -1)
      {
        std::cerr << "Failed to set connection socket to nonblocking (Error " << errno << ")" << std::endl;
        exit(1);
      }
    }
  }
}

void readMessage()
{
  struct pollfd pollDescription;
  pollDescription.fd = connection;
  pollDescription.events = POLLIN;
  int pollStatus = poll(&pollDescription, 1, POLL_TIMEOUT);
  if (pollStatus == -1)
  {
    std::cerr << "Read poll failed (Error " << errno << ")" << std::endl;
    close(connection);
    connected = false;
  }
  else if (pollStatus == 0)
  {
    std::cout << "Read poll timed out" << std::endl;
  }
  else
  {
    std::cout << "Read poll success" << std::endl;
    int readstatus = read(connection, (void*)buf, MAX_MSG_SIZE);
    if (readstatus == 0)  // EOF
    {
      std::cout << "Read EOF" << std::endl;
      connected = false;
      close(connection);
      std::cout << "Closed connection" << std::endl;
    }
    else if (readstatus == -1)  // ERROR
    {
      std::cerr << "Read failed (Error " << errno << ")" << std::endl;
      if (errno == EWOULDBLOCK || errno == EINTR)
      {
        return;
      }
      connected = false;
      close(connection);
      std::cout << "Closed connection" << std::endl;
    }
    else
    {
      buf[readstatus] = '\0';
      std::cout << "Successully read " << readstatus << " bytes: \"" << buf << "\"" << std::endl;
      if (buf[0] == 'q' && readstatus == 1)
      {
        std::cout << "Quit" << std::endl;
        shutdown(connection, SHUT_RDWR);
      }
    }
  }
}