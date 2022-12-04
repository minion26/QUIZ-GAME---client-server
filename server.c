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
#include <sqlite3.h>

#define PORT 2604
extern int errno;

sqlite3 *db; // pointer pentru baza de date

typedef struct thread
{
    int fdClient; // socketul pt conectarea cu clientul
    int idThread; // al catelea thread este
    bool exited;
    struct sockaddr address;
    struct sockaddr_in client; // adresa clientului conectat
    char username[256];
    int addr_len; // lungimea lui adress

} clientThread;

struct client
{
    char username[256];
    int points;
    int socketDescriptor;
    bool exited;

} clients[6];

struct game
{
    int numberPlayers;
    int noQuestion;
} game;

void selectAnswer(int , char* , char* );
void sendQuestion( int, char*);

void play(void *thr)
{
    clientThread *info = (clientThread *)thr;
    int socket = info->fdClient;

    // folosesc select
    fd_set readfds;
    fd_set actfds;
    struct timeval tv;
    int fd;
    int nfds = info->fdClient;

    char correctAnswer;
    char clientCommand[100], clientAnswer[100];

    for (int question = 1; question <= game.noQuestion; question++)
    {
        // primesc intrebarea de la db
        char* intrebare[100];
        bzero(intrebare, 100);
        sendQuestion(question, intrebare);
        printf("[server] %s\n", intrebare);
        
        // primesc raspunsul de la db
        char *rasp[100];
        char *pct[100];
        bzero(rasp, 100);
        bzero(pct, 100);
        selectAnswer(question, rasp, pct);
        printf("[server] raspuns: %s, puncte:%s\n", rasp, pct);
        

        // bzero(clientCommand, 100);
        // FD_ZERO(&actfds);
        // FD_SET(info->fdClient, &readfds);

        // tv.tv_sec = 30; // clientul are la dispozitie 30s sa raspunda la intrebare
        // tv.tv_usec = 0;

        // int myselect = select(nfds + 1, &readfds, NULL, NULL, &tv);
        // if (myselect < 0)
        // {
        //     printf("[thread - %d] eroare la select().\n", info->idThread);
        // }

        // // verificam daca clientul a raspuns
        // if (FD_ISSET(info->fdClient, &readfds))
        // {
        //     char buffer[100];
        //     int bytes;

        //     bytes = read(info->fdClient, clientAnswer, sizeof(buffer));
        //     if (bytes < 0)
        //     {
        //         printf("[thread -%d] Eroare la read() de la client.\n", info->idThread);
        //     }

        //     // trebuie sa verific daca clientul imi da raspunsul bun
        // }
    }
}

void *threadFunction(void *thr)
{
    // struct clientThread *info;
    // info = ((struct clientThread*)thr);
    clientThread *info = (clientThread *)thr;
    int socket = info->fdClient;

    if (info->idThread >= 2)
    {
        printf("[server] there are enaugh clients");
        fflush(stdout);
        char cerere[100];
        bzero(cerere, 100);
        strcat(cerere, "There are enaugh clients, please wait for another game to start.");
        int mywrite = write(socket, cerere, 100);
        return 0;
        pthread_detach(pthread_self());
    }

    // printf("[thread-%d] - Introduceti un username:...\n", info->idThread);
    // fflush(stdout);

    char cerere[100];
    bzero(cerere, 100);
    strcat(cerere, "Introduceti un username:");

    int mywrite = write(socket, cerere, 100);
    if (mywrite <= 0)
    {
        printf("[thread -%d] - eroare la write().", info->idThread);
        perror("[server] eroare la write() de la client");
    }

    struct sockaddr_in client = info->client;
    char username[100];
    bzero(username, 100);

    int myread = read(socket, username, 100);
    if (myread <= 0)
    {
        printf("[thread -%d] - eroare la read().", info->idThread);
        perror("[server] eroare la read() de la client");
    }

    strcpy(info->username, username);
    strcpy(clients[info->idThread].username, username);
    clients[info->idThread].points = 0; // cate pct are
    clients[info->idThread].exited = 0; // daca a iesit din joc

    printf("Client %d with username: %s and thread id: %ld is connected\n", info->idThread, username, pthread_self());
    fflush(stdout);

    play(thr);

    // pthread_detach(pthread_self());
}

static int callback(void *NotUsed, int argc, char **argv, char **azColName)
{
    int i;
    for (i = 0; i < argc; i++)
    {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
        fflush(stdout);
    }
    return 0;
}

