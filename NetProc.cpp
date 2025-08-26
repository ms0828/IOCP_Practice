#include "NetProc.h"
#include <iostream>

#include "Protocol.h"


#define PROFILE
#include "Profiler.h"

using namespace std;

SOCKET listenSock;
HANDLE hCp;

unsigned int g_SessionIdCnt = 1;

unordered_map<unsigned int, Session*> g_SessionMap;
SRWLOCK g_SessionMapLock;


CRingBuffer g_JobQ;
SRWLOCK g_JogQLock;
HANDLE g_JobEvent;

void NetStartUp()
{
	// Job 이벤트 생성
	g_JobEvent = CreateEvent(nullptr, false, false, nullptr);


	// Completion Port 생성
	hCp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);


	// 리슨 소켓 생성 후 바인딩 및 리슨
	WSAData wsaData;	
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		cout << "startup error";
		exit(1);
	}

	listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSock == INVALID_SOCKET)
	{
		cout << "create socket error";
		exit(1);
	}
	
	LINGER linger;
	linger.l_onoff = 1;
	linger.l_linger = 0;
	int ret = setsockopt(listenSock, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(linger));


	//----------------------------------------------------------
	// I/O Pending 유도하기 위해 TCP 송신 버퍼 크기를 0으로 세팅
	//----------------------------------------------------------
	int optval = 0;
	ret = setsockopt(listenSock, SOL_SOCKET, SO_SNDBUF, (char*)&optval, sizeof(optval));


	SOCKADDR_IN servAdr;
	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = AF_INET;
	servAdr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAdr.sin_port = htons(SERVERPORT);

	int bindRet = bind(listenSock, (SOCKADDR*)&servAdr, sizeof(servAdr));
	if (bindRet == SOCKET_ERROR)
	{
		cout << "binderror error";
		exit(1);
	}

	int listenRet = listen(listenSock, SOMAXCONN);
	if (listenRet == SOCKET_ERROR)
	{
		cout << "listen error";
		exit(1);
	}
}

unsigned int AcceptProc(void* arg)
{
	
	while (1)
	{
		SOCKADDR_IN clnAdr;
		int clnAdrSz = sizeof(clnAdr);
		SOCKET clnSock = accept(listenSock, (SOCKADDR*)&clnAdr, &clnAdrSz);
		if (clnSock == INVALID_SOCKET)
		{
			continue;
		}

		//-----------------------------------------
		// 1. 세션 생성 및 세션 맵 설정
		// 2. Completion Port와 해당 세션 소켓 연결
		//-----------------------------------------
		Session* newSession = new Session(clnSock, g_SessionIdCnt++);
		AcquireSRWLockExclusive(&g_SessionMapLock);
		g_SessionMap.insert({ newSession->sessionId, newSession });
		ReleaseSRWLockExclusive(&g_SessionMapLock);

		CreateIoCompletionPort((HANDLE)newSession->sock, hCp, (ULONG_PTR)newSession->sessionId, 0);

		
		// ---------------------------------------
		// 초기 Recv 걸기
		// ---------------------------------------
		/*SessionLock(newSession);
		int recvPostRet = RecvPost(newSession);
		if (recvPostRet)
			SessionUnlock(newSession);*/

		RecvPost(newSession);
	}

	return 0;
}


