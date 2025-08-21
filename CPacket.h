#pragma once

#define DEFAULT_BUFSIZE 1400

class CPacket
{
public:

	CPacket();
	CPacket(int iBufferSize);

	~CPacket();


	//////////////////////////////////////////////////////////////////////////
	// 버퍼 초기화
	// Parameters: 없음.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void Clear(void);


	//////////////////////////////////////////////////////////////////////////
	// 버퍼 사이즈 얻기
	//
	// Parameters: 없음.
	// Return: (int)패킷 버퍼 사이즈 얻기.
	//////////////////////////////////////////////////////////////////////////
	int	GetBufferSize(void) { return bufferMaxSize; }
	//////////////////////////////////////////////////////////////////////////
	// 현재 사용중인 사이즈 얻기.
	//
	// Parameters: 없음.
	// Return: (int)사용중인 데이타 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int	GetDataSize(void) { return useSize; }



	//////////////////////////////////////////////////////////////////////////
	// 버퍼 포인터 얻기.
	//
	// Parameters: 없음.
	// Return: (char *)버퍼 포인터.
	//////////////////////////////////////////////////////////////////////////
	char* GetBufferPtr(void) { return buffer; }


	//////////////////////////////////////////////////////////////////////////
	// 버퍼 Pos 이동. (음수이동은 안됨)
	// GetBufferPtr 함수를 이용하여 외부에서 강제로 버퍼 내용을 수정할 경우 사용. 
	//
	// Parameters: (int) 이동 사이즈.
	// Return: (int) 이동된 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int	MoveWritePos(int iSize);
	int	MoveReadPos(int iSize);




	/* ============================================================================= */
	// 연산자 오버로딩
	/* ============================================================================= */
	CPacket& operator = (CPacket& clSrcPacket);

	//////////////////////////////////////////////////////////////////////////
	// 넣기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////
	CPacket& operator << (unsigned char byValue);
	CPacket& operator << (char chValue);

	CPacket& operator << (unsigned short wValue);
	CPacket& operator << (short shValue);
	
	CPacket& operator << (unsigned int iValue);
	CPacket& operator << (int iValue);

	CPacket& operator << (unsigned long lValue);
	CPacket& operator << (long lValue);

	CPacket& operator << (float fValue);
	CPacket& operator << (double dValue);

	CPacket& operator << (unsigned __int64 iValue);
	CPacket& operator << (__int64 iValue);





	//////////////////////////////////////////////////////////////////////////
	// 빼기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////
	CPacket& operator >> (unsigned char& byValue);
	CPacket& operator >> (char& chValue);

	CPacket& operator >> (unsigned short& wValue);
	CPacket& operator >> (short& shValue);
	
	CPacket& operator >> (unsigned int& iValue);
	CPacket& operator >> (int& iValue);

	CPacket& operator >> (unsigned long& dwValue);
	CPacket& operator >> (long& dwValue);

	CPacket& operator >> (float& fValue);
	CPacket& operator >> (double& dValue);

	CPacket& operator >> (unsigned __int64& iValue);
	CPacket& operator >> (__int64& iValue);
	



	//////////////////////////////////////////////////////////////////////////
	// 데이타 얻기.
	//
	// Parameters: (char *)Dest 포인터. (int)Size.
	// Return: (int)복사한 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int	GetData(char* chpDest, int iSize);

	//////////////////////////////////////////////////////////////////////////
	// 데이타 삽입.
	//
	// Parameters: (char *)Src 포인터. (int)SrcSize.
	// Return: (int)복사한 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int	PutData(char* chpSrc, int iSrcSize);




protected:
	char* buffer;
	char* readPtr;
	char* writePtr;
	
	int	bufferMaxSize;
	int	useSize;
};

