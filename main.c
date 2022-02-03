#include "main.h"
#include "watek_komunikacyjny.h"
#include "watek_glowny.h"
// #include "monitor.h"
/* wątki */
#include <pthread.h>

/* sem_init sem_destroy sem_post sem_wait */
//#include <semaphore.h>
/* flagi dla open */
//#include <fcntl.h>

state_t stan=InRun;
action_t actionType = GET_DESKS;
volatile char end = FALSE;
int size,rank, tallow ; /* nie trzeba zerować, bo zmienna globalna statyczna */
int groupSize;
int maxDesksCount;
int maxRoomsCount;
int maxFieldsCount;
bool finishProcess = false;
packet_t queue[4];
packet_t endQueue[4];
int clk = 0;
int priority = 0;

MPI_Datatype MPI_PAKIET_T;
pthread_t threadKom, threadMon;

pthread_mutex_t stateMut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t tallowMut = PTHREAD_MUTEX_INITIALIZER;

int counter = 1;

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
		case EXIT:
			return "Zakoncz";
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

/* srprawdza, czy są wątki, tworzy typ MPI_PAKIET_T
*/
void inicjuj(int *argc, char ***argv)
{
	int provided;
	MPI_Init_thread(argc, argv,MPI_THREAD_MULTIPLE, &provided);
	check_thread_support(provided);

	/* Stworzenie typu */
	/* Poniższe (aż do MPI_Type_commit) potrzebne tylko, jeżeli
	   brzydzimy się czymś w rodzaju MPI_Send(&typ, sizeof(pakiet_t), MPI_BYTE....
	*/
	/* sklejone z stackoverflow */
	const int nitems=5; /* bo packet_t ma trzy pola */
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
	// if (rank==0) {
	// 	pthread_create( &threadMon, NULL, startMonitor, 0);
	// }
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
	pthread_join(threadKom,NULL);
	if (rank==0) pthread_join(threadMon,NULL);
	MPI_Type_free(&MPI_PAKIET_T);
	MPI_Finalize();
}

void sortArray()
{
	int i, j, min;
	packet_t temp;
	for(i = 0; i < size; i++)
	{		
		for(j = i + 1; j < size; j++)
		{
			if(queue[i].priority > queue[j].priority)
			{
				temp  = queue[i];
				queue[i] = queue[j];
				queue[j] = temp;
			}
			else if(queue[i].priority == queue[j].priority)
			{
				if(queue[i].src > queue[j].src)
				{
					temp  = queue[i];
					queue[i] = queue[j];
					queue[j] = temp;
				}
			}
		}
	}
}

void displayArray()
{
	for(int i = 0; i < size; i++)
		printf("i: %d, rank: %d ts: %d groupsize: %d  src: %d, priority: %d, actionType: %d\n", 
			i, rank, queue[i].ts, queue[i].groupSize, queue[i].src, queue[i].priority, queue[i].actionType);
}

void setActionState(action_t newState)
{
	pthread_mutex_lock( &tallowMut );
	if (stan==InFinish) 
	{ 
		pthread_mutex_unlock( &tallowMut );
		return;
	}
	actionType = newState;
	pthread_mutex_unlock( &tallowMut );
}

void removeFromQueue(packet_t rcvPacket)
{
	for (int i = 0; i < size; i++)
	{
		if(queue[i].src == rcvPacket.src)
		{
			queue[i].actionType = SKIP;
			// printf("RANK: %d  !!   usunieto pakiet ID: %d\n", rank, rcvPacket.src);
			manageCriticalSection(actionType); // gdy proces nie może wejsc do sekcji, czeka na odpowiedz i usuwa inne procesy z kolejki kalkulujac za każdym razem, czy może się wpierdolic
			return;
		}
	}
}

void insertToEndQueue(packet_t rcvPacket)
{
	rcvPacket.actionType = END;
	endQueue[rcvPacket.src] = rcvPacket;

	for (int i = 0; i < size; i++)
	{
		printf("rank: %d, endqueue: %d, action1: %d, action2: %d\n", rank, endQueue[i].src, actionType, endQueue[i].actionType);
		if(endQueue[i].actionType != END)
			return;
	}
	changeState(InFinish);
}