unsigned int WorkerThreadNetProc(void* arg)
{
	while (1)
	{
		//--------------------- GQCS 예외 처리---------------------------
		// 1. GQCS가 false을 반환
		//		(1) CP 핸들이 닫힌 경우 (또는 타임 아웃) -> Dequeue 실패 -> overlapped == null
		//		(2) 연결이 파괴된 경우(RST) overlapped != null, transferred = 0
		//			-> 그러나 I/O가 실패했다고 즉시 처리할 게 없다.
		//				- 멀티 스레드 환경에서는 I/O 실패를 인지 후에 I/O 성공 완료 통지가 처리될 수 있음
		//				- 그 자리에서 세션을 삭제한다거나.. 등의 처리가 불가
		// 2. GQCS가 true를 반환
		//		- I/O 성공 및 Dequeue 성공
		//--------------------------------------------------------------
		DWORD transferred = 0;
		//Session* session = nullptr;
		ULONGLONG sessionId;
		SessionOverlapped* sessionOlp;
		bool gqcsRet = GetQueuedCompletionStatus(hCp, &transferred, (PULONG_PTR)&sessionId, (LPOVERLAPPED*)&sessionOlp, INFINITE);

		//--------------------------------------------------------------
		// lpOverlapped가 null인지는 무조건 확인 필요 
		// - CP 핸들이 닫힌 경우 (또는 dwMillisecond 타임 아웃) -> Dequeue 실패
		// - 이 때 completion Key와 transferred는 과거 값 그대로 남아있기 때문에, 엉뚱한 세션에 잘못된 로직이 돌 가능성이 있으므로 무조건 체크
		//	
		// 일반적으로 Dequeue 실패에 대한 예외 처리를 따로 분류해서 하지 않음	
		//		- PQCS 종료 신호에 따른 워커 스레드 종료 처리에 대한 예외
		//		- overlapped null, transferred 0, completion key 0
		//		- 이에 대한 예외가 상단에 들어갔다면 결국 overlapped null이면 워커 스레드 종료를 타게 됨
		//		- PQCS 종료 신호에 대한 예외처리로 overlapped가 null임을 일괄 처리하는게 일반적임 (나머지 멤버도 0으로 초기화를 하니까)
		//--------------------------------------------------------------
		if (sessionOlp == nullptr)
		{
			printf("overlapped null!!!!!!!!!!!!!!!!!!!!!!\n");
			return 0;
		}
		

		AcquireSRWLockExclusive(&g_SessionMapLock);
		const auto it = g_SessionMap.find(sessionId);
		if (it == g_SessionMap.end())
		{
			ReleaseSRWLockExclusive(&g_SessionMapLock);
			continue;
		}
		Session* session = it->second;
		SessionLock(session);
		ReleaseSRWLockExclusive(&g_SessionMapLock);
		
		// ------------------------------------------
		// [ transferred가 0이 되는 상황 ]
		// - RST로 인한 I/O 실패
		// - FIN으로 인한 I/O 성공
		// 
		// [ transferred가 0일 때 처리 ]
		// - 즉시 세션 삭제 불가
		// - 세션 종료 플래그를 활성화
		// ------------------------------------------
		if (transferred == 0)
		{
			cout << "transferred = 0 -> Session id : "<< session->sessionId << " - Disconnected On" << endl;
			cout << "recvQ가 다 찼거나 FIN을 수신했습니다. 혹은 연결이 끊겼습니다." << endl;
			InterlockedExchange8((char*)&session->bDisconnected, true);
			SessionUnlock(session);
			if (InterlockedDecrement((LONG*)&session->ioCount) == 0) 
				ReleaseSession(session);
			
			continue;
		}
		
		//---------------------------------------------------------
		// Recv 완료 처리
		//---------------------------------------------------------
		if (sessionOlp->type == ERecv)
		{
			session->recvQ->MoveRear(transferred);
			cout << "------------CompletionPort : Recv------------\n";
			cout << "Recv Complete / transferred : " << transferred << endl;

			while (1)
			{
				//-------------------------------------------
				// 완성된 메시지 추출
				//-------------------------------------------
				st_Header header;
				int peekLen = session->recvQ->Peek((char *)&header, sizeof(header));
				if (peekLen < sizeof(header))
					break;
				if (session->recvQ->GetUseSize() < sizeof(header) + header.payloadLen)
					break;
				session->recvQ->MoveFront(sizeof(header));
				

				//-------------------------------------------
				// payloadLen 검사
				// - 설계된 데이터 길이 이상 보냈다면 연결 끊기
				//-------------------------------------------
				if (header.payloadLen > MAX_ECHOBYTE)
				{
					InterlockedExchange8((char*)&session->bDisconnected, true);
					break;
				}

				//-------------------------------------------
				// 메시지 추출
				//-------------------------------------------
				char messageBuf[MAX_ECHOBYTE];
				int dequeueRet = session->recvQ->Dequeue(messageBuf, header.payloadLen);
				CPacket message;
				message.PutData((char*)&header, sizeof(header));
				int putDataRet = message.PutData(messageBuf, header.payloadLen);
				if (putDataRet == 0)
				{
					InterlockedExchange8((char*)&session->bDisconnected, true);
					break;
				}

				OnMessage(session->sessionId, &message);
			}


			// ------------------------------------------
			// 다시 Recv 걸기
			// ------------------------------------------
			/*SessionLock(session);
			bool recvPostRet = RecvPost(session);
			if (recvPostRet)
				SessionUnlock(session);*/

			RecvPost(session);
		}
		else if (sessionOlp->type == ESend)
		{
			//---------------------------------------------------------
			// Send 완료 처리
			//---------------------------------------------------------
			session->sendQ->MoveFront(transferred);
			cout << "------------CompletionPort : Send------------\n";
			cout << "Send Complete / transferred : " << transferred << endl; 
			InterlockedExchange(&session->isSending, false);

			//---------------------------------------------------------
			// Send 송신 중에 SendQ에 전송할 데이터가 쌓였다면 다시 Send 
			//---------------------------------------------------------
			//SessionLock(session);
			bool sendPostRet = SendPost(session);
			if (sendPostRet == false)
				continue;
				//SessionUnlock(session);
		}
		

		// ----------------------------------------------------------------
		// IOCount 감소 후, 세션 정리 시점 확인
		// ----------------------------------------------------------------
		if (InterlockedDecrement((LONG*)&session->ioCount) == 0)
		{
			SessionUnlock(session);
			ReleaseSession(session);
			continue;
		}

		SessionUnlock(session);
	}

	return 0;
}

