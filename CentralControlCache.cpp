//���Ŀ��ƻ��棺�������£������ڴ�
#pragma once
#include"CentralControlCache.h"
#include"PageCache.h"

//���������������ʼ��
CentralControlCache CentralControlCache::_sInst;

size_t CentralControlCache::GetRangeMem(void*& start, void*& end, size_t expectedNum, size_t memSize)
{
	//����memSize��С���ڴ������SpanList������
	size_t index = SizeCalculation::Index(memSize);

	std::lock_guard<std::mutex> lock(_spanlists[index]._mtx);
	//��SpanList�л�ȡ�����ڴ���ڴ��Span
	Span* sp = GetOneSpan(_spanlists[index], memSize);

	// �ҵ�һ���ж����span���ж��ٸ�����
	size_t j = 1;
	start = sp->_list;
	void* cur = start;
	void* prev = start;
	while (j <= expectedNum && cur != nullptr)
	{
		prev = cur;
		cur = Next(cur);
		++j;
		//ʹ������+1������黹�ڴ�ʱ�ж��Ƿ�ȫ���黹
		sp->_usecount++;
	}

	sp->_list = cur;
	end = prev;
	Next(prev) = nullptr;
	//ʵ������Ķ������Ϊj-1��
	return j - 1;
}

//��SpanList�л�ȡ�����ڴ���ڴ��Span
Span* CentralControlCache::GetOneSpan(SpanList& list, size_t memSize)
{
	//����SpanList���ж��Ƿ����ڴ�
	Span* cur = list.Begin();
	while (cur != list.End())
	{
		if (cur->_list == nullptr)
			cur = cur->_next;
		else
			return cur;
	}

	//�ߵ�����֤��û�����ڴ��Span,��pagecache�����ڴ�
	Span* span = PageCache::GetInstance()->NewSpan(SizeCalculation::NumMovePage(memSize));

	//�����뵽��Span�и�ù���SpanList��
	//����ҳ�ż�����ʵ��ַ
	char* start = (char*)(span->_pageId << PAGE_SHIFT);
	//���ݿ�ʼ��ַ��ҳ���������һҳ�Ķ���ַ
	char* end = start + (span->_n << PAGE_SHIFT);
	while (start < end)
	{
		char* next = start + memSize;
		// ͷ��
		Next(start) = span->_list;
		span->_list = start;

		start = next;
	}
	//���õ�������Ĵ�С
	span->_objsize = memSize;
	//�����뵽��span���ӵ�spanlist��
	list.PushFront(span);
	return span;
}

void CentralControlCache::ReleaseListToSpans(void* start, size_t memSize)
{
	size_t index = SizeCalculation::Index(memSize);
	std::lock_guard<std::mutex> lock(_spanlists[index]._mtx);

	//��һ�������󻹸�_spanlist�ж�Ӧ��span
	while (start)
	{
		void* next = Next(start);

		// ��start�ڴ�������ĸ�span
		Span* span = PageCache::GetInstance()->GetSpanToMap(start);

		// �Ѷ�����뵽span�����list��
		Next(start) = span->_list;
		span->_list = start;
		span->_usecount--;
		// _usecount == 0˵�����span���г�ȥ�Ĵ���ڴ涼��������
		if (span->_usecount == 0)
		{
			//�����span����pagecache
			_spanlists[index].Erase(span);
			span->_list = nullptr;
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		}

		start = next;
	}
}
