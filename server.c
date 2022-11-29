// creez socketul
// fac threaduri pt clienti
// fiecare client va fi o structura care va contine: username, points
// idThread,
// in thread:
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
#include <stdbool.h>
#include <pthread.h>

#define PORT 2604
extern int errno;

typedef struct
{
    int fdClient; // socketul pt conectarea cu clientul
    int idThread;
    bool exit;
    struct sockaddr address;
    struct sockaddr_in client; // adresa clientului conectat
    char username[100];
    int addr_len; // lungimea lui adress

} clientThread;

struct
{
    int numberPlayers;
} game;

void *threadFunction(void *thr)
{
    // struct clientThread *info;
    // info = ((struct clientThread*)thr);
    clientThread *info = (clientThread *)thr;
    printf("[thread-%d] - Asteptam mesajul...\n", info->idThread);
    fflush(stdout);
    //pthread_detach(pthread_self());	

}
int main()
{
    struct sockaddr_in server;

    int fdserver;

    // creez socket
    fdserver = socket(AF_INET, SOCK_STREAM, 0);
    if (fdserver == -1)
    {
        perror("[server]Eroare la socket().\n");
        return errno;
    }

    int on = 1; // utilizarea optiunii SO_REUSEADDR
    setsockopt(fdserver, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    bzero(&server, sizeof(server));

    // umplem structura folosita de server
    // stabilirea familiei de socket-uri
    server.sin_family = AF_INET;
    /* acceptam orice adresa */
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    /* utilizam un port utilizator */
    server.sin_port = htons(PORT);

    // atasam socketul
    int mybind = bind(fdserver, (struct sockaddr *)&server, sizeof(struct sockaddr));
    if (mybind == -1)
    {
        perror("[server]Eroare la bind().\n");
        return errno;
    }

    // punem serverul sa asculte
    int mylisten = listen(fdserver, 2);
    if (mylisten == -1)
    {
        perror("[server]Eroare la listen().\n");
        return errno;
    }

    int id = 0;
    // pentru fiecare client fac thread
    while (1)
    {

        pthread_t tid; // id thread
        clientThread *thread_arg;

        printf("[server]Asteptam la portul %d...\n", PORT);
        fflush(stdout);


        thread_arg = (clientThread *)malloc(sizeof(clientThread));
        thread_arg->idThread = id;
        id++;

        thread_arg->fdClient = accept(fdserver, &thread_arg->address, &thread_arg->addr_len);
        if (thread_arg->fdClient == -1)
        {
            perror("[server]Eroare la accept().\n");
            continue;
        }

        tid = pthread_create(&tid, NULL, &threadFunction, thread_arg);
        if (tid < 0)
        {
            perror("[server] eroare la pthread_create()");
            //free(thread_arg);
            return errno;
        }

       
    }
}