unsigned int EchoThreadProc(void* arg)
{
	while (1)
	{
		if(g_JobQ.GetUseSize() == 0)
			WaitForSingleObject(g_JobEvent, INFINITE);

		// 동시 Dequeue가 안되도록 Lock (사실 EchoThread가 1개 이므로 락이 필요 없음)
		//AcquireSRWLockExclusive(&g_JogQLock);
		st_JobMessage message;
		message.sessionId = 0;
		message.echoData = 0;
		int dequeueRet = g_JobQ.Dequeue((char*)&message, sizeof(st_JobMessage));
		//ReleaseSRWLockExclusive(&g_JogQLock);
		
		if (dequeueRet == 0)
			continue;

		st_Header header;
		header.payloadLen = 8;
		CPacket packet;
		packet.PutData((char*)&header, sizeof(header));
		packet.PutData((char*)&message.echoData, sizeof(__int64));
		SendPacket(message.sessionId, &packet);
	}

	return 0;
}


void OnMessage(ULONG sessionId, CPacket* message)
{
	//------------------------------------------------------------
	// - 간단한 에코 서버이므로, 메시지를 헤더 타입으로 분류하여 메시지 종류별 처리하는 과정은 생략
	// - 현재 에코 더미 자체가 헤더 안에 메시지 타입을 기재하고 있지 않음
	//------------------------------------------------------------
	unsigned short payloadLen;
	__int64 echoData;
	*message >> payloadLen;
	*message >> echoData;
	

	//-------------------------------------------------------------
	// 에코(컨텐츠) 처리 스레드에게 작업 메시지를 생성 및 작업 큐에 인큐
	// - Enqueue 할 때 락 무조건 필요!
	//-------------------------------------------------------------
	st_JobMessage jobMsg;
	jobMsg.sessionId = sessionId;
	jobMsg.echoData = echoData;
	AcquireSRWLockExclusive(&g_JogQLock);
	int enqueueRet = g_JobQ.Enqueue((char*)&jobMsg, sizeof(jobMsg));
	ReleaseSRWLockExclusive(&g_JogQLock);
	if (enqueueRet == 0)
	{
		cout << "JobQ가 다 찼습니다.\n";
		exit(1);
	}
	SetEvent(g_JobEvent);
}

