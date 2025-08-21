#include <iostream>
#include "NetProc.h"
#include <Process.h>
#include <conio.h>

//#define PROFILE
//#include "Profiler.h"

using namespace std;

extern HANDLE hCp;

int main()
{

	NetStartUp();

	//----------------------------------------------
	// 스레드 생성
	//----------------------------------------------
	HANDLE acceptThread = (HANDLE)_beginthreadex(nullptr, 0, AcceptProc, nullptr, 0, nullptr);
	HANDLE worker1 = (HANDLE)_beginthreadex(nullptr, 0, WorkerThreadNetProc, nullptr, 0, nullptr);
	HANDLE worker2 = (HANDLE)_beginthreadex(nullptr, 0, WorkerThreadNetProc, nullptr, 0, nullptr);
	HANDLE worker3 = (HANDLE)_beginthreadex(nullptr, 0, WorkerThreadNetProc, nullptr, 0, nullptr);
	HANDLE worker4 = (HANDLE)_beginthreadex(nullptr, 0, WorkerThreadNetProc, nullptr, 0, nullptr);
	HANDLE worker5 = (HANDLE)_beginthreadex(nullptr, 0, WorkerThreadNetProc, nullptr, 0, nullptr);
	HANDLE worker6 = (HANDLE)_beginthreadex(nullptr, 0, WorkerThreadNetProc, nullptr, 0, nullptr);
	HANDLE worker7 = (HANDLE)_beginthreadex(nullptr, 0, WorkerThreadNetProc, nullptr, 0, nullptr);
	


	while (1)
	{
		if (_kbhit())
		{
			WCHAR ControlKey = _getwch();

			if (ControlKey == L's' || ControlKey == L'S')
			{
				//ProfileDataOutText("WSASend_8192btes.txt");
				//cout << "저장완료\n";

				CloseHandle(hCp);
			}
		}
	}

}