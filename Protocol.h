#pragma once

#define SERVERPORT 6000

#define MAX_ECHOBYTE 8

struct st_Header
{
	unsigned short payloadLen;
};