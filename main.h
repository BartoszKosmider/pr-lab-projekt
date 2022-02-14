#ifndef GLOBALH
#define GLOBALH

#define _GNU_SOURCE
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>

#define TRUE 1
#define FALSE 0
#define MAX_SIZE 9999
/* stany procesu */
typedef enum {InRun, InSend, InFinish, InWait, InSection, InEnd} state_t;
extern state_t stan;
// typy akcji
typedef enum { GET_DESKS, GET_R00M, GET_FIELD, SKIP, GET_DESK_AFTER_FINISH } action_t;
extern action_t actionType;
extern int rank; //id procesu
extern int size; //liczba procesów
extern int clk; // zegar
extern int priority;
extern int maxDesksCount;
extern int maxRoomsCount;
extern int maxFieldsCount;
extern int groupSize;
extern bool finishProcess;

typedef struct {
    int ts; //zegar
    int src; //od kogo pakiet idzie
    action_t actionType; //o co sie ubiegamy
    int groupSize; //rozmiar grupy
	int priority; //priorytet
} packet_t;

extern int endDetectionCounter; //wykrywamy tym koniec programu
extern packet_t queue[MAX_SIZE]; //kolejka gdy ubiegamy sie o sekcje krytyczną

extern MPI_Datatype MPI_PAKIET_T;

/* Typy wiadomości */
#define FINISH 0 //nie zaimplementowano
#define REQ 1 //żądanie ubiegania się o zasób
#define RES 2 //odpowiedź na żądanie
#define REL 3 //zwolnienie zasobu

#define SEC_IN_STATE 2

#ifdef DEBUG
#define debug(FORMAT,...) printf("%c[%d;%dm [%d | %d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, clk, ##__VA_ARGS__, 27,0,37);
#else
#define debug(...) ;
#endif

#define P_WHITE printf("%c[%d;%dm",27,1,37);
#define P_BLACK printf("%c[%d;%dm",27,1,30);
#define P_RED printf("%c[%d;%dm",27,1,31);
#define P_GREEN printf("%c[%d;%dm",27,1,33);
#define P_BLUE printf("%c[%d;%dm",27,1,34);
#define P_MAGENTA printf("%c[%d;%dm",27,1,35);
#define P_CYAN printf("%c[%d;%d;%dm",27,1,36);
#define P_SET(X) printf("%c[%d;%dm",27,1,31+(6+X)%7);
#define P_CLR printf("%c[%d;%dm",27,0,37);

#define println(FORMAT, ...) printf("%c[%d;%dm [%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, ##__VA_ARGS__, 27,0,37);

void sendPacket(packet_t *pkt, int destination, int tag);
void removeFromQueue(packet_t rcvPacket); // gdy nie można wejść do sekcji, czekamy i odbieramy pakiety. metoda ta też sprawdza czy można wejśc do sekcji
void changeState(state_t);
void insertToQueueOnRes(packet_t rcvPacket); //do kolejki wstawia gdy odbierze się RES w wątku_kom -> w sumie to nie wiem czy to działa xD
void insertToQueueOnReq(packet_t rcvPacket); //do kolejki wstawia gdy odbierze się REQ w wątku_kom -> w sumie to nie wiem czy to działa xD
void insertToEndQueue(packet_t rcvPacket); //kolejka gdy chcemy zakończyć program
void manageCriticalSection(); //zarządzanie sekcja krytyczną
void changeClock(int); //zmiana zegara przy odbieraniu wiadomości
void incrementClock(); //zmiana zegara przy wysyłaniu wiadomości
void insertInitialPackage();	//gdy zaczynamy ubiegać się o zasób, to wstawiamy nasz pakiet do koejki. 
								//jak wcześniej proces otrzymał REQ o takim samym typie to nie robimy tego, bo priorytet != 0 i nie możemy tego nadpisywać (chyba)
void setActionState(action_t newState);
char* getActionName(action_t action); //zwraca string na podstawie typu akcji, żeby w debugu ładnie było
void sortArray(packet_t arr[MAX_SIZE]);
void incrementEndCounter();
#endif
