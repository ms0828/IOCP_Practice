#include <stdlib.h>
#include <memory.h>
#include "CPacket.h"

CPacket::CPacket()
{
	buffer = (char*)malloc(sizeof(char) * DEFAULT_BUFSIZE);
	bufferMaxSize = DEFAULT_BUFSIZE;
	useSize = 0;
	readPtr = buffer;
	writePtr = buffer;
}

CPacket::CPacket(int iBufferSize)
{
	buffer = (char*)malloc(sizeof(char) * iBufferSize);
	bufferMaxSize = iBufferSize;
	useSize = 0;
	readPtr = buffer;
	writePtr = buffer;
}

CPacket::~CPacket()
{
	readPtr = nullptr;
	writePtr = nullptr;
	free(buffer);
}

void CPacket::Clear(void)
{
	writePtr = buffer;
	readPtr = buffer;
	useSize = 0;
}

int CPacket::MoveWritePos(int iSize)
{
	if (writePtr + iSize > buffer + bufferMaxSize)
		return 0;
	writePtr = writePtr + iSize;
	useSize += iSize;
	return iSize;
}

int CPacket::MoveReadPos(int iSize)
{
	if (readPtr + iSize > buffer + bufferMaxSize)
		return 0;
	readPtr = readPtr + iSize;
	useSize -= iSize;
	return iSize;
}

CPacket& CPacket::operator=(CPacket& clSrcPacket)
{
	int writePos = clSrcPacket.useSize;
	__int64 readPos = clSrcPacket.readPtr - clSrcPacket.buffer;
	memcpy_s(buffer, bufferMaxSize, clSrcPacket.buffer, clSrcPacket.useSize);
	bufferMaxSize = clSrcPacket.bufferMaxSize;
	useSize = clSrcPacket.useSize;
	readPtr = buffer + readPos;
	writePtr = buffer + writePos;
	return *this;
}

CPacket& CPacket::operator<<(unsigned char byValue)
{
	*writePtr = byValue;
	writePtr += sizeof(unsigned char);
	useSize += sizeof(unsigned char);
	return *this;
}

CPacket& CPacket::operator<<(char chValue)
{
	*writePtr = chValue;
	writePtr += sizeof(char);
	useSize += sizeof(char);
	return *this;
}

CPacket& CPacket::operator<<(unsigned short wValue)
{
	*(unsigned short*)writePtr = wValue;
	writePtr += sizeof(unsigned short);
	useSize += sizeof(unsigned short);
	return *this;
}

CPacket& CPacket::operator<<(short shValue)
{
	*(short *)writePtr = shValue;
	writePtr += sizeof(short);
	useSize += sizeof(short);
	return *this;
}


CPacket& CPacket::operator<<(unsigned int iValue)
{
	*(unsigned int*)writePtr = iValue;
	writePtr += sizeof(unsigned int);
	useSize += sizeof(unsigned int);
	return *this;
}
CPacket& CPacket::operator<<(int iValue)
{
	*(int*)writePtr = iValue;
	writePtr += sizeof(int);
	useSize += sizeof(int);
	return *this;
}

CPacket& CPacket::operator<<(unsigned long lValue)
{
	*(unsigned long*)writePtr = lValue;
	writePtr += sizeof(unsigned long);
	useSize += sizeof(unsigned long);
	return *this;
}

CPacket& CPacket::operator<<(long lValue)
{
	*(long*)writePtr = lValue;
	writePtr += sizeof(long);
	useSize += sizeof(long);
	return *this;
}

CPacket& CPacket::operator<<(float fValue)
{
	*(float*)writePtr = fValue;
	writePtr += sizeof(float);
	useSize += sizeof(float);
	return *this;
}

CPacket& CPacket::operator<<(double dValue)
{
	*(double*)writePtr = dValue;
	writePtr += sizeof(double);
	useSize += sizeof(double);
	return *this;
}

CPacket& CPacket::operator<<(unsigned __int64 iValue)
{
	*(unsigned __int64*)writePtr = iValue;
	writePtr += sizeof(unsigned __int64);
	useSize += sizeof(unsigned __int64);
	return *this;
}

CPacket& CPacket::operator<<(__int64 iValue)
{
	*(__int64*)writePtr = iValue;
	writePtr += sizeof(__int64);
	useSize += sizeof(__int64);
	return *this;
}


CPacket& CPacket::operator>>(unsigned char& byValue)
{
	byValue = *(unsigned char*)readPtr;
	readPtr += sizeof(unsigned char);
	useSize -= sizeof(unsigned char);
	return *this;
}

CPacket& CPacket::operator>>(char& chValue)
{
	chValue = *(char*)readPtr;
	readPtr += sizeof(char);
	useSize -= sizeof(char);
	return *this;
}

CPacket& CPacket::operator>>(short& shValue)
{
	shValue = *(short*)readPtr;
	readPtr += sizeof(short);
	useSize -= sizeof(short);
	return *this;
}

CPacket& CPacket::operator>>(unsigned short& wValue)
{
	wValue = *(unsigned short*)readPtr;
	readPtr += sizeof(unsigned short);
	useSize -= sizeof(unsigned short);
	return *this;
}

CPacket& CPacket::operator>>(unsigned int& iValue)
{
	iValue = *(unsigned int*)readPtr;
	readPtr += sizeof(unsigned int);
	useSize -= sizeof(unsigned int);
	return *this;
}

CPacket& CPacket::operator>>(int& iValue)
{
	iValue = *(int*)readPtr;
	readPtr += sizeof(int);
	useSize -= sizeof(int);
	return *this;
}

CPacket& CPacket::operator>>(unsigned long& dwValue)
{
	dwValue = *(unsigned long*)readPtr;
	readPtr += sizeof(unsigned long);
	useSize -= sizeof(unsigned long);
	return *this;
}
CPacket& CPacket::operator>>(long& dwValue)
{
	dwValue = *(long*)readPtr;
	readPtr += sizeof(long);
	useSize -= sizeof(long);
	return *this;
}


CPacket& CPacket::operator>>(float& fValue)
{
	fValue = *(float*)readPtr;
	readPtr += sizeof(float);
	useSize -= sizeof(float);
	return *this;
}
CPacket& CPacket::operator>>(double& dValue)
{
	dValue = *(double*)readPtr;
	readPtr += sizeof(double);
	useSize -= sizeof(double);
	return *this;
}


CPacket& CPacket::operator>>(unsigned __int64& iValue)
{
	iValue = *(unsigned __int64*)readPtr;
	readPtr += sizeof(unsigned __int64);
	useSize -= sizeof(unsigned __int64);
	return *this;
}

CPacket& CPacket::operator>>(__int64& iValue)
{
	iValue = *(__int64*)readPtr;
	readPtr += sizeof(__int64);
	useSize -= sizeof(__int64);
	return *this;
}


int CPacket::GetData(char* chpDest, int iSize)
{
	if (readPtr + iSize > buffer + useSize)
		return 0;
	memcpy_s(chpDest, iSize, readPtr, iSize);
	readPtr += iSize;
	useSize -= iSize;
	return iSize;
}

int CPacket::PutData(char* chpSrc, int iSrcSize)
{
	if (writePtr + iSrcSize > buffer + bufferMaxSize)
		return 0;

	memcpy_s(writePtr, buffer + bufferMaxSize - writePtr, chpSrc, iSrcSize);
	writePtr += iSrcSize;
	useSize += iSrcSize;
	return iSrcSize;
}
