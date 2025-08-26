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
		isSending = false;
		bDisconnected = false;
		ioCount = 0;
		lockRef = 0;
		InitializeSRWLock(&lock);
	}
	~Session()
	{
		delete sendQ;
		delete recvQ;
	}

public:
	SOCKET sock;
	ULONGLONG sessionId;
	SessionOverlapped sendOlp;
	SessionOverlapped recvOlp;
	CRingBuffer* sendQ;
	CRingBuffer* recvQ;
	SRWLOCK lock;
	LONG isSending;
	LONG lockRef;
	bool bDisconnected;
	
	unsigned int ioCount;
};

void NetStartUp();

unsigned int AcceptProc(void *arg);

unsigned int WorkerThreadNetProc(void* arg);

unsigned int EchoThreadProc(void* arg);

void OnMessage(ULONG sessionId, CPacket* message);

//-------------------------------------------
// SendPacket : SendQ�� ������ ������ Enqueue ��, SendPost ȣ�� 
// - ���� �� ���� ���� �÷��� Ȱ��ȭ
// [ SendPacket ���� ��� ]
// - SendQ�� �� �� ���
//-------------------------------------------
bool SendPacket(ULONG sessionId, CPacket* packet);


// --------------------------------------------------------------
// RecvPost : IOCount ���� ��, RecvQ�� ������ ���� ��û
// - RecvQ�� ������ ��Ȳ�̸� IOCount�� �������� ������ �ٷ� ����
// - ���� �÷��װ� Ȱ��ȭ �Ǿ��ִٸ� IOCount�� �������� ������ �ٷ� ����
// 
// ��ȯ ��
// - �Լ� ���ο��� ReleaseSession�� ȣ������ �ʾ����� true
// - �Լ� ���ο��� ReleaseSession�� ȣ�������� false
// --------------------------------------------------------------
bool RecvPost(Session* session);

// --------------------------------------------------------------
// SendPost : IOCount ���� ��, SendQ�� �ִ� �����͸� ���� ��û
// - �̹� WSASend�� �ɷ��ִٸ� IOCount�� �������� ������ �ٷ� ����
// - ���� �÷��װ� Ȱ��ȭ �Ǿ��ִٸ� IOCount�� �������� ������ �ٷ� ����
// - SendQ�� �����Ͱ� ���ٸ� IOCount�� �������� ������ �ٷ� ����
// 
// ������
// - �� �Լ��� ȣ���ϱ� ������ Session�� ���� ���� �ɷ��־���Ѵ�.
// - �� �Լ��� ȣ���� �ܺο����� Session�� ���� ���� �����ؾ��Ѵ�.
// 
// ��ȯ ��
// - �Լ� ���ο��� ReleaseSession�� ȣ������ �ʾ����� true
// - �Լ� ���ο��� ReleaseSession�� ȣ�������� false
// --------------------------------------------------------------
bool SendPost(Session* session);


// --------------------------------------------------------------
// ���� ���� �� �ش� ���� ����
// - �� �Լ��� ������ IOCount�� 0�� ���� ȣ��Ǿ�� ��
// --------------------------------------------------------------
void ReleaseSession(Session* session);


void SessionLock(Session* session);
void SessionUnlock(Session* session);