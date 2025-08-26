#include <iostream>
#include "NetProc.h"
#include <Process.h>
#include <conio.h>

//#define PROFILE
//#include "Profiler.h"

using namespace std;

extern HANDLE hCp;
extern unordered_map<unsigned int, Session*> g_SessionMap;


class CTest2
{
public:
	CTest2()
	{
		
	}

	int a;
	int b;
	int c;
	int d;
};
class CTest
{
public:
	CTest()
	{
		InitializeSRWLock(&lock);
		count = 0;
		str = new char[5];
		t = new CTest2;
	}

	CTest2* t;
	char s[5000];
	char* str;
	SRWLOCK lock;
	LONG count;
};

void func(CTest* test)
{
	AcquireSRWLockExclusive(&test->lock);
	delete test;
	ReleaseSRWLockExclusive(&test->lock);
	InterlockedIncrement(&test->count);
}



int main()
{
	NetStartUp();

	//SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	//Session* session = new Session(sock, 1);
	//delete session;
	//delete session;
	//session->sendQ->GetFreeSize();
	//AcquireSRWLockExclusive(&session->lock);
	////ReleaseSRWLockExclusive(&session->lock);
	//cout << "main lock release!\n";
	//return 1;

	//----------------------------------------------
	// 스레드 생성
	//----------------------------------------------
	HANDLE acceptThread = (HANDLE)_beginthreadex(nullptr, 0, AcceptProc, nullptr, 0, nullptr);
	HANDLE worker1 = (HANDLE)_beginthreadex(nullptr, 0, WorkerThreadNetProc, nullptr, 0, nullptr);
	HANDLE worker2 = (HANDLE)_beginthreadex(nullptr, 0, WorkerThreadNetProc, nullptr, 0, nullptr);
	HANDLE worker3 = (HANDLE)_beginthreadex(nullptr, 0, WorkerThreadNetProc, nullptr, 0, nullptr);
	HANDLE worker4 = (HANDLE)_beginthreadex(nullptr, 0, WorkerThreadNetProc, nullptr, 0, nullptr);
	HANDLE worker5 = (HANDLE)_beginthreadex(nullptr, 0, WorkerThreadNetProc, nullptr, 0, nullptr);
	HANDLE echoWorker = (HANDLE)_beginthreadex(nullptr, 0, EchoThreadProc, nullptr, 0, nullptr);
	


	while (1)
	{
		if (_kbhit())
		{
			WCHAR ControlKey = _getwch();

			if (ControlKey == L's' || ControlKey == L'S')
			{
				
			}
		}
	}

}