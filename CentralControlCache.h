#pragma once
#include"CommonResource.h"

class CentralControlCache
{
public:
	static CentralControlCache* GetInstance()
	{
		return &_sInst;
	};
	//��ȡ����ڴ����--��ʼ��ַ ������ַ �������� �ڴ�����С,����ʵ�ʻ�ȡ���ڴ�������
	size_t GetRangeMem(void*& start, void*& end, size_t expectedNum, size_t size);
	//��SpanList�л�ȡ�����ڴ���ڴ��Span
	Span* GetOneSpan(SpanList& list,size_t size);

	//�ͷ��ڴ浽central control cache��
	void ReleaseListToSpans(void* start,size_t memSize);
private:
	SpanList _spanlists[CENTRAL_SPANLIST_SIZE];
private:
	//�������
	CentralControlCache()
	{}

	CentralControlCache(const CentralControlCache&) = delete;
	//��Ϊ��static���͵ģ���˿��Բ�����ָ������
	static CentralControlCache _sInst;
};