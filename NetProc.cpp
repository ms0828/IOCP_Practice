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
	// Completion Port ����
	hCp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);


	// ���� ���� ���� �� ���ε� �� ����
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
	// I/O Pending �����ϱ� ���� TCP �۽� ���� ũ�⸦ 0���� ����
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
		// 1. ���� ���� �� ���� �� ����
		// 2. Completion Port�� �ش� ���� ���� ����
		//-----------------------------------------
		Session* newSession = new Session(clnSock, g_SessionIdCnt++);
		g_SessionMap.insert({ newSession->sessionId, newSession });
		CreateIoCompletionPort((HANDLE)newSession->sock, hCp, (ULONG_PTR)newSession, 0);
		
		// ---------------------------------------
		// �ʱ� Recv �ɱ�
		// ---------------------------------------
		RecvPost(newSession);
	}

	return 0;
}


unsigned int WorkerThreadNetProc(void* arg)
{
	while (1)
	{
		//--------------------- GQCS ���� ó��---------------------------
		// 1. GQCS�� false�� ��ȯ
		//		(1) CP �ڵ��� ���� ��� (�Ǵ� Ÿ�� �ƿ�) -> Dequeue ���� -> overlapped == null
		//		(2) ������ �ı��� ���(RST) overlapped != null, transferred = 0
		//			-> �׷��� I/O�� �����ߴٰ� ��� ó���� �� ����.
		//				- ��Ƽ ������ ȯ�濡���� I/O ���и� ���� �Ŀ� I/O ���� �Ϸ� ������ ó���� �� ����
		//				- �� �ڸ����� ������ �����Ѵٰų�.. ���� ó���� �Ұ�
		// 2. GQCS�� true�� ��ȯ
		//		- I/O ���� �� Dequeue ����
		//--------------------------------------------------------------
		DWORD transferred = 0;
		Session* session = nullptr;
		SessionOverlapped* sessionOlp;
		bool gqcsRet = GetQueuedCompletionStatus(hCp, &transferred, (PULONG_PTR)&session, (LPOVERLAPPED*)&sessionOlp, INFINITE);

		//--------------------------------------------------------------
		// lpOverlapped�� null������ ������ Ȯ�� �ʿ� 
		// - CP �ڵ��� ���� ��� (�Ǵ� dwMillisecond Ÿ�� �ƿ�) -> Dequeue ����
		// - �� �� completion Key�� transferred�� ���� �� �״�� �����ֱ� ������, ������ ���ǿ� �߸��� ������ �� ���ɼ��� �����Ƿ� ������ üũ
		//	
		// �Ϲ������� Dequeue ���п� ���� ���� ó���� ���� �з��ؼ� ���� ����	
		//		- PQCS ���� ��ȣ�� ���� ��Ŀ ������ ���� ó���� ���� ����
		//		- overlapped null, transferred 0, completion key 0
		//		- �̿� ���� ���ܰ� ��ܿ� ���ٸ� �ᱹ overlapped null�̸� ��Ŀ ������ ���Ḧ Ÿ�� ��
		//		- PQCS ���� ��ȣ�� ���� ����ó���� overlapped�� null���� �ϰ� ó���ϴ°� �Ϲ����� (������ ����� 0���� �ʱ�ȭ�� �ϴϱ�)
		//--------------------------------------------------------------
		if (sessionOlp == nullptr)
		{
			printf("overlapped null!!!!!!!!!!!!!!!!!!!!!!\n");
			return 0;
		}
		
		
		// ------------------------------------------
		// transferred�� 0�� �Ǵ� ��Ȳ
		// - RST�� ���� I/O ����
		// - FIN���� ���� I/O ����
		// 
		// [���� ó��]
		// - ��� ���� ���� �Ұ�
		// ------------------------------------------
		if (transferred == 0)
		{
			cout << "transferred = 0 -> Session id : "<< session->sessionId << " - Disconnected On" << endl;
			cout << "recvQ�� �� á�ų� FIN�� �����߽��ϴ�. Ȥ�� ������ ������ϴ�." << endl;

			if (InterlockedDecrement((LONG*)&session->ioCount) == 0) 
				TryDeleteSession(session);
			
			continue;
		}
			
		//---------------------------------------------------------
		// Recv �Ϸ� ó��
		//---------------------------------------------------------
		if (sessionOlp->type == ERecv)
		{
			session->recvQ->MoveRear(transferred);
			cout << "------------CompletionPort : Recv------------\n";
			cout << "Recv Complete / transferred : " << transferred << endl;
			

			// ------------------------------------------
			// ���� �����͸� SendQ�� Enqueue
			// ------------------------------------------
			char localBuf[40000];
			int dequeueRet = session->recvQ->Dequeue(localBuf, transferred);

			// ������ - �������� ����� �� ���ܰ� �Ͼ �� ����
			if (dequeueRet != transferred) 
			{
				cout << "RingBuffer Error : dequeuRet != transferred\n";
				exit(1);
			}

			int enqueueRet = session->sendQ->Enqueue(localBuf, transferred);
			printf("recvQ -> sendQ ���� ������ enqueue Size : %d\n", enqueueRet);
			if (enqueueRet == 0)
			{
				cout << " �۽� ���� ������ ���ڶ��ϴ�. sendQ Free size == " << session->sendQ->GetFreeSize() << endl;
				if (InterlockedDecrement((LONG*)&session->ioCount) == 0)
					TryDeleteSession(session);
				continue;
			}

			// ------------------------------------------
			// SendQ�� �ִ� �����͸� ����
			// ------------------------------------------
			SendPost(session);
			

			// ------------------------------------------
			// �ٽ� Recv �ɱ�
			// ------------------------------------------
			RecvPost(session);
		}
		else if (sessionOlp->type == ESend)
		{
			//---------------------------------------------------------
			// Send �Ϸ� ó��
			//---------------------------------------------------------
			session->sendQ->MoveFront(transferred);
			cout << "------------CompletionPort : Send------------\n";
			cout << "Send Complete / transferred : " << transferred << endl; 
			InterlockedExchange((LONG*)&session->bIsSending, false);
		}

		// ----------------------------------------------------------------
		// IOCount ���� ��, ���� ���� ���� Ȯ��
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

	// ���� ���� (�� �ʿ�)
	int eraseRet = g_SessionMap.erase(session->sessionId);
	
	delete session;
}
