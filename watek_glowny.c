#include "main.h"
#include "watek_glowny.h"
#include "stdbool.h"

void mainLoop()
{
	srandom(rank);
	// for(int i = 0; i < size; i++)
	// 	deskQueue[i] = malloc(sizeof(packet_t));
	packet_t *pkt = malloc(sizeof(packet_t));
	pkt->actionType = GET_DESKS;
	pkt->src = rank;
	pkt->groupSize = random()%10;
	while (stan != InFinish) {
		if (stan==InRun) {
			// debug("Zmieniam stan na wysyłanie");
			changeState( InSend );
			//changeTallow( -perc); // ????????
			pkt->ts = clk + 1;
			sleep( SEC_IN_STATE); // to nam zasymuluje, że wiadomość trochę leci w kanale
										// bez tego algorytm formalnie błędny za każdym razem dawałby poprawny wynik
			for (int i = 0; i < size; i++)
			{
				if(rank == i)
					continue;
				sendPacket(pkt, i ,TALLOWTRANSPORT);
			}
			changeState( InRun );
			// debug("Skończyłem wysyłać");
		}
		
		sleep(SEC_IN_STATE);
	}
}
