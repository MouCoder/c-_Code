#pragma once

#include "Common.h"

class ThreadCache
{
public:
	// ������ͷ��ڴ����
	void* Allocate(size_t memSize);
	void Deallocate(void* ptr, size_t memSize);

	// �����Ļ����ȡ����
	void* GetMemoryFromCentralCache(size_t index, size_t memSize);

	// �ͷŶ���ʱ���������ʱ�������ڴ�ص����Ļ���
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freeLists[NFREELISTS];
};

static __declspec(thread) ThreadCache* tls_threadcache = nullptr;