void selectAnswer(int i, char* raspuns, char* puncte)
{
    // iau din baza de date si raspunsul corect, si punctajul
    int rc = sqlite3_open("test.db", &db);

    if (rc)
    {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    }
    else
    {
        fprintf(stderr, "Opened database successfully\n");
    }

    char inregistrare1[100];
    char inregistrare2[100];
    sprintf(inregistrare1, "SELECT ANSWER FROM QUIZ WHERE ID=%d", i);
    sprintf(inregistrare2, "SELECT POINTS FROM QUIZ WHERE ID=%d", i);

    sqlite3_stmt *stmt1;
    sqlite3_stmt *stmt2;

    rc = sqlite3_prepare_v2(db, inregistrare1, -1, &stmt1, NULL);

    if (rc < 0)
    {
        printf("Error executing sql statement\n");
        fflush(stdout);
    }

    rc = sqlite3_step(stmt1);
    strcpy(raspuns, sqlite3_column_text(stmt1, 0));

    rc = sqlite3_prepare_v2(db, inregistrare2, -1, &stmt2, NULL);

    if (rc < 0)
    {
        printf("Error executing sql statement\n");
        fflush(stdout);
    }

    rc = sqlite3_step(stmt2);
    strcpy(puncte, sqlite3_column_text(stmt2, 0));

    sqlite3_finalize(stmt1);
    sqlite3_finalize(stmt2);
    sqlite3_close(db);

}

void sendQuestion( int i, char* intrebare)
{
    int rc = sqlite3_open("test.db", &db);

    if (rc)
    {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    }
    else
    {
        fprintf(stderr, "Opened database successfully\n");
    }

    // casting int la char pierde din informatii si nu pot folosit (char)
    // folosesc sprintf
    char inregistrare[100];
    sprintf(inregistrare, "SELECT QUESTION FROM QUIZ WHERE ID=%d", i);

    sqlite3_stmt *stmt;

    rc = sqlite3_prepare_v2(db, inregistrare, -1, &stmt, NULL);

    if (rc < 0)
    {
        printf("Error executing sql statement\n");
        fflush(stdout);
    }

    rc = sqlite3_step(stmt);
    // char* q;
    // q = (char*)sqlite3_column_text(stmt, 0);
    // char* backup = q;
    // printf("[server] question is: %s", backup);
    

    strcpy(intrebare, sqlite3_column_text(stmt, 0));
    //strcpy(backup, q);
    //printf("[db] %s", q);
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    //return backup;

    
}

void populateTable()
{
    char *zErrMsg = 0;
    char *sql2;
    int rc = sqlite3_open("test.db", &db);

    if (rc)
    {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    }
    else
    {
        fprintf(stderr, "Opened database successfully\n");
    }

    sql2 = "INSERT INTO QUIZ (ID,QUESTION,ANSWER,POINTS) "
           "VALUES (1, 'How many lives is a cat said to have?', '9', 10); "
           "INSERT INTO QUIZ(ID,QUESTION,ANSWER,POINTS) "
           "VALUES (2, 'What is the currency of Italy?', 'Euro', 10); "
           "INSERT INTO QUIZ (ID,QUESTION,ANSWER,POINTS) "
           "VALUES (3, 'Which element is said to keep bones strong?', 'Calcium', 10);";

    rc = sqlite3_exec(db, sql2, callback, 0, &zErrMsg);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error at populateTable(): %s\n", zErrMsg);
        fflush(stdout);
        sqlite3_free(zErrMsg);
    }
    else
    {
        fprintf(stdout, "Records created successfully\n");
        fflush(stdout);
    }

    sqlite3_close(db);
}

void createDataBase()
{
    // sqlite3 *db;
    char *zErrMsg = 0;
    int rc = sqlite3_open("test.db", &db);

    if (rc)
    {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        fflush(stdout);
    }
    else
    {
        fprintf(stderr, "Opened database successfully\n");
        fflush(stdout);
    }

    char *sql;
    sql = "CREATE TABLE QUIZ("
          "ID         INT              NOT NULL, "
          "QUESTION   VARCHAR(1000)    NOT NULL, "
          "ANSWER     VARCHAR(1000)    NOT NULL, "
          "POINTS     INT              NOT NULL);";

    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error at createDataBase(): %s\n", zErrMsg);
        fflush(stdout);
        sqlite3_free(zErrMsg);
    }
    else
    {
        fprintf(stdout, "Table created successfully\n");
        fflush(stdout);
    }

    populateTable();
    sqlite3_close(db);
}

int main()
{

    createDataBase();
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

        clients[thread_arg->idThread].socketDescriptor = thread_arg->fdClient;
        game.numberPlayers = 0;
        game.numberPlayers++;
        game.noQuestion = 3;

        tid = pthread_create(&tid, NULL, &threadFunction, thread_arg);
        if (tid < 0)
        {
            perror("[server] eroare la pthread_create()");
            // free(thread_arg);
            return errno;
        }
    }
}