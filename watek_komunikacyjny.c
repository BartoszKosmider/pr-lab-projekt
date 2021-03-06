#include "main.h"
#include "watek_komunikacyjny.h"

/* wątek komunikacyjny; zajmuje się odbiorem i reakcją na komunikaty */
void *startKomWatek(void *ptr)
{
	MPI_Status status;
	int is_message = FALSE;
	packet_t pakiet;
	packet_t *pkt = malloc(sizeof(packet_t));

	/* Obrazuje pętlę odbierającą pakiety o różnych typach */
	while ( stan!=InFinish ) 
	{
		MPI_Recv( &pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		changeClock(pakiet.ts);
		switch ( status.MPI_TAG ) 
		{
			case RES:
				// debug("odbieram odpowiedź od %d. rozmiar: %d, akcja: %s, clk: %d", 
					// pakiet.src, pakiet.groupSize, getActionName(pakiet.actionType), pakiet.ts);
				insertToQueueOnRes(pakiet);
				break;
			case REQ:
				// debug("odbieram żądanie od %d. rozmiar: %d, akcja: %s, clk: %d", 
					// pakiet.src, pakiet.groupSize, getActionName(pakiet.actionType), pakiet.ts);
				insertToQueueOnReq(pakiet);
				sendPacket(pkt, pakiet.src, RES);
				// debug("wysyłam odpowiedź do %d. Typ: %s",pakiet.src, getActionName(actionType));
				break;
			case REL:
				// debug("odbieram zwolnienie zasobu od %d. rozmiar: %d, akcja: %s, clk: %d", 
				// 	pakiet.src, pakiet.groupSize, getActionName(pakiet.actionType), pakiet.ts);
				removeFromQueue(pakiet);
				break;
			default:
				break;

			sleep(1);
		}
	}
}

