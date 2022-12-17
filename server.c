// TODO : de adaugat intrebari
// TODO : MAI MULTE JOCURI IN PARALEL  :(
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

// #pragma SQLITE_TEMP_STORE=0;


pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t wlock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t addlock = PTHREAD_MUTEX_INITIALIZER;

#define PORT 2604
#define MAX_PLAYERS 2
extern int errno;

bool closed = 0;

sqlite3 *db; // pointer pentru baza de date

int id = 0;

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

char* strnstr(const char *s, const char *find, size_t slen)
{
    char c, sc;
    size_t len;

    if ((c = *find++) != '\0')
    {
        len = strlen(find);
        do
        {
            do
            {
                if (slen-- < 1 || (sc = *s++) == '\0')
                    return (NULL);
            } while (sc != c);
            if (len > slen)
                return (NULL);
        } while (strncmp(s, find, len) != 0);
        s--;
    }
    return ((char *)s);
}
void selectAnswer(int, char *, char *);
void sendQuestion(int, char *, void *);
void deleteDataBase(void *);
void addRowRanking(int, char *, int);
void winner(void *, char *, char *);
void deleteRanking(void *);
void deletePlayers(void *);
void createRanking();
void createDataBase();

void play(void *thr)
{
    clientThread *info = (clientThread *)thr;
    int socket = info->fdClient;

    fd_set readfds;
    fd_set actfds;
    struct timeval tv;
    int fd;
    int nfds = info->fdClient;

    for (int question = 1; question <= 8; question++)
    {

        time_t start, end;

        // primesc intrebarea de la db si o trimite clientului
        char intrebare[100];
        bzero(intrebare, 100);
        sendQuestion(question, intrebare, thr);

        // primesc raspunsul de la db
        char rasp[100];
        char pct[100];
        bzero(rasp, 100);
        bzero(pct, 100);
        selectAnswer(question, rasp, pct);
        // convertesc din char * in int pentru a putea modifica punctajele
        int puncte;
        sscanf(pct, "%d", &puncte);

        char clientCommand[100], clientAnswer[100];

        // bzero(clientAnswer, 100);
        bzero(clientCommand, 100);

        FD_ZERO(&actfds);
        FD_SET(info->fdClient, &readfds);

        tv.tv_sec = 30; // clientul are la dispozitie 30s sa raspunda la intrebare
        tv.tv_usec = 0;

        time(&start);
        int myselect = select(nfds + 1, &readfds, NULL, NULL, &tv);
        time(&end);
        double secPassed = (double)(end - start);

        if (myselect == -1)
        {
            printf("[thread FOR - %d] eroare.\n", info->idThread);
            fflush(stdout);
        }
        else
        {
            // verificam daca clientul a raspuns
            if (FD_ISSET(info->fdClient, &readfds))
            {
                int bytes;
                bzero(clientAnswer, 100);
                bytes = read(info->fdClient, clientAnswer, 100);

                if (bytes == -1)
                {
                    printf("[thread -%d] eroare la read().\n", info->idThread);
                    fflush(stdout);
                }
                clientAnswer[bytes] = NULL;

                if (strstr(clientAnswer, "a : ") != NULL)
                {
                    char answer[50];
                    bzero(answer, 50);
                    strcpy(answer, clientAnswer + 4);

                    printf("[server] clientul %d a raspuns: %s si raspunsul bun era: %s \n", info->idThread, answer, rasp);
                    fflush(stdout);
                    // trebuie sa verific daca clientul imi da raspunsul bun

                    if (strnstr(answer, rasp, 1) != NULL)
                    {
                        clients[info->idThread].points = clients[info->idThread].points + puncte - (secPassed / 10);
                        printf("[server] clientul %d a raspuns corect si are %d puncte\n", info->idThread, clients[info->idThread].points);
                        fflush(stdout);
                    }
                }
                else
                {
                    if (strstr(clientAnswer, "exit") != NULL)
                    {
                        printf("[thread -%d] a iesit .\n", info->idThread);
                        fflush(stdout);

                        clients[info->idThread].points = -1;

                        pthread_detach(pthread_self());

                        close(info->fdClient);
                        pthread_exit(NULL);
                    }
                }
            }
            else
            {
                printf("[thread FOR - %d] timpul a expirat.\n", info->idThread);
                fflush(stdout);

                char buffer[100];
                bzero(buffer, 100);
                strcpy(buffer, "time expired, you are disqualified");
                buffer[100] = NULL;

                if (write(socket, buffer, 100) <= -1)
                {
                    perror("eroare la write() cand timpul a expirat");
                }

                clients[info->idThread].points = -1;

                pthread_detach(pthread_self());

                close(info->fdClient);
                pthread_exit(NULL);
            }
        }
    }

    // tv = (struct timeval){ 30 }; //trying to reset the time for another game

    addRowRanking(info->idThread, clients[info->idThread].username, clients[info->idThread].points);

    sleep(5);

    char username[100];
    char puncte[100];
    winner(info, username, puncte);

    deleteDataBase(info);

    deletePlayers(info);
}

