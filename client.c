#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>

extern int errno;

int quiz_count;
int port;

int main(int argc, char *argv[])
{
  int fdclient; // descriptorul de socket
  struct sockaddr_in server;

  if (argc != 3)
  {
    printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
    return -1;
  }

  /* stabilim portul */
  port = atoi(argv[2]);

  // creez socket
  fdclient = socket(AF_INET, SOCK_STREAM, 0);
  if (fdclient == -1)
  {
    perror("[server]Eroare la socket().\n");
    return errno;
  }

  /* umplem structura folosita pentru realizarea conexiunii cu serverul */
  /* familia socket-ului */
  server.sin_family = AF_INET;
  /* adresa IP a serverului */
  server.sin_addr.s_addr = inet_addr(argv[1]);
  /* portul de conectare */
  server.sin_port = htons(port);

  // ma conectez la server
  int myconnect = connect(fdclient, (struct sockaddr *)&server, sizeof(struct sockaddr));
  if (myconnect == -1)
  {
    perror("[client]Eroare la connect().\n");
    return errno;
  }

  int questionNumber = 0;

  char msg[100];
  bzero(msg, 100);
  printf("[client]Introduceti un nume: ");
  fflush(stdout);
  read(0, msg, 100);

  /* trimiterea mesajului la server */
  if (write(fdclient, msg, 100) <= 0)
  {
    perror("[client]Eroare la write() spre server.\n");
    return errno;
  }

  while (1)
  {
    //while cate inrebari am atatea read() si write() am




    // char msg[100];
    // bzero(msg, 100);
    // printf("[client] Scrieti ceva: ");
    // fflush(stdout);
    // read(0, msg2, 100);

    // if (write(fdclient, msg, 100) <= 0)
    // {
    //   perror("[client]Eroare la write() spre server.\n");
    //   return errno;
    // }
  }
  close(fdclient);
}