#pragma once
#include <WinSock2.h>
#include "RingBuffer.h"
#pragma comment (lib, "ws2_32")

#define SERVERPORT 2000

enum EIOTYPE
{
	ERecv = 0,
	ESend = 1,
};
struct SessionOverlapped
{
	WSAOVERLAPPED overlapped;
	EIOTYPE type; // send인지 recv인지
};

class Session
{
public:
	Session(SOCKET _sock, unsigned int _id)
	{
		sock = _sock;
		sessionId = _id;
		recvOlp.type = ERecv;
		sendOlp.type = ESend;
		sendQ = new CRingBuffer();
		recvQ = new CRingBuffer();
		bIsSending = false;
		bDisconnected = false;
		ioCount = 0;
		InitializeSRWLock(&lock);
	}
	~Session()
	{
		delete sendQ;
		delete recvQ;
	}

public:
	SOCKET sock;
	unsigned int sessionId;
	SessionOverlapped sendOlp;
	SessionOverlapped recvOlp;
	CRingBuffer* sendQ;
	CRingBuffer* recvQ;
	SRWLOCK lock;
	bool bIsSending;
	bool bDisconnected;
	unsigned int ioCount;
};

void NetStartUp();

unsigned int AcceptProc(void *arg);

unsigned int WorkerThreadNetProc(void* arg);

void RecvPost(Session* session);

void SendPost(Session* session);

void TryDeleteSession(Session* session);