#include "NetProc.h"
#include <iostream>
#include <unordered_map>

#define PROFILE
#include "Profiler.h"

using namespace std;

SOCKET listenSock;
HANDLE hCp;

unsigned int g_SessionIdCnt = 0;

unordered_map<unsigned int, Session*> g_SessionMap;


void NetStartUp()
{
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
		g_SessionMap.insert({ newSession->sessionId, newSession });
		CreateIoCompletionPort((HANDLE)newSession->sock, hCp, (ULONG_PTR)newSession, 0);
		
		// ---------------------------------------
		// 초기 Recv 걸기
		// ---------------------------------------
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
		Session* session = nullptr;
		SessionOverlapped* sessionOlp;
		bool gqcsRet = GetQueuedCompletionStatus(hCp, &transferred, (PULONG_PTR)&session, (LPOVERLAPPED*)&sessionOlp, INFINITE);

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
		
		
		// ------------------------------------------
		// transferred가 0이 되는 상황
		// - RST로 인한 I/O 실패
		// - FIN으로 인한 I/O 성공
		// 
		// [예외 처리]
		// - 즉시 세션 삭제 불가
		// ------------------------------------------
		if (transferred == 0)
		{
			cout << "transferred = 0 -> Session id : "<< session->sessionId << " - Disconnected On" << endl;
			cout << "recvQ가 다 찼거나 FIN을 수신했습니다. 혹은 연결이 끊겼습니다." << endl;

			if (InterlockedDecrement((LONG*)&session->ioCount) == 0) 
				TryDeleteSession(session);
			
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
			

			// ------------------------------------------
			// 받은 데이터를 SendQ에 Enqueue
			// ------------------------------------------
			char localBuf[40000];
			int dequeueRet = session->recvQ->Dequeue(localBuf, transferred);

			// 디버깅용 - 정상적인 경우라면 이 예외가 일어날 수 없음
			if (dequeueRet != transferred) 
			{
				cout << "RingBuffer Error : dequeuRet != transferred\n";
				exit(1);
			}

			int enqueueRet = session->sendQ->Enqueue(localBuf, transferred);
			printf("recvQ -> sendQ 에코 데이터 enqueue Size : %d\n", enqueueRet);
			if (enqueueRet == 0)
			{
				cout << " 송신 버퍼 공간이 모자랍니다. sendQ Free size == " << session->sendQ->GetFreeSize() << endl;
				if (InterlockedDecrement((LONG*)&session->ioCount) == 0)
					TryDeleteSession(session);
				continue;
			}

			// ------------------------------------------
			// SendQ에 있는 데이터를 전송
			// ------------------------------------------
			SendPost(session);
			

			// ------------------------------------------
			// 다시 Recv 걸기
			// ------------------------------------------
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
			InterlockedExchange((LONG*)&session->bIsSending, false);
		}

		// ----------------------------------------------------------------
		// IOCount 감소 후, 세션 정리 시점 확인
		// ----------------------------------------------------------------
		if (InterlockedDecrement((LONG*)&session->ioCount) == 0)
		{
			TryDeleteSession(session);
			continue;
		}
	}

	return 0;
}

void RecvPost(Session* session)
{
	InterlockedIncrement((LONG*)&session->ioCount);
	printf("------------AsyncRecv  session id : %d------------\n", session->sessionId);
	
	WSABUF wsaRecvBufArr[2];
	int wsaBufCnt = 1;
	int directEnqueueSize = session->recvQ->DirectEnqueueSize();
	int totalFreeSize = session->recvQ->GetFreeSize();
	wsaRecvBufArr[0].buf = session->recvQ->GetRearBufferPtr();
	wsaRecvBufArr[0].len = directEnqueueSize;
	if (directEnqueueSize < totalFreeSize)
	{
		int remainFreeSize = totalFreeSize - directEnqueueSize;
		wsaRecvBufArr[1].buf = session->recvQ->m_buf;
		wsaRecvBufArr[1].len = remainFreeSize;
		wsaBufCnt = 2;
	}


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
				TryDeleteSession(session);
			}
			cout << "recv error : " << error;
		}
	}
}

void SendPost(Session* session)
{
	if (InterlockedCompareExchange((LONG*)&session->bIsSending, true, false) == true)
		return;
	InterlockedIncrement((LONG*)&session->ioCount);

	printf("------------AsyncSend  session id : %d------------\n", session->sessionId);
	WSABUF wsaSendBufArr[2];
	int wsaBufCnt = 1;
	int directDequeueSize = session->sendQ->DirectDequeueSize();
	int totalUseSize = session->sendQ->GetUseSize();
	wsaSendBufArr[0].buf = session->sendQ->GetFrontBufferPtr();
	wsaSendBufArr[0].len = directDequeueSize;
	if (directDequeueSize < totalUseSize)
	{
		int remainUseSize = totalUseSize - directDequeueSize;
		wsaSendBufArr[1].buf = session->sendQ->m_buf;
		wsaSendBufArr[1].len = remainUseSize;
		wsaBufCnt = 2;
	}

	cout << "SendQ total Use Size : " << session->sendQ->GetUseSize() << endl;
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
			if (InterlockedDecrement((LONG*)&session->ioCount) == 0)
			{
				TryDeleteSession(session);
			}
			cout << "send error : " << error << "\n";
		}
	}
	return;
}

void TryDeleteSession(Session* session)
{
	cout << "TryDeleteSession - id : " << session->sessionId << endl;

	closesocket(session->sock);

	// 세션 삭제 (락 필요)
	int eraseRet = g_SessionMap.erase(session->sessionId);
	
	delete session;
}
