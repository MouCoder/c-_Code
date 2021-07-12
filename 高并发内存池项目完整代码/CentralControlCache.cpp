#include "CentralControlCache.h"
#include "PageCache.h"

//������static�����������������ʼ��
CentralControlCache CentralControlCache::_sInst;

Span* CentralControlCache::GetOneSpan(SpanList& list, size_t memSize)
{
	// ����spanlist��ȥ�һ����ڴ��span
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_list)
		{
			return it;
		}

		it = it->_next;
	}

	// �ߵ����������span��û���ڴ��ˣ�ֻ����pagecache
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(memSize));
	// �зֺù���list��
	char* start = (char*)(span->_pageId << PAGE_SHIFT);
	char* end = start + (span->_n << PAGE_SHIFT);
	while (start < end)
	{
		char* next = start + memSize;
		// ͷ��
		NextObj(start) = span->_list;
		span->_list = start;

		start = next;
	}
	span->_objsize = memSize;

	list.PushFront(span);

	return span;
}

size_t CentralControlCache::FetchRangeObj(void*& start, void*& end, size_t n, size_t memSize)
{
	//�����ڴ�����С����������
	size_t i = SizeClass::Index(memSize);

	//Ͱ��
	std::lock_guard<std::mutex> lock(_spanLists[i]._mtx);

	Span* span = GetOneSpan(_spanLists[i], memSize);
	// �ҵ�һ���ж����span���ж��ٸ�����
	size_t j = 1;
	start = span->_list;
	void* cur = start;
	void* prev = start;
	while (j <= n && cur != nullptr)
	{
		prev = cur;
		cur = NextObj(cur);
		++j;
		span->_usecount++;
	}

	span->_list = cur;
	end = prev;
	NextObj(prev) = nullptr;
	//����ʵ�����뵽���ڴ����
	return j - 1;
}

void CentralControlCache::ReleaseListToSpans(void* start, size_t memSize)
{
	//�����ڴ�����С��������
	size_t i = SizeClass::Index(memSize);
	std::lock_guard<std::mutex> lock(_spanLists[i]._mtx);

	//��ÿһ���ڴ������뵽�Լ���Ӧ��span��
	while (start)
	{
		void* next = NextObj(start);

		// ��start�ڴ�������ĸ�span
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

		// �Ѷ�����뵽span�����list��
		NextObj(start) = span->_list;
		span->_list = start;
		span->_usecount--;
		// _usecount == 0˵�����span���г�ȥ�Ĵ���ڴ�
		// ���������ˣ��ڻ���page cache�ϲ��ɸ�����ڴ�
		if (span->_usecount == 0)
		{
			_spanLists[i].Erase(span);
			span->_list = nullptr;
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		}

		start = next;
	}
}
