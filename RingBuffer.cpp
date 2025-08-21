#include "RingBuffer.h"

void CRingBuffer::Resize(int size)
{
	char* temp = m_buf;
	m_buf = (char*)realloc(m_buf, size);
	m_maxBufSize = size;

	// 실패 시 -> 기존 블럭으로 재사용
	if (m_buf == nullptr)
		m_buf = temp;
}

int CRingBuffer::GetBufferSize(void)
{
	return m_maxBufSize;
}

int CRingBuffer::GetUseSize(void)
{
	int rear = m_rear;
	if (rear >= m_front)
		return rear - m_front;
	else
		return m_maxBufSize - m_front + rear;
}

int CRingBuffer::GetFreeSize(void)
{
	int front = m_front;
	if (front > m_rear)
		return front - m_rear - 1;
	else
	{
		if (front == 0)
			return m_maxBufSize - m_rear - 1;
		else
			return  m_maxBufSize - m_rear + front - 1;
	}
}

int CRingBuffer::Enqueue(const char* chpData, int iSize)
{
	if (iSize > GetFreeSize() || iSize == 0)
		return 0;

	int directEnqueueSize = DirectEnqueueSize();

	if (iSize <= directEnqueueSize)
	{
		memcpy_s(m_buf + m_rear, directEnqueueSize, chpData, iSize);
	}
	else
	{
		memcpy_s(m_buf + m_rear, directEnqueueSize, chpData, directEnqueueSize);
		int remainEnqueueSize = iSize - directEnqueueSize;
		memcpy_s(m_buf, remainEnqueueSize, chpData + directEnqueueSize, remainEnqueueSize);
	}
	
	MoveRear(iSize);
	return iSize;
}


int CRingBuffer::Dequeue(char* chpDest, int iSize)
{
	if (iSize > GetUseSize() || iSize == 0)
		return 0;
	
	int directDequeueSize = DirectDequeueSize();

	if (iSize <= directDequeueSize)
	{
		memcpy_s(chpDest, iSize, m_buf + m_front, iSize);
	}
	else
	{
		memcpy_s(chpDest, iSize, m_buf + m_front, directDequeueSize);
		int remainDequeueSize = iSize - directDequeueSize;
		memcpy_s(chpDest + directDequeueSize, remainDequeueSize, m_buf, remainDequeueSize);
	}
	
	MoveFront(iSize);
	return iSize;
}

int CRingBuffer::Peek(char* chpDest, int iSize)
{
	if (GetUseSize() < iSize)
		return 0;

	int totalCopySize = iSize;
	int directCopySize = DirectDequeueSize();

	if (totalCopySize <= directCopySize)
	{
		memcpy_s(chpDest, totalCopySize, m_buf + m_front, totalCopySize);
	}
	else
	{
		memcpy_s(chpDest, directCopySize, m_buf + m_front, directCopySize);
		int remainCopySize = totalCopySize - directCopySize;
		memcpy_s(chpDest + directCopySize, remainCopySize, m_buf, remainCopySize);
	}

	return totalCopySize;
}

void CRingBuffer::ClearBuffer(void)
{
	m_rear = 0;
	m_front = 0;
}

int CRingBuffer::DirectEnqueueSize(void)
{
	int front = m_front;
	if (front > m_rear)
		return front - m_rear - 1;
	else
	{
		if (front == 0)
			return m_maxBufSize - m_rear - 1;
		else
			return m_maxBufSize - m_rear;
	}
}

int CRingBuffer::DirectDequeueSize(void)
{
	int rear = m_rear;
	if (rear >= m_front)
		return rear - m_front;
	else
		return m_maxBufSize - m_front;
}

void CRingBuffer::MoveRear(int iSize)
{
	m_rear = (m_rear + iSize) % m_maxBufSize;
}

void CRingBuffer::MoveFront(int iSize)
{
	m_front = (m_front + iSize) % m_maxBufSize;
}

char* CRingBuffer::GetRearBufferPtr(void)
{
	return m_buf + m_rear;
}

char* CRingBuffer::GetFrontBufferPtr(void)
{
	return m_buf + m_front;
}
