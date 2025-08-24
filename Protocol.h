#pragma once

#define SERVERPORT 6000

#define MAX_ECHOBYTE 8

struct st_Header
{
	unsigned short payloadLen;
};


struct st_JobMessage
{
	ULONG sessionId;
	__int64 echoData;
};