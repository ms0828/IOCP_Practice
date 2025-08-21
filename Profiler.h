#pragma once

#include <Windows.h>
#include <iostream>
#include <winnt.h>

#ifdef PROFILE
#define PRO_BEGIN(TagName)	ProfileBegin(TagName)
#define PRO_END(TagName)	ProfileEnd(TagName)
#else
#define PRO_BEGIN(TagName)
#define PRO_END(TagName)
#endif


typedef struct PROFILE_SAMPLE
{
	bool			bStartFlag;			// 프로파일의 사용 여부. (배열시에만)
	bool			bInitFlag;		// 리셋이후, 처음 초기화인지 확인
	const char*		szName;			// 프로파일 샘플 이름.

	LARGE_INTEGER	lStartTick;			// 프로파일 샘플 실행 시간.

	__int64			iTotalTick;			// 전체 사용시간 카운터 Tick.	(출력시 호출회수로 나누어 평균 구함)
	__int64			iMin[2];			// 최소 사용시간 카운터 Tick.	(초단위로 계산하여 출력 / [0] 가장최소 [1] 다음 최소 [2])
	__int64			iMax[2];			// 최대 사용시간 카운터 Tick.	(초단위로 계산하여 출력 / [0] 가장최대 [1] 다음 최대 [2])

	__int64			iCall;				// 누적 호출 횟수.

}PROFILE_SAMPLE;


PROFILE_SAMPLE profile_sample[100];
int entryNum = 0;


int p_strcmp(const char* pstr1, const char* pstr2)
{
	if (!pstr1 || !pstr2)
		return -1;

	while ((*pstr1 || *pstr2) && (*pstr1 == *pstr2))
	{
		pstr1++;
		pstr2++;
	}
	return (unsigned int)*pstr1 - (unsigned int)*pstr2;
}

int findProfileEntry(const char* szName)
{
	for (int i = 0; i < entryNum; i++)
	{
		if (p_strcmp(profile_sample[i].szName, szName) == 0)
			return i;
	}

	return -1;
}


/////////////////////////////////////////////////////////////////////////////
// 하나의 함수 Profiling 시작, 끝 함수.
//
// Parameters: (char *)Profiling이름.
// Return: 없음.
/////////////////////////////////////////////////////////////////////////////
void ProfileBegin(const char* szName)
{
	int index = findProfileEntry(szName);

	// if index = -1	: 기존에 없는 태그에 대한 엔트리 등록
	// else				: 기존에 등록되어 있는 태그에 대해 업데이트
	if (index == -1)
	{
		profile_sample[entryNum].szName = szName;
		profile_sample[entryNum].iCall = 1;
		profile_sample[entryNum].bStartFlag = true;
		QueryPerformanceCounter(&profile_sample[entryNum].lStartTick);

		entryNum++;
		return;
	}
	else if(profile_sample[index].bStartFlag)
	{
		QueryPerformanceCounter(&profile_sample[index].lStartTick);
	}
	else
	{
		profile_sample[index].iCall++;
		QueryPerformanceCounter(&profile_sample[index].lStartTick);
		profile_sample[index].bStartFlag = true;
	}

}

void ProfileEnd(const char* szName)
{
	int index = findProfileEntry(szName);
	if (index == -1)
	{
		printf("ProfileBegin되지 않았던 ProfileEnd의 호출\n");
		return;
	}

	LARGE_INTEGER End;
	QueryPerformanceCounter(&End);
	long long consumedTick = End.QuadPart - profile_sample[index].lStartTick.QuadPart;



	if (profile_sample[index].bInitFlag == false)
	{
		profile_sample[index].iMax[0] = consumedTick;
		profile_sample[index].iMax[1] = consumedTick;
		profile_sample[index].iMin[0] = consumedTick;
		profile_sample[index].iMin[1] = consumedTick;
		profile_sample[index].bInitFlag = true;
	}

	// Max 업데이트
	if (profile_sample[index].iMax[0] < consumedTick)
	{
		profile_sample[index].iMax[1] = profile_sample[index].iMax[0];
		profile_sample[index].iMax[0] = consumedTick;
	}
	else if (profile_sample[index].iMax[1] < consumedTick)
	{
		profile_sample[index].iMax[1] = consumedTick;
	}

	// Min 업데이트
	if (profile_sample[index].iMin[0] > consumedTick)
	{
		profile_sample[index].iMin[1] = profile_sample[index].iMin[0];
		profile_sample[index].iMin[0] = consumedTick;
	}
	else if (profile_sample[index].iMin[1] > consumedTick)
	{
		profile_sample[index].iMin[1] = consumedTick;
	}

	// Total Tick 업데이트
	profile_sample[index].iTotalTick += consumedTick;
	profile_sample[index].bStartFlag = false;
}


/////////////////////////////////////////////////////////////////////////////
// Profiling 된 데이타를 Text 파일로 출력한다.
//
// Parameters: (char *)출력될 파일 이름.
// Return: 없음.
/////////////////////////////////////////////////////////////////////////////
void ProfileDataOutText(const char* szFileName)
{
	FILE* fp;
	errno_t ret;

	ret = fopen_s(&fp, szFileName, "wt");
	if (ret != 0)
	{
		printf("프로파일러 파일 생성에 실패하였습니다\n");
		return;
	}

	char buffer[150];
	sprintf_s(buffer, sizeof(buffer), "------------------------------------------------------------------------------------\n");
	fputs(buffer, fp);
	sprintf_s(buffer, sizeof(buffer), "        Name        |        Average        |           Min           |           Max           |           Call           |\n");
	fputs(buffer, fp);
	sprintf_s(buffer, sizeof(buffer), "------------------------------------------------------------------------------------\n");
	fputs(buffer, fp);

	for (int i = 0; i < entryNum; i++)
	{
		LARGE_INTEGER Freq;
		QueryPerformanceFrequency(&Freq);
		double consumedTotalTime_us = profile_sample[i].iTotalTick / (Freq.QuadPart / (double)1000000);
		double consumedMaxTime_us = profile_sample[i].iMax[1] / (Freq.QuadPart / (double)1000000);
		double consumedMinTime_us = profile_sample[i].iMin[1] / (Freq.QuadPart / (double)1000000);
		double averageTime_us;
		if (entryNum > 2)
			averageTime_us = (consumedTotalTime_us - profile_sample[i].iMax[0] - profile_sample[i].iMin[0]) / profile_sample[i].iCall;
		else
			averageTime_us = consumedTotalTime_us / profile_sample[i].iCall;
		
		sprintf_s(buffer, sizeof(buffer), "%20s | %17.4lf㎲ | %17.4lf㎲ | %17.4lf㎲ | %17d |\n", profile_sample[i].szName, averageTime_us, consumedMinTime_us, consumedMaxTime_us, profile_sample[i].iCall);
		fputs(buffer, fp);
	}

	fclose(fp);
}



/////////////////////////////////////////////////////////////////////////////
// 프로파일링 된 데이터를 모두 초기화 한다.
//
// Parameters: 없음.
// Return: 없음.
/////////////////////////////////////////////////////////////////////////////
void ProfileReset(void)
{
	for (int i = 0; i < entryNum; i++)
	{
		profile_sample[i].bInitFlag = false;
		profile_sample[i].iCall = 0;
		profile_sample[i].iMax[0] = 0;
		profile_sample[i].iMax[1] = 0;
		profile_sample[i].iMin[0] = 0;
		profile_sample[i].iMin[1] = 0;
		profile_sample[i].iTotalTick = 0;
	}
}