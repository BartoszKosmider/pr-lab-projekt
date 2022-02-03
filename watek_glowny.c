#include "main.h"
#include "watek_glowny.h"

bool canEnterCriticalSection(int maxCount, bool validateGroupSize, packet_t temp[4])
{
	int sum = 0;
	for(int i = 0; i < size; i++)
	{
		if(temp[i].actionType == SKIP || temp[i].actionType != actionType)
		{
			continue;
		}
		if(validateGroupSize)
			sum = sum + temp[i].groupSize;
		else
			sum++;
		if(rank == temp[i].src)
		{
			break;
		}
	}
	if(sum <= maxCount) 
		return true;
	else
		return false;
}

void enterCriticalSection(packet_t temp[4])
{
	packet_t *pkt = malloc(sizeof(packet_t));
	pkt->actionType = actionType;
	debug("Wchodzę do sekcji krytycznej. Typ: %s, Priorytet: %d, Rozmiar: %d", getActionName(actionType), priority, groupSize);
	sleep(5);
	debug("Wychodzę z sekcji krytycznej. Typ: %s", getActionName(actionType));

	for (int i = 0; i < size; i++)
	{
		temp[i] = queue[i]; //odświeżenie danych
	}
	sortArray(temp);

	bool startSendingRel = false;
	for (int i = 0; i < size; i++)
	{
		if(temp[i].src == rank)
		{
			startSendingRel = true;
			continue;
		}
		if(temp[i].actionType == actionType && startSendingRel)
		{
			// debug("wysyłam zwolnienie zasobu do %d. akcja: %s, rozmiar: %d", temp[i].src, getActionName(actionType), groupSize);
			sendPacket(pkt, temp[i].src, REL);
		}
	}
	if(finishProcess)
	{
		debug("END DETECTED");
		changeState(InEnd);
		return;
	}

	if(actionType == GET_DESKS)
		setActionState(GET_R00M);
	else if(actionType == GET_R00M)
		setActionState(GET_FIELD);
	else if(actionType == GET_FIELD)
	{
		setActionState(GET_DESKS);
		groupSize = 1;
		incrementEndCounter();
		finishProcess = true;
	}

	free(pkt);
	memset(queue, 0, sizeof(queue));
	setPriority();
	changeState(InRun);
}

void manageCriticalSection()
{
	packet_t temp[4];
	for (int i = 0; i < size; i++)
	{
		temp[i] = queue[i];
	}
	sortArray(temp);
	bool result = false;
	switch (actionType)
	{
		case GET_DESKS:
			result = canEnterCriticalSection(maxDesksCount, true, temp);
			break;
		case GET_R00M:
			result = canEnterCriticalSection(maxRoomsCount, false, temp);
			break;
		case GET_FIELD:
			result = canEnterCriticalSection(maxFieldsCount, false, temp);
			break;
		default: break;
	}
	if(result)
	{
		enterCriticalSection(temp);
	}
	else
	{
		debug("Nie mogę wejść do sekcji krytycznej. Typ: %s, Priorytet: %d", getActionName(actionType), priority);
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
			changeState(InSend);
			sleep(1);
			insertInitialPackage();
			for (int i = 0; i < size; i++)
			{
				if(rank == i)
					continue;

				// debug("wysyłam żądanie do %d. akcja: %s, rozmiar: %d", i, getActionName(actionType), groupSize);
				sendPacket(pkt, i ,REQ);
			}
		}

		if(stan == InSection)
		{
			manageCriticalSection();
		}

		if (stan==InEnd) 
		{
			for (int i = 0; i < size; i++)
			{
				if(rank == i)
					continue;
				setActionState(END);
				// debug("wysyłam FINISH do %d. akcja: %s, rozmiar: %d", i, getActionName(actionType), groupSize);
				sendPacket(pkt, i ,FINISH);
			}
			changeState(InWait);
		}

		if(stan == InWait)
		{
			detectEnd();
		}

		sleep(SEC_IN_STATE);
	}
}
