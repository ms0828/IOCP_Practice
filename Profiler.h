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
	bool			bStartFlag;			// ���������� ��� ����. (�迭�ÿ���)
	bool			bInitFlag;		// ��������, ó�� �ʱ�ȭ���� Ȯ��
	const char*		szName;			// �������� ���� �̸�.

	LARGE_INTEGER	lStartTick;			// �������� ���� ���� �ð�.

	__int64			iTotalTick;			// ��ü ���ð� ī���� Tick.	(��½� ȣ��ȸ���� ������ ��� ����)
	__int64			iMin[2];			// �ּ� ���ð� ī���� Tick.	(�ʴ����� ����Ͽ� ��� / [0] �����ּ� [1] ���� �ּ� [2])
	__int64			iMax[2];			// �ִ� ���ð� ī���� Tick.	(�ʴ����� ����Ͽ� ��� / [0] �����ִ� [1] ���� �ִ� [2])

	__int64			iCall;				// ���� ȣ�� Ƚ��.

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
// �ϳ��� �Լ� Profiling ����, �� �Լ�.
//
// Parameters: (char *)Profiling�̸�.
// Return: ����.
/////////////////////////////////////////////////////////////////////////////
void ProfileBegin(const char* szName)
{
	int index = findProfileEntry(szName);

	// if index = -1	: ������ ���� �±׿� ���� ��Ʈ�� ���
	// else				: ������ ��ϵǾ� �ִ� �±׿� ���� ������Ʈ
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
		printf("ProfileBegin���� �ʾҴ� ProfileEnd�� ȣ��\n");
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

	// Max ������Ʈ
	if (profile_sample[index].iMax[0] < consumedTick)
	{
		profile_sample[index].iMax[1] = profile_sample[index].iMax[0];
		profile_sample[index].iMax[0] = consumedTick;
	}
	else if (profile_sample[index].iMax[1] < consumedTick)
	{
		profile_sample[index].iMax[1] = consumedTick;
	}

	// Min ������Ʈ
	if (profile_sample[index].iMin[0] > consumedTick)
	{
		profile_sample[index].iMin[1] = profile_sample[index].iMin[0];
		profile_sample[index].iMin[0] = consumedTick;
	}
	else if (profile_sample[index].iMin[1] > consumedTick)
	{
		profile_sample[index].iMin[1] = consumedTick;
	}

	// Total Tick ������Ʈ
	profile_sample[index].iTotalTick += consumedTick;
	profile_sample[index].bStartFlag = false;
}


/////////////////////////////////////////////////////////////////////////////
// Profiling �� ����Ÿ�� Text ���Ϸ� ����Ѵ�.
//
// Parameters: (char *)��µ� ���� �̸�.
// Return: ����.
/////////////////////////////////////////////////////////////////////////////
void ProfileDataOutText(const char* szFileName)
{
	FILE* fp;
	errno_t ret;

	ret = fopen_s(&fp, szFileName, "wt");
	if (ret != 0)
	{
		printf("�������Ϸ� ���� ������ �����Ͽ����ϴ�\n");
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
		
		sprintf_s(buffer, sizeof(buffer), "%20s | %17.4lf�� | %17.4lf�� | %17.4lf�� | %17d |\n", profile_sample[i].szName, averageTime_us, consumedMinTime_us, consumedMaxTime_us, profile_sample[i].iCall);
		fputs(buffer, fp);
	}

	fclose(fp);
}



/////////////////////////////////////////////////////////////////////////////
// �������ϸ� �� �����͸� ��� �ʱ�ȭ �Ѵ�.
//
// Parameters: ����.
// Return: ����.
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