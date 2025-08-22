#pragma once
#include <WinSock2.h>
#include <unordered_map>
#include "RingBuffer.h"
#include "CPacket.h"
#pragma comment (lib, "ws2_32")

#define SERVERPORT 6000

enum EIOTYPE
{
	ERecv = 0,
	ESend = 1,
};
struct SessionOverlapped
{
	WSAOVERLAPPED overlapped;
	EIOTYPE type; // send���� recv����
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

//-------------------------------------------
// SendPacket : SendQ�� ������ ������ Enqueue ��, SendPost ȣ�� 
// - ���� �� ���� ���� �÷��� Ȱ��ȭ
// [ SendPacket ���� ��� ]
// - SendQ�� �� �� ���
//-------------------------------------------
bool SendPacket(Session* session, CPacket* packet);


// --------------------------------------------------------------
// RecvPost : IOCount ���� ��, RecvQ�� ������ ���� ��û
// - RecvQ�� ������ ��Ȳ�̸� IOCount�� �������� ������ �ٷ� ����
// - ���� �÷��װ� Ȱ��ȭ �Ǿ��ִٸ� IOCount�� �������� ������ �ٷ� ����
// --------------------------------------------------------------
void RecvPost(Session* session);

// --------------------------------------------------------------
// SendPost : IOCount ���� ��, SendQ�� �ִ� �����͸� ���� ��û
// - �̹� WSASend�� �ɷ��ִٸ� IOCount�� �������� ������ �ٷ� ����
// - ���� �÷��װ� Ȱ��ȭ �Ǿ��ִٸ� IOCount�� �������� ������ �ٷ� ����
// - SendQ�� �����Ͱ� ���ٸ� IOCount�� �������� ������ �ٷ� ����
// --------------------------------------------------------------
void SendPost(Session* session);


// --------------------------------------------------------------
// ���� ���� �� �ش� ���� ����
// - �� �Լ��� ������ IOCount�� 0�� ���� ȣ��Ǿ�� ��
// --------------------------------------------------------------
void DisconnectSession(Session* session);