// wstawia do kolejki odebrane wiadomosci jezeli stan tych jest taki sam jak nasz, gdy odbierze wszystkie sprawdza czy moze wejsc do sekcji
void insertToQueue(packet_t rcvPacket)
{
	if(rcvPacket.actionType != actionType)
		rcvPacket.actionType = SKIP;

	queue[rcvPacket.src] = rcvPacket;
	counter++;

	if(counter == size)
	{
		sortArray();
		// displayArray();
		changeState(InSection);
		counter = 1;
	}
}

void insertToQueue2(packet_t rcvPacket)
{
	for (int i = 0; i < size; i++)
	{
		if(queue[i].src == rcvPacket.src)
			queue[i] = rcvPacket;
	}
	// queue[rcvPacket.src] = rcvPacket;
	if(priority == 0)
	{
		// printf("USTANOWIONO NOWY PAKIET INIT priority: %d, action: %d \n", priority, actionType);
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
	pthread_mutex_lock( &tallowMut );
	priority = clk + 1;
	queue[rank].ts = clk + 1;
	queue[rank].src = rank;
	queue[rank].groupSize = groupSize;
	queue[rank].actionType = actionType;
	queue[rank].priority = priority;
	// printf("RANK: %d INSERT ON START, ts: %d, src: %d, gs: %d, at: %d, priority: %d\n", rank, clk + 1,queue[rank].src, groupSize, actionType, priority);
	pthread_mutex_unlock( &tallowMut );
}

void insertFinishPackage()
{
	pthread_mutex_lock( &tallowMut );
	priority = clk + 1;
	queue[rank].ts = clk + 1;
	queue[rank].src = rank;
	queue[rank].groupSize = groupSize;
	queue[rank].actionType = END;
	queue[rank].priority = priority;
	// printf("RANK: %d INSERT ON START, ts: %d, src: %d, gs: %d, at: %d, priority: %d\n", rank, clk + 1,queue[rank].src, groupSize, actionType, priority);
	pthread_mutex_unlock( &tallowMut );
}

/* opis patrz main.h */
void sendPacket(packet_t *pkt, int destination, int tag)
{
	int freepkt=0;
	if (pkt==0) 
	{ 
		pkt = malloc(sizeof(packet_t)); 
		freepkt=1;
	}
	incrementClock(destination);
	pkt->ts = clk;
	pkt->actionType = actionType;
	pkt->groupSize = groupSize;
	pkt->src = rank;
	pkt->priority = priority;
	// printf("RANK %d  WYSYLAM WIADOMOŚĆ!! DO: %d CLK %d priority: %d\n",rank, destination, clk, priority);
	MPI_Send( pkt, 1, MPI_PAKIET_T, destination, tag, MPI_COMM_WORLD);
	if (freepkt) free(pkt);
}

void changeClock( int newClock )
{
	// if(rank == 1) printf("RANK: %d    |   CHANGE CLOCK OLD %d, RCV %d\n", rank, clk, newClock);
	pthread_mutex_lock( &tallowMut );
	if (stan==InFinish) 
	{ 
	pthread_mutex_unlock( &tallowMut );
		return;
	}
	if(clk >= newClock)
		clk = clk + 1;
	else
		clk = newClock + 1;
	pthread_mutex_unlock( &tallowMut );
}

void incrementClock(int destination)
{
	pthread_mutex_lock( &tallowMut );
	if (stan==InFinish) { 
	pthread_mutex_unlock( &tallowMut );
		return;
	}
	clk++;
	pthread_mutex_unlock( &tallowMut );
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
	/* Tworzenie wątków, inicjalizacja itp */
	readConfigFile();
	inicjuj(&argc,&argv); // tworzy wątek komunikacyjny w "watek_komunikacyjny.c"
	tallow = 1000; // by było wiadomo ile jest łoju
	mainLoop();          // w pliku "watek_glowny.c"

	finalizuj();
	return 0;
}

