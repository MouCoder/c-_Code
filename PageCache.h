#pragma once
#include"CommonResource.h"
#include"PageMap.h"

class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}
	//����numҳ���ڴ�
	Span* NewSpan(size_t num);

	//ֱ����ϵͳ����numҳ�ڴ�
	void* SystemAllocPage(size_t num);
	//ͨ���ڴ��ַ��map�в��Ҷ�Ӧ��span
	Span* GetSpanToMap(void* mem);
	// �ͷſ���span�ص�Pagecache�����ϲ����ڵ�span
	void ReleaseSpanToPageCache(Span* span);
	std::recursive_mutex _mtx;
private:
	TCMalloc_PageMap2<32 - PAGE_SHIFT> _idSpanMap;
	SpanList _spanList[MAX_PAGE];
private:
	//����ģʽ
	PageCache()
	{}

	PageCache(const PageCache&) = delete;//delete����˼�ǲ������ڱ�ĵط�����

	// ����
	static PageCache _sInst;
};