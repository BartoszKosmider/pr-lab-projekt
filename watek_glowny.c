#include "main.h"
#include "watek_glowny.h"

bool canEnterCriticalSection(int maxCount, bool validateGroupSize)
{
	int sum = 0;
	for(int i = 0; i < size; i++)
	{
		if(queue[i].actionType == SKIP || queue[i].actionType != actionType)
		{
			continue;
		}
		if(validateGroupSize)
			sum = sum + queue[i].groupSize;
		else
			sum++;
		if(rank == queue[i].src)
		{
			break;
		}
	}
	if(sum <= maxCount) 
		return true;
	else
		return false;
}

void enterCriticalSection()
{
	packet_t *pkt = malloc(sizeof(packet_t));
	pkt->actionType = actionType;
	debug("Wchodzę do sekcji krytycznej. Typ: %s", getActionName(actionType));
	sleep(5);
	debug("Wychodzę z sekcji krytycznej. Typ: %s", getActionName(actionType));

	bool startSendingRel = false;
	for (int i = 0; i < size; i++)
	{
		if(queue[i].src == rank)
		{
			startSendingRel = true;
			continue;
		}
		if(queue[i].actionType == actionType && startSendingRel)
		{
			debug( "wysyłam zwolnienie zasobu do %d. akcja: %s, rozmiar: %d", i, getActionName(actionType), groupSize);
			sendPacket(pkt, queue[i].src, REL);
		}
	}

	// if(finishProcess)
	// {
	// 	printf("Koniec programu rank: %d, action: %d\n", rank, actionType);
	// 	insertFinishPackage();
	// 	changeState(InEnd);
	// 	return;
	// }

	if(actionType == GET_DESKS)
		setActionState(GET_R00M);
	else if(actionType == GET_R00M)
		setActionState(GET_FIELD);
	else if(actionType == GET_FIELD)
	{
		setActionState(GET_DESKS);
		// groupSize = 1;
		// finishProcess = true;
	}
	free(pkt);
	memset(queue, 0, sizeof(queue));
	setPriority();
	changeState(InRun);
}

void manageCriticalSection(int maxSize)
{
	bool result = false;
	switch (actionType)
	{
		case GET_DESKS:
			result = canEnterCriticalSection(maxDesksCount, true);
			break;
		case GET_R00M:
			result = canEnterCriticalSection(maxRoomsCount, false);
			break;
		case GET_FIELD:
			result = canEnterCriticalSection(maxFieldsCount, false);
			break;
		default: break;
	}
	if(result)
	{
		enterCriticalSection();
	}
	else
	{
		debug("Nie mogę wejść do sekcji krytycznej. Typ: %s", getActionName(actionType));
		changeState(InWait);
	}
}

void mainLoop()
{
	srandom(rank);
	packet_t *pkt = malloc(sizeof(packet_t));
	groupSize = random()%10 + 1;
	while (stan != InFinish)
	{
		if (stan==InRun) 
		{
			changeState( InSend );
			sleep(1);
			insertInitialPackage();
			for (int i = 0; i < size; i++)
			{
				if(rank == i)
					continue;

				debug("wysyłam żądanie do %d. akcja: %s, rozmiar: %d", i, getActionName(actionType), groupSize);
				sendPacket(pkt, i ,REQ);
			}
		}

		if(stan == InSection)
		{
			manageCriticalSection(maxDesksCount);
		}

		// if(stan == InEnd)
		// {
		// 	changeState( InSend );
		// 	for (int i = 0; i < size; i++)
		// 	{
		// 		if(rank == i)
		// 			continue;

		// 		printf("RANK: %d WYSYLAM END DO i: %d CLK: %d, action: %d\n", rank, i, clk, actionType);
		// 		actionType = END;
		// 		sendPacket(pkt, i ,FINISH);
		// 	}
		// }
		sleep(SEC_IN_STATE);
	}
}
