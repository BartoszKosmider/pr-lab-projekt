#include "main.h"
#include "watek_komunikacyjny.h"
#include "watek_glowny.h"
#include <pthread.h>

state_t stan=InRun;
action_t actionType = GET_DESKS;
volatile char end = FALSE;
int size, rank;
int groupSize;
int maxDesksCount;
int maxRoomsCount;
int maxFieldsCount;
bool finishProcess = false;
packet_t queue[4];
int clk = 0;
int priority = 0;
int counter = 1;
int endDetectionCounter = 0;

MPI_Datatype MPI_PAKIET_T;
pthread_t threadKom;
pthread_mutex_t stateMut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clkMut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t actionMut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queueMut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t endClkMut = PTHREAD_MUTEX_INITIALIZER;

char* getActionName(action_t action)
{
	switch (action)
	{
		case GET_DESKS:
			return "Zajmij biurka";
			break;
		case GET_R00M:
			return "Zajmij sale";
			break;	
		case GET_FIELD:
			return "Zajmij pole";
			break;
		case END:
			return "Zakończ proces";
			break;
		default:
			return "Nie zdefiniowano akcji";
			break;
	}
}

void check_thread_support(int provided)
{
	printf("THREAD SUPPORT: chcemy %d. Co otrzymamy?\n", provided);
	switch (provided) {
		case MPI_THREAD_SINGLE: 
			printf("Brak wsparcia dla wątków, kończę\n");
			/* Nie ma co, trzeba wychodzić */
		fprintf(stderr, "Brak wystarczającego wsparcia dla wątków - wychodzę!\n");
		MPI_Finalize();
		exit(-1);
		break;
		case MPI_THREAD_FUNNELED: 
			printf("tylko te wątki, ktore wykonaly mpi_init_thread mogą wykonać wołania do biblioteki mpi\n");
		break;
		case MPI_THREAD_SERIALIZED: 
			/* Potrzebne zamki wokół wywołań biblioteki MPI */
			printf("tylko jeden watek naraz może wykonać wołania do biblioteki MPI\n");
		break;
		case MPI_THREAD_MULTIPLE: printf("Pełne wsparcie dla wątków\n"); /* tego chcemy. Wszystkie inne powodują problemy */
		break;
		default: printf("Nikt nic nie wie\n");
	}
}

void inicjuj(int *argc, char ***argv)
{
	int provided;
	MPI_Init_thread(argc, argv,MPI_THREAD_MULTIPLE, &provided);
	check_thread_support(provided);

	const int nitems=5;
	int blocklengths[5] = {1,1,1,1,1};
	MPI_Datatype typy[5] = {MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT};

	MPI_Aint offsets[5]; 
	offsets[0] = offsetof(packet_t, ts);
	offsets[1] = offsetof(packet_t, src);
	offsets[2] = offsetof(packet_t, groupSize);
	offsets[3] = offsetof(packet_t, actionType);
	offsets[4] = offsetof(packet_t, priority);

	MPI_Type_create_struct(nitems, blocklengths, offsets, typy, &MPI_PAKIET_T);
	MPI_Type_commit(&MPI_PAKIET_T);

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	srand(rank);
	
	pthread_create(&threadKom, NULL, startKomWatek , 0);
	debug("jestem");
}

void setPriority()
{
	priority = 0;
}

void finalizuj()
{
	pthread_mutex_destroy( &stateMut);
	/* Czekamy, aż wątek potomny się zakończy */
	println("czekam na wątek \"komunikacyjny\"\n" );
	// pthread_join(threadKom, NULL);
	int rc = pthread_cancel(threadKom); //recv blokuje i powstaje niemożność zakończenia wątku, nie wiem czy to poprawne rozwiązanie
	if(rc) printf("failed to cancel the thread\n");
	MPI_Type_free(&MPI_PAKIET_T);
	MPI_Finalize();
}

void sortArray(packet_t arr[4])
{
	int i, j, min;
	packet_t temp;
	for(i = 0; i < size; i++)
	{		
		for(j = i + 1; j < size; j++)
		{
			if(arr[i].priority > arr[j].priority)
			{
				temp  = arr[i];
				arr[i] = arr[j];
				arr[j] = temp;
			}
			else if(arr[i].priority == arr[j].priority)
			{
				if(arr[i].src > arr[j].src)
				{
					temp  = arr[i];
					arr[i] = arr[j];
					arr[j] = temp;
				}
			}
		}
	}
}

void displayArray(packet_t arr[4])
{
	for(int i = 0; i < size; i++)
		printf("i: %d, rank: %d ts: %d groupsize: %d  src: %d, priority: %d, actionType: %d\n", 
			i, rank, arr[i].ts, arr[i].groupSize, arr[i].src, arr[i].priority, arr[i].actionType);
}

