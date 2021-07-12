#pragma once
#include "Common.h"
class CentralControlCache
{
public:
	static CentralControlCache* GetInstance()
	{
		return &_sInst;
	}

	// �����Ļ����ȡһ�������Ķ����thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t memSize);

	// ��SpanList����page cache��ȡһ��span
	Span* GetOneSpan(SpanList& list, size_t memSize);

	// ��һ�������Ķ����ͷŵ�span���
	void ReleaseListToSpans(void* start, size_t memSize);
private:
	SpanList _spanLists[NFREELISTS]; // �����뷽ʽӳ��

private:
	//����ģʽ���
	CentralControlCache()
	{}
	//delete��ʾ���������ʵ��
	CentralControlCache(const CentralControlCache&) = delete;

	static CentralControlCache _sInst;
};

