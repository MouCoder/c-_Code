#include "PageCache.h"

PageCache PageCache::_sInst;

// ��ϵͳ����kҳ�ڴ�
void* PageCache::SystemAllocPage(size_t k)
{
	return ::SystemAlloc(k);
}


Span* PageCache::NewSpan(size_t k)
{
	//����
	std::lock_guard<std::recursive_mutex> lock(_mtx);

	// ���ֱ���������NPAGES�Ĵ���ڴ棬ֱ����ϵͳҪ
	if (k >= NPAGES)
	{
		void* ptr = SystemAllocPage(k);
		Span* span = new Span;
		span->_pageId = (ADDRES_INT)ptr >> PAGE_SHIFT;
		span->_n = k;

		_idSpanMap[span->_pageId] = span;
		return span;
	}

	//�����ֱ�ӷ���
	if (!_spanList[k].Empty())
	{
		return _spanList[k].PopFront();
	}

	//���������һ����Ľ����з�
	for (size_t i = k + 1; i < NPAGES; ++i)
	{
		// ��ҳ����С,�г�kҳ��span����
		// �г�i-kҳ�һ���������
		if (!_spanList[i].Empty())
		{
			// β�г�һ��kҳspan
			Span* span = _spanList[i].PopFront();

			Span* split = new Span;
			split->_pageId = span->_pageId + span->_n - k;
			split->_n = k;

			// �ı��г���span��ҳ�ź�span��ӳ���ϵ
			for (PageID i = 0; i < k; ++i)
			{
				_idSpanMap[split->_pageId + i] = split;
			}

			span->_n -= k;
			//��spanʣ�µ�ҳ���ڶ�Ӧ��λ��
			_spanList[span->_n].PushFront(span);
			return split;
		}
	}
	//spanlist��û���ڴ棬��ϵͳ����һ��128ҳ���ڴ�����и�
	Span* bigSpan = new Span;
	void* memory = SystemAllocPage(NPAGES - 1);
	//�����ҳ��
	bigSpan->_pageId = (size_t)memory >> 12;
	bigSpan->_n = NPAGES - 1;
	// ��ҳ�ź�spanӳ���ϵ����
	for (PageID i = 0; i < bigSpan->_n; ++i)
	{
		PageID id = bigSpan->_pageId + i;
		_idSpanMap[id] = bigSpan;
	}

	_spanList[NPAGES - 1].Insert(_spanList[NPAGES - 1].Begin(), bigSpan);

	return NewSpan(k);
}

Span* PageCache::MapObjectToSpan(void* obj)
{
	//std::lock_guard<std::recursive_mutex> lock(_mtx);
	//ͨ���ڴ��ַ��map���ҵ���Ӧ��span
	PageID id = (ADDRES_INT)obj >> PAGE_SHIFT;

	Span* span = _idSpanMap.get(id);
	if (span != nullptr)
	{
		return span;
	}
	else
	{
		assert(false);
		return nullptr;
	}
}

void PageCache::ReleaseSpanToPageCache(Span* span)
{
	//����128ҳ���ڴ棬ֱ�ӻ���ϵͳ
	if (span->_n >= NPAGES)
	{
		_idSpanMap.erase(span->_pageId);
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr);
		delete span;
		return;
	}

	std::lock_guard<std::recursive_mutex> lock(_mtx);

	// ���ǰ�����spanҳ�����кϲ�,����ڴ���Ƭ����

	// ��ǰ�ϲ�
	while (1)
	{
		PageID preId = span->_pageId - 1;
		Span* preSpan = _idSpanMap.get(preId);
		if (preSpan == nullptr)
		{
			break;
		}

		// ���ǰһ��ҳ��span����ʹ���У�������ǰ�ϲ�
		if (preSpan->_usecount != 0)
		{
			break;
		}

		// ��ʼ�ϲ�...

		// ����128ҳ������Ҫ�ϲ���
		if (preSpan->_n + span->_n >= NPAGES)
		{
			break;
		}

		// �Ӷ�Ӧ��span�����н��������ٺϲ�
		_spanList[preSpan->_n].Erase(preSpan);

		span->_pageId = preSpan->_pageId;
		span->_n += preSpan->_n;

		// ����ҳ֮��ӳ���ϵ
		for (PageID i = 0; i < preSpan->_n; ++i)
		{
			_idSpanMap[preSpan->_pageId + i] = span;
		}

		delete preSpan;
	}

	// ���ϲ�
	while (1)
	{
		//�õ���һ��ҳ
		PageID nextId = span->_pageId + span->_n;

		//ͨ��ҳ����map�в��Һ�һ��ҳ��Ӧ��span
		Span* nextSpan = _idSpanMap.get(nextId);
		if (nextSpan == nullptr)
		{
			break;
		}

		//û��ʹ���꣬���ܺϲ�
		if (nextSpan->_usecount != 0)
		{
			break;
		}

		// ����128ҳ������Ҫ�ϲ���
		if (nextSpan->_n + span->_n >= NPAGES)
		{
			break;
		}

		//ɾ��ԭ��ҳ��ӳ���ϵ
		_spanList[nextSpan->_n].Erase(nextSpan);

		span->_n += nextSpan->_n;

		//�Ժϲ���ɺ��ҳ���½���ӳ���ϵ
		for (PageID i = 0; i < nextSpan->_n; ++i)
		{
			_idSpanMap[nextSpan->_pageId + i] = span;
		}

		delete nextSpan;
	}

	// �ϲ����Ĵ�span�����뵽��Ӧ��������
	_spanList[span->_n].PushFront(span);
}