void *threadFunction(void *thr)
{
    // struct clientThread *info;
    // info = ((struct clientThread*)thr);

    clientThread *info = (clientThread *)thr;
    int socket = info->fdClient;

    if (game.numberPlayers > MAX_PLAYERS)
    {
        printf("[DEBUG] there are enaugh clients");
        fflush(stdout);
        char cerere[100];
        bzero(cerere, 100);
        strcat(cerere, "There are enaugh clients, please wait for another game to start.");
        int mywrite = write(socket, cerere, 100);
        return 0;
        pthread_detach(pthread_self());
    }

    char cerere[100];
    bzero(cerere, 100);
    strcat(cerere, "For game to start write 'start : username', for answering 'a : answer', for exiting 'exit'");

    int mywrite = write(socket, cerere, 100);
    if (mywrite <= 0)
    {
        printf("[thread -%d] - eroare la write().", info->idThread);
        fflush(stdout);
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
        fflush(stdout);
    }
    username[myread] = NULL;

    if (strstr(username, "exit") != NULL)
    {
        printf("[thread -%d] a iesit .\n", info->idThread);
        fflush(stdout);
        pthread_detach(pthread_self());

        close(info->fdClient);
        pthread_exit(NULL);
    }

    if (strstr(username, "start : ") != NULL)
    {
        game.numberPlayers++;

        char name[100];
        bzero(name, 100);

        strcpy(name, username + 8);

        strcpy(info->username, name);
        strcpy(clients[info->idThread].username, name);
        clients[info->idThread].points = 0; // cate pct are
        clients[info->idThread].exited = 0; // daca a iesit din joc

        printf("Client %d with username: %s and thread id: %ld is connected\n", info->idThread, name, pthread_self());
        fflush(stdout);

        fd_set readfds;
        struct timeval tv;
        // astept sa fie destui clienti
        while (1)
        {
            printf("[DEBUG] asteptam clientii, numar clienti : %d\n", game.numberPlayers);
            fflush(stdout);
            tv.tv_sec = 2;
            tv.tv_usec = 500000;
            FD_ZERO(&readfds);
            FD_SET(info->fdClient, &readfds);

            int myselect = select(info->fdClient + 1, &readfds, NULL, NULL, &tv);
            if (myselect == 0)
            {
                printf("[ THRD_ROUTINE thread - %d] Didn't answer.\n", info->idThread);
                fflush(stdout);
            }

            if (FD_ISSET(info->fdClient, &readfds))
            {

                char buffer[100];
                int bytes;
                char command[100];

                bytes = read(info->fdClient, command, sizeof(buffer));
                if (bytes == 0)
                {
                    printf("[thread -%d] eroare la read() cand asteptam sa intre clientii.\n", info->idThread);
                    fflush(stdout);
                }
            }

            if (game.numberPlayers == MAX_PLAYERS)
            {
                // createDataBase();
                // createRanking();
                play(thr);
                break;
            }
        }
    }
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

void selectAnswer(int i, char *raspuns, char *puncte)
{
    // iau din baza de date si raspunsul corect si punctajul
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

void sendQuestion(int i, char *intrebare, void *client)
{
    clientThread *info = (clientThread *)client;
    int socket = info->fdClient;

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

    strcpy(intrebare, sqlite3_column_text(stmt, 0));

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    int mywrite = write(socket, intrebare, 100);
    if (mywrite < 0)
    {
        printf("[thread - %d] eroare la write().\n", info->idThread);
    }
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
           "VALUES (1, 'How many lives is a cat said to have?  a.9 b.10 c.7', 'a', 10); "
           "INSERT INTO QUIZ(ID,QUESTION,ANSWER,POINTS) "
           "VALUES (2, 'What is the currency of Italy?  a.euro b.lira c.ron', 'a', 10); "
           "INSERT INTO QUIZ (ID,QUESTION,ANSWER,POINTS) "
           "VALUES (3, 'Which element is said to keep bones strong? a.food b.calcium c.vitamins', 'b', 10);"
           "INSERT INTO QUIZ (ID,QUESTION,ANSWER,POINTS) "
           "VALUES (4, 'How many Harry Potter books are there? a.6 b.5 c.7', 'c', 15);"
           "INSERT INTO QUIZ (ID,QUESTION,ANSWER,POINTS) "
           "VALUES (5, 'In which city can you find the Colosseum? a.Milan b.Rome c.Torino', 'b', 15);"
           "INSERT INTO QUIZ (ID,QUESTION,ANSWER,POINTS) "
           "VALUES (6, 'Which fruit is associated with Isaac Newton? a.apple b.pear c.banana', 'a', 15);"
           "INSERT INTO QUIZ (ID,QUESTION,ANSWER,POINTS) "
           "VALUES (7, '“Smells Like Teen Spirit” is a famous song from which band? a.ACDC b.Nirvana c.NBHD', 'b', 10);"
           "INSERT INTO QUIZ (ID,QUESTION,ANSWER,POINTS) "
           "VALUES (8, 'What was the name of Jennifer Aniston s character in Friends? a.Phoebe b.Monica c.Rachel', 'c', 10);";

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
          "POINTS     INT             NOT NULL); ";

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

void deleteDataBase(void *client)
{
    clientThread *info = (clientThread *)client;
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
    sprintf(inregistrare, "DROP TABLE IF EXISTS QUIZ");

    sqlite3_stmt *stmt;

    rc = sqlite3_prepare_v2(db, inregistrare, -1, &stmt, NULL);

    if (rc < 0)
    {
        printf("Error executing sql statement\n");
        fflush(stdout);
    }
    else
    {
        printf("TABLE QUIZ DELETED");
        fflush(stdout);
    }

    rc = sqlite3_step(stmt);

    sqlite3_finalize(stmt);

    deleteRanking(info);

    sqlite3_close(db);
}

void deleteRanking(void *client)
{
    sleep(5);

    pthread_mutex_lock(&lock);
    clientThread *info = (clientThread *)client;

    int r = sqlite3_open("ranking.db", &db);

    if (r != SQLITE_OK)
    {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    }
    else
    {
        fprintf(stderr, "Opened database successfully\n");
    }

    // casting int la char pierde din informatii si nu pot folosit (char)
    // folosesc sprintf
    char query[1000];
    sprintf(query, "DELETE FROM RANK WHERE THR=%d", info->idThread);

    sqlite3_stmt *st;

    r = sqlite3_prepare_v2(db, query, -1, &st, NULL);

    if (r < 0)
    {
        printf("Error executing sql statement\n");
        fflush(stdout);
    }
    else
    {
        printf("ROW RANK DELETED");
        fflush(stdout);
    }

    r = sqlite3_step(st);

    sqlite3_finalize(st);
    sqlite3_close(db);
    pthread_mutex_unlock(&lock);
}

void createRanking()
{
    char *zErrMsg = 0;
    int rc = sqlite3_open("ranking.db", &db);

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
    sql = "CREATE TABLE RANK("
          "THR        INT              NOT NULL, "
          "USERNAME   VARCHAR(1000)    NOT NULL, "
          "POINTS     INT              NOT NULL); ";

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

    sqlite3_close(db);
}

void addRowRanking(int id_thr, char *nume, int puncte)
{
    // sqlite3 *db;
    pthread_mutex_lock(&addlock);
    char *error_message = 0;
    int rc;

    rc = sqlite3_open("ranking.db", &db);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    }
    else
    {
        fprintf(stderr, "Opened database successfully\n");
    }

    char sql[1024]; /* make room to hold sql string */

    sprintf(sql, "INSERT INTO RANK(THR,USERNAME,POINTS) VALUES (%d,'%s', %d);", id_thr, nume, puncte);

    rc = sqlite3_exec(db, sql, callback, 0, &error_message);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error at addRowRanking(): %s\n", error_message);
        fflush(stdout);
        sqlite3_free(error_message);
    }
    else
    {
        fprintf(stdout, "Records created successfully\n");
        fflush(stdout);
    }

    sqlite3_close(db);
    pthread_mutex_unlock(&addlock);
}

void winner(void *client, char *username, char *puncte)
{
    pthread_mutex_lock(&wlock);

    clientThread *info = (clientThread *)client;
    int socket = info->fdClient;
    int i = info->idThread;
    int rc = sqlite3_open("ranking.db", &db);

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
    char *inregistrare3;
    char *inregistrare4;

    sprintf(inregistrare1, "SELECT USERNAME FROM RANK WHERE THR=%d", i);
    sprintf(inregistrare2, "SELECT POINTS FROM RANK WHERE THR=%d", i);
    inregistrare3 = "SELECT MAX(POINTS) FROM RANK";
    inregistrare4 = "SELECT USERNAME FROM RANK WHERE POINTS = (SELECT MAX(POINTS) FROM RANK)";

    sqlite3_stmt *stmt1;
    sqlite3_stmt *stmt2;
    sqlite3_stmt *stmt3;
    sqlite3_stmt *stmt4;

    rc = sqlite3_prepare_v2(db, inregistrare1, -1, &stmt1, NULL);

    if (rc < 0)
    {
        printf("Error executing sql statement\n");
        fflush(stdout);
    }

    rc = sqlite3_step(stmt1);
    // strcpy(username, sqlite3_column_text(stmt1, 0));

    rc = sqlite3_prepare_v2(db, inregistrare2, -1, &stmt2, NULL);

    if (rc < 0)
    {
        printf("Error executing sql statement\n");
        fflush(stdout);
    }

    rc = sqlite3_step(stmt2);
    // strcpy(puncte, sqlite3_column_text(stmt1, 0));

    rc = sqlite3_prepare_v2(db, inregistrare3, -1, &stmt3, NULL);

    if (rc < 0)
    {
        printf("Error executing sql statement\n");
        fflush(stdout);
    }

    rc = sqlite3_step(stmt3);
    // strcpy(username, sqlite3_column_text(stmt3, 0));

    rc = sqlite3_prepare_v2(db, inregistrare4, -1, &stmt4, NULL);

    if (rc < 0)
    {
        printf("Error executing sql statement\n");
        fflush(stdout);
    }

    rc = sqlite3_step(stmt4);
    // strcpy(puncte, sqlite3_column_text(stmt4, 0));

    char toSend[100];
    sprintf(toSend, "CONGRATS! You got %s points \n The WINNER is : %s with %s points\n", sqlite3_column_text(stmt2, 0), sqlite3_column_text(stmt4, 0), sqlite3_column_text(stmt3, 0));

    int mywrite = write(socket, toSend, 100);
    if (mywrite < 0)
    {
        printf("[WINNER thread - %d] eroare la write().\n", info->idThread);
    }

    sqlite3_finalize(stmt1);
    sqlite3_finalize(stmt2);
    sqlite3_finalize(stmt3);
    sqlite3_finalize(stmt4);
    sqlite3_close(db);

    pthread_mutex_unlock(&wlock);
}

void deletePlayers(void *client)
{

    // inchid conexiunea cu clientii
    clientThread *info = (clientThread *)client;
    int socket = info->fdClient;
    // To end connection,press any key..
    char buffer[100];
    bzero(buffer, 100);
    strcpy(buffer, "To end connection, press enter key...");
    buffer[100] = NULL;

    if (write(socket, buffer, 100) <= -1)
    {
        perror("eroare la write() cand timpul a expirat");
    }

    id = 0;
    game.numberPlayers = 0;

    pthread_detach(pthread_self());
    close(info->fdClient);
    pthread_exit(NULL);
    // exit(0);
}

int main()
{

    createDataBase();
    createRanking();
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

        game.noQuestion = 8;

        tid = pthread_create(&tid, NULL, &threadFunction, thread_arg);
        if (tid < 0)
        {
            perror("[server] eroare la pthread_create()");
            // free(thread_arg);
            return errno;
        }
    }

    close(fdserver);
    return 0;
}