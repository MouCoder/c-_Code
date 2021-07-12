#include "ThreadCache.h"
#include "CentralControlCache.h"

void* ThreadCache::GetMemoryFromCentralCache(size_t index, size_t memSize)
{
	// ��ȡһ����������ʹ����������ʽ
	// SizeClass::NumMoveSize(size)������ֵ,�����ֵ��maxsize��ȡ��Сֵ
	//������������ȡ�Ķ������
	size_t expectNum = min(SizeClass::NumMoveSize(memSize), _freeLists[index].MaxSize());

	// ȥ���Ļ����ȡbatch_num������
	void* start = nullptr;
	void* end = nullptr;
	//����᷵��ʵ�ʻ�ȡ�����ڴ����������Ӧ��Ϊ1��
	size_t actualNum = CentralControlCache::GetInstance()->FetchRangeObj(start, end, expectNum, SizeClass::RoundUp(memSize));
	assert(actualNum > 0);

	// >1������һ����ʣ�¹ҵ���������
	// ���һ����������ʣ�¹��������´�����Ͳ���Ҫ�����Ļ��棬���������������Ч��
	if (actualNum > 1)
	{
		_freeLists[index].PushRange(NextObj(start), end, actualNum - 1);
	}

	// ����������
	if (_freeLists[index].MaxSize() == expectNum)
	{
		_freeLists[index].SetMaxSize(_freeLists[index].MaxSize() + 1);
	}

	return start;
}

void* ThreadCache::Allocate(size_t memSize)
{
	//���������ڴ�Ĵ�С������freelist�е�����
	size_t index = SizeClass::Index(memSize);
	if (!_freeLists[index].Empty())
	{
		//��freelist�����ڴ棬ֱ�ӷ���һ���ڴ����
		return _freeLists[index].Pop();
	}
	else
	{
		//���freelist��û���ڴ�ֱ��ȥcentralcontrolcache����
		return GetMemoryFromCentralCache(index, memSize);
	}
}

void ThreadCache::Deallocate(void* ptr, size_t memSize)
{
	//ͨ�����������С������freelist�е�����
	size_t i = SizeClass::Index(memSize);
	_freeLists[i].Push(ptr);

	// ��list�е��ڴ�̫��ȥ�ͷŴ���central control cache��
	if (_freeLists[i].Size() >= _freeLists[i].MaxSize())
	{
		ListTooLong(_freeLists[i], memSize);
	}
}

// �ͷŶ���ʱ���������ʱ�������ڴ�ص����Ļ���
void ThreadCache::ListTooLong(FreeList& list, size_t memSize)
{
	size_t expectNum = list.MaxSize();
	void* start = nullptr;
	void* end = nullptr;
	//�������н���Щ�ڴ�ɾ��
	list.PopRange(start, end, expectNum);
	//����central control cache��
	CentralControlCache::GetInstance()->ReleaseListToSpans(start, memSize);
}