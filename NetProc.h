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

//-------------------------------------------
// SendPacket : SendQ에 전송할 데이터 Enqueue 후, SendPost 호출 
// - 실패 시 세션 종료 플래그 활성화
// [ SendPacket 실패 요소 ]
// - SendQ가 다 찬 경우
//-------------------------------------------
bool SendPacket(Session* session, CPacket* packet);


// --------------------------------------------------------------
// RecvPost : IOCount 증가 후, RecvQ로 데이터 수신 요청
// - RecvQ가 가득찬 상황이면 IOCount는 증가하지 않으며 바로 리턴
// - 종료 플래그가 활성화 되어있다면 IOCount는 증가하지 않으며 바로 리턴
// --------------------------------------------------------------
void RecvPost(Session* session);

// --------------------------------------------------------------
// SendPost : IOCount 증가 후, SendQ에 있는 데이터를 전송 요청
// - 이미 WSASend가 걸려있다면 IOCount는 증가하지 않으며 바로 리턴
// - 종료 플래그가 활성화 되어있다면 IOCount는 증가하지 않으며 바로 리턴
// - SendQ에 데이터가 없다면 IOCount는 증가하지 않으며 바로 리턴
// --------------------------------------------------------------
void SendPost(Session* session);


// --------------------------------------------------------------
// 연결 종료 및 해당 세션 삭제
// - 이 함수는 세션의 IOCount가 0일 때만 호출되어야 함
// --------------------------------------------------------------
void DisconnectSession(Session* session);