bool SendPacket(ULONG sessionId, CPacket* packet)
{
	//---------------------------------------------------------------------------
	// 세션 검색
	// - Map에 대한 락을 풀기 전에 세션 락 걸기
	// - 세션이 검색되면 락을 풀기 전까지 세션이 지워지지 않음을 보장할 수 있게된다.
	// 
	// [참고 사항]
	// + 세션이 도중에 삭제되면 어떤 문제가 발생하는가?
	//	1. session->lock이 0xdddd..로 밀렸다면 AcquireSRWLockExclusive에서 블로킹에 빠져버림 (실제로 나타나진 않았으나 별도의 테스트에서는 나타남)
	//	2. session->lock이 0x0000..로 밀렸다면 AcquireSRWLockExclusive가 성공됨
	//		- sendQ 멤버 함수 호출 시, this 콜에서 this가 0xffff...인 문제 발생 (실제로 나타난 결과)
	//	3. 이미 삭제된 세션을 대상으로 다시 에코 처리 스레드에서 SendPost->DisconnectSession에서 문제 발생
	//		- delete시 session 소멸자의 sendQ, recvQ의 소멸자에서 문제 발생
	//		- 혹은 소멸자가 아니더라도 free할 때 힙 invalidate 문제 발생
	//---------------------------------------------------------------------------
	AcquireSRWLockExclusive(&g_SessionMapLock);
	const auto it = g_SessionMap.find(sessionId);
	if (it == g_SessionMap.end())
	{
		ReleaseSRWLockExclusive(&g_SessionMapLock);
		return false;
	}
	Session* session = it->second;
	SessionLock(session);
	ReleaseSRWLockExclusive(&g_SessionMapLock);
	
	//-------------------------------------------
	// 세션 접근 및 사용
	// 
	// 송신 버퍼 공간이 모자랄 때 예외 처리
	// - 종료 플래그를 활성화 -> 세션 종료 유도한다.
	//-------------------------------------------
	int enqueueRet = session->sendQ->Enqueue(packet->GetBufferPtr(), packet->GetDataSize());
	if (enqueueRet == 0)
	{
		cout << "sendQ 공간이 모자랍니다." << endl;
		InterlockedExchange8((char*)&session->bDisconnected, true);
		SessionUnlock(session);
		return false;
	}

	int sendPostRet = SendPost(session);
	if (sendPostRet)
		SessionUnlock(session);

	return true;
}

bool RecvPost(Session* session)
{
	if (session->bDisconnected)
		return true;

	WSABUF wsaRecvBufArr[2];
	int wsaBufCnt = 1;
	int directEnqueueSize = session->recvQ->DirectEnqueueSize();
	int totalFreeSize = session->recvQ->GetFreeSize();
	if (totalFreeSize == 0 || session->bDisconnected)
		return true;
	wsaRecvBufArr[0].buf = session->recvQ->GetRearBufferPtr();
	wsaRecvBufArr[0].len = directEnqueueSize;
	if (directEnqueueSize < totalFreeSize)
	{
		int remainFreeSize = totalFreeSize - directEnqueueSize;
		wsaRecvBufArr[1].buf = session->recvQ->m_buf;
		wsaRecvBufArr[1].len = remainFreeSize;
		wsaBufCnt = 2;
	}

	InterlockedIncrement((LONG*)&session->ioCount);
	printf("------------AsyncRecv  session id : %d------------\n", session->sessionId);
	cout << "recvQ total Free Size : " << session->recvQ->GetFreeSize() << endl;
	DWORD recvBytes;
	DWORD flags = 0;
	int recvRet = WSARecv(session->sock, wsaRecvBufArr, wsaBufCnt, &recvBytes, &flags, (WSAOVERLAPPED*)&session->recvOlp, nullptr);

	// Fast I/O
	if (recvRet == 0)
	{
		cout << " RECV (FAST I/O) / recvBytes : " << recvBytes << '\n';
	}
	else if (recvRet == SOCKET_ERROR)
	{
		int error = WSAGetLastError();

		// Direct I/O
		if (error == WSA_IO_PENDING)
		{
			cout << " RECV IO PENDING\n";
		}
		else
		{
			if (InterlockedDecrement((LONG*)&session->ioCount) == 0)
			{
				SessionUnlock(session);
				ReleaseSession(session);
				return false;
			}
			cout << "recv error : " << error;
		}
	}
	return true;
}


bool SendPost(Session* session)
{
	if (InterlockedCompareExchange(&session->isSending, true, false) == true || session->bDisconnected)
		return true;
	printf("------------AsyncSend  session id : %d------------\n", session->sessionId);
	WSABUF wsaSendBufArr[2];
	int wsaBufCnt = 1;
	int directDequeueSize = session->sendQ->DirectDequeueSize();
	int totalUseSize = session->sendQ->GetUseSize();
	if (totalUseSize == 0)
	{
		InterlockedExchange(&session->isSending, false);
		return true;
	}
	wsaSendBufArr[0].buf = session->sendQ->GetFrontBufferPtr();
	wsaSendBufArr[0].len = directDequeueSize;
	if (directDequeueSize < totalUseSize)
	{
		int remainUseSize = totalUseSize - directDequeueSize;
		wsaSendBufArr[1].buf = session->sendQ->m_buf;
		wsaSendBufArr[1].len = remainUseSize;
		wsaBufCnt = 2;
	}
	InterlockedIncrement((LONG*)&session->ioCount);
	DWORD sendBytes;
	int sendRet = WSASend(session->sock, wsaSendBufArr, wsaBufCnt, &sendBytes, 0, (WSAOVERLAPPED*)&session->sendOlp, nullptr);
	//-------------------------------------
	// Fast I/O
	//-------------------------------------
	if (sendRet == 0)
	{
		cout << " Send (FAST I/O) / sendBytes : " << sendBytes << '\n';
	}
	else if (sendRet == SOCKET_ERROR)
	{
		int error = WSAGetLastError();

		//-------------------------------------
		// Direct I/O
		//-------------------------------------
		if (error == WSA_IO_PENDING)
		{
			cout << " Send IO PENDING\n";
		}
		else
		{
			cout << "send error : " << error << "\n";
			if (InterlockedDecrement((LONG*)&session->ioCount) == 0)
			{
				SessionUnlock(session);
				ReleaseSession(session);
				return false;
			}
		}
	}
	return true;
}

void ReleaseSession(Session* session)
{	
	//------------------------------------------------------------------------
	// 세션 맵에서 해당 세션 삭제
	// 
	// [ 예외 처리 ]
	// - 이미 세션이 맵에서 지워진 경우
	// - IOCP WorkerThread에서 ReleaseSession을 호출하여 다른 쪽의 세션 접근이 끝나기를 대기하고 있는 상황
	//	 - 이미 맵에서 해당 세션이 삭제되었으므로 현재 컨텐츠 스레드에서 호출한 ReleaseSession에서는 바로 return
	//		- 아래서 세션 접근이 끝나기를 대기하고 있는 스레드가 delete 하게 될 것
	//------------------------------------------------------------------------
	AcquireSRWLockExclusive(&g_SessionMapLock);
	int eraseRet = g_SessionMap.erase(session->sessionId);
	ReleaseSRWLockExclusive(&g_SessionMapLock);
	if (eraseRet == 0)
	{
		//SessionUnlock(session);
		return;
	}
	cout << "TryReleaseSession - id : " << session->sessionId << endl;

	// -------------------------------------------
	// 세션이 다른쪽에서 현재 사용 중이면
	// - 사용이 끝날 때 까지 기다렸다가 delete
	// -------------------------------------------
	AcquireSRWLockExclusive(&session->lock);
	ReleaseSRWLockExclusive(&session->lock);
	closesocket(session->sock);
	delete session;
	return;
}

void SessionLock(Session* session)
{
	InterlockedIncrement(&session->lockRef);
	AcquireSRWLockExclusive(&session->lock);
}

void SessionUnlock(Session* session)
{
	ReleaseSRWLockExclusive(&session->lock);
	InterlockedDecrement(&session->lockRef);
}
