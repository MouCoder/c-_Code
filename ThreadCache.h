//�̻߳��棺ÿ���̶߳��У�С��64k���ڴ�ֱ����thread cache����
#include"CommonResource.h"
#include"CentralControlCache.h"

class ThreadCache
{
public:
	//������ͷ��ڴ�
	void* Allocate(size_t memSize);
	void DeAllocate(void* mem,size_t memSize);

	//�����Ļ����л�ȡ����ڴ����
	void* GetMemFromCentalCache(size_t index, size_t memSize);

	// �ͷŶ���ʱ���������ʱ�������ڴ�ص����Ļ���
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freelist[THREAD_FREELIST_SIZE];
};

static __declspec(thread) ThreadCache* tls_threadcache = nullptr;