void setActionState(action_t newState)
{
	pthread_mutex_lock( &actionMut );
	if (stan==InFinish) 
	{ 
		pthread_mutex_unlock( &actionMut );
		return;
	}
	actionType = newState;
	pthread_mutex_unlock( &actionMut );
}

void removeFromQueue(packet_t rcvPacket)
{
	for (int i = 0; i < size; i++)
	{
		if(queue[i].src == rcvPacket.src)
		{
			queue[i].actionType = SKIP;
			if(stan == InWait)
				manageCriticalSection(); // gdy proces nie może wejsc do sekcji, czeka na odpowiedz i usuwa inne procesy z kolejki kalkulujac za każdym razem, czy może się wpierdolic
			return;
		}
	}
}

// wstawia do kolejki odebrane wiadomosci jezeli stan tych jest taki sam jak nasz, gdy odbierze wszystkie sprawdza czy moze wejsc do sekcji
void insertToQueueOnRes(packet_t rcvPacket)
{
	if(rcvPacket.actionType != actionType)
		rcvPacket.actionType = SKIP;

	queue[rcvPacket.src] = rcvPacket;
	counter++;

	if(counter == size)
	{
		changeState(InSection);
		counter = 1;
	}
}

void insertToQueueOnReq(packet_t rcvPacket)
{
	for (int i = 0; i < size; i++)
	{
		if(queue[i].src == rcvPacket.src)
			queue[i] = rcvPacket;
	}
	if(priority == 0)
	{
		priority = clk;
		queue[rank].ts = clk;
		queue[rank].src = rank;
		queue[rank].groupSize = groupSize;
		queue[rank].actionType = actionType;
		queue[rank].priority = priority;
	}
}

void insertInitialPackage()
{
	if(queue[rank].priority != 0) return;
	pthread_mutex_lock( &queueMut );
	priority = clk + 1;
	queue[rank].ts = clk + 1;
	queue[rank].src = rank;
	queue[rank].groupSize = groupSize;
	queue[rank].actionType = actionType;
	queue[rank].priority = priority;
	pthread_mutex_unlock( &queueMut );
}

//podbija sie tutaj zegar i mapuje pakiet do wyslania
void sendPacket(packet_t *pkt, int destination, int tag)
{
	incrementClock(destination);
	pkt->ts = clk;
	pkt->actionType = actionType;
	pkt->groupSize = groupSize;
	pkt->src = rank;
	pkt->priority = priority;
	MPI_Send( pkt, 1, MPI_PAKIET_T, destination, tag, MPI_COMM_WORLD);
}

void changeClock(int newClock)
{
	pthread_mutex_lock( &clkMut );
	if (stan==InFinish) 
	{
	pthread_mutex_unlock( &clkMut );
		return;
	}
	if(clk >= newClock)
		clk = clk + 1;
	else
		clk = newClock + 1;
	pthread_mutex_unlock( &clkMut );
}

void incrementClock()
{
	pthread_mutex_lock( &clkMut );
	if (stan==InFinish) { 
	pthread_mutex_unlock( &clkMut );
		return;
	}
	clk++;
	pthread_mutex_unlock( &clkMut );
}

void incrementEndCounter()
{
	pthread_mutex_lock( &endClkMut );
	if (stan==InFinish) 
	{ 
		pthread_mutex_unlock( &endClkMut );
		return;
	}
	endDetectionCounter++;
	pthread_mutex_unlock( &endClkMut );
}

void detectEnd()
{
	if(endDetectionCounter == size)
		changeState(InFinish);
}

void changeState( state_t newState )
{
	pthread_mutex_lock( &stateMut );
	if (stan==InFinish) 
	{ 
		pthread_mutex_unlock( &stateMut );
		return;
	}
	stan = newState;
	pthread_mutex_unlock( &stateMut );
}

void readConfigFile()
{
	FILE *fileName;
	char buf[100];
	fileName = fopen("config.txt", "r");
	while(fgets(buf, 100, fileName) != NULL)
	{
		char temp[100];
		if(sscanf(buf, "DESKS	%s", temp) == 1)
		{
			maxDesksCount = atoi(temp);
		}
		if(sscanf(buf, "ROOMS	%s", temp) == 1)
		{
			maxRoomsCount = atoi(temp);
		}
		if(sscanf(buf, "FIELDS	%S", temp) == 1)
		{
			maxFieldsCount = atoi(temp);
		}
	}
	fclose(fileName);
}

int main(int argc, char **argv)
{
	readConfigFile();
	inicjuj(&argc,&argv); 
	mainLoop();
	finalizuj();
	return 0;
}

