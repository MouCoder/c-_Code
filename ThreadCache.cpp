#include"ThreadCache.h"

//��central control cache�л�ȡһ���ڴ�
void* ThreadCache::GetMemFromCentalCache(size_t index,size_t memSize)
{
	//central control cache���ڴ�����Ӧ��������freelist�ж�Ӧ������Ӧ������ͬ��ֱ���ɲ��������������ټ���

	//ʹ����������ȡһ���ڴ����
	size_t expectedNum = min(SizeCalculation::NumMoveSize(memSize), _freelist[index].MaxSize());//����������ڴ�������

	//ȥ���Ļ����л�ȡexpectedNum������
	void* start = nullptr;
	void* end = nullptr;
	//������ڴ��С��Ҫ���϶���
	size_t actualNum = CentralControlCache::GetInstance()->GetRangeMem(start, end, expectedNum, SizeCalculation::RoundUp(memSize));

	//����ʧ��
	assert(actualNum > 0);

	//��������뵽���ڴ����1��������ߵĹ����������ص�һ��
	if (actualNum > 1)
	{
		_freelist[index].PushRange(Next(start), end, actualNum - 1);
		//��start��next�ÿ�
		Next(start) = nullptr;
	}

	//�����������������ֵ+1���´�����ʱ���������ľͶ�һ������������Ѿ��������ֵ���Ͳ���������
	if (expectedNum == _freelist[index].MaxSize())
		_freelist[index].SetMaxSize(expectedNum+1);
	
	//����һ������
	return start;
}

//�����ڴ�
void* ThreadCache::Allocate(size_t memSize)
{
	assert(memSize <= THREAD_MAX_SIZE);
	//��������---�ڴ������freelist�����е�����
	size_t freelistIndex = SizeCalculation::Index(memSize);

	if (_freelist[freelistIndex].empty())
	{
		//��central control cache������
		return GetMemFromCentalCache(freelistIndex,memSize);
	}
	else
	{
		//ֱ�Ӵ�������ȡһ���ڴ���󷵻�
		return _freelist[freelistIndex].pop();
	}
}
//�ͷ��ڴ�
void ThreadCache::DeAllocate(void* mem, size_t memSize)
{
	//�����Ӧ������
	size_t index = SizeCalculation::Index(memSize);
	//��mem�ڴ�Push��_freelist��
	_freelist[index].push(mem);

	//�ж�_freelist�е��ڴ��Ƿ���Ҫ�黹��centtral control cache��
	if (_freelist[index].Size() >= _freelist[index].MaxSize())
	{
		ListTooLong(_freelist[index], memSize);
	}
}

// �ͷŶ���ʱ���������ʱ�������ڴ�ص����Ļ���
void ThreadCache::ListTooLong(FreeList& list, size_t memSize)
{
	size_t batchNum = list.MaxSize();
	void* start = nullptr;
	void* end = nullptr;
	//��ȡ��maxsize������(_freelist��ɾ��)
	list.PopRange(start, end, batchNum);
	//�ͷŵ�central control cache��
	CentralControlCache::GetInstance()->ReleaseListToSpans(start, memSize);
}
