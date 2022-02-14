#include "main.h"
#include "watek_glowny.h"

bool canEnterCriticalSection(int maxCount, bool validateGroupSize, packet_t temp[MAX_SIZE])
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

bool canGetDesks(int maxCount, packet_t temp[MAX_SIZE])
{
	int sum = 0;
	for(int i = 0; i < size; i++)
	{
		if(temp[i].actionType == GET_DESKS || temp[i].actionType == GET_DESK_AFTER_FINISH)
		{
			if (temp[i].actionType == GET_DESK_AFTER_FINISH) {
				sum += 1;
			} else {
				sum = sum + temp[i].groupSize;
			}
			if(rank == temp[i].src)
			{
				break;
			}
		}
	}
	if(sum <= maxCount) 
		return true;
	else
		return false;
}

void enterCriticalSection(packet_t temp[MAX_SIZE])
{
	packet_t *pkt = malloc(sizeof(packet_t));
	pkt->actionType = actionType;
	debug("Wchodzę do sekcji krytycznej.        Typ: %s, Priorytet: %d, Rozmiar: %d", getActionName(actionType), priority, groupSize);
	sleep(5);
	debug("Wychodzę z sekcji krytycznej.        Typ: %s", getActionName(actionType));

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
			debug("wysyłam zwolnienie zasobu do %d. akcja: %s, rozmiar: %d", temp[i].src, getActionName(actionType), groupSize);
			sendPacket(pkt, temp[i].src, REL);
		}
	}

	if(actionType == GET_DESKS)
		setActionState(GET_R00M);
	else if(actionType == GET_R00M)
		setActionState(GET_FIELD);
	else if(actionType == GET_FIELD)
	{
		setActionState(GET_DESK_AFTER_FINISH);
		finishProcess = true;
	} else if (actionType == GET_DESK_AFTER_FINISH) {
		setActionState(GET_DESKS);
	}

	free(pkt);
	memset(queue, 0, sizeof(queue));
	priority = 0;
	changeState(InRun);
}

void manageCriticalSection()
{
	packet_t temp[MAX_SIZE];
	for (int i = 0; i < size; i++)
	{
		temp[i] = queue[i];
	}
	sortArray(temp);
	bool result = false;
	switch (actionType)
	{
		case GET_DESKS:
			result = canGetDesks(maxDesksCount, temp);
			break;
		case GET_R00M:
			result = canEnterCriticalSection(maxRoomsCount, false, temp);
			break;
		case GET_FIELD:
			result = canEnterCriticalSection(maxFieldsCount, false, temp);
			break;
		case GET_DESK_AFTER_FINISH:
			result = canGetDesks(maxFieldsCount, temp);
		default: break;
	}
	if(result)
	{
		enterCriticalSection(temp);
	}
	else
	{
		debug("Nie mogę wejść do sekcji krytycznej. Typ: %s, Priorytet: %d, Rozmiar: %d", getActionName(actionType), priority, groupSize);
		changeState(InWait);
	}
}

void mainLoop()
{
	srandom(rank);
	packet_t *pkt = malloc(sizeof(packet_t));
	srand(time(NULL) + rank);
	groupSize = rand() % maxDesksCount + 1;
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

		sleep(SEC_IN_STATE);
	}
}
