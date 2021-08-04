#include"PageCache.h"

//���������������ʼ��
PageCache PageCache::_sInst;

// ��ϵͳ����kҳ�ڴ�
void* PageCache::SystemAllocPage(size_t k)
{
	return ::SystemAlloc(k);
}


//����numҳ���ڴ�
Span* PageCache::NewSpan(size_t num)
{
	std::lock_guard<std::recursive_mutex> lock(_mtx);
	//���ֱ���������128ҳ���ڴ棬ֱ����ϵͳҪ
	if (num >= 128)
	{
		//��ϵͳ����
		void* ptr = SystemAllocPage(num);
		Span* span = new Span;
		span->_pageId = (ADDRES_INT)ptr >> PAGE_SHIFT;
		span->_n = num;

		{
			//std::lock_guard<std::mutex> lock(_map_mtx);
			_idSpanMap[span->_pageId] = span;
		}

		return span;
	}
	else
	{
		if (!_spanList[num - 1].isEmpty())
			_spanList[num - 1].isEmpty();
		//�ߵ�����˵��û���ڴ棬��Ҫ����ж��Ƿ���numҳ���ڴ�

		//�����
		for (int i = num; i < 128; i++)
		{
			if (!_spanList[i].isEmpty())
			{
				//��numҳ��_spanList[num - 1]
				Span* span = _spanList[i].PopFront();
					
				//�г������ڴ�---����β��
				Span* split = new Span;
				split->_n = num;
				split->_next = nullptr;
				//span���ж�ҳ�����洢��ҳ���ǵ�һҳ��ҳ�ţ���������ҳ��Ӧ�����г�����ҳ��+��ʼҳ��
				split->_pageId = span->_pageId+span->_n-num;

				// �ı��г���span��ҳ�ź�span��ӳ���ϵ
				{
					//std::lock_guard<std::mutex> lock(_map_mtx);
					for (PageID i = 0; i < num; ++i)
					{
						_idSpanMap[split->_pageId + i] = split;
					}
				}

				//��Spanʣ���ҳ���ڶ�Ӧ��λ��
				span->_n -= num;
				_spanList[span->_n - 1].PushFront(span);

				return split;
			}
		}
		
		//�ߵ�����˵��page cache��û���ڴ�,��ϵͳ����
		Span* bigSpan = new Span;
		void* memory = SystemAllocPage(MAX_PAGE);
		//�����ҳ��
		bigSpan->_pageId = (size_t)memory >> 12;
		bigSpan->_n = MAX_PAGE;
		// ��ҳ�ź�spanӳ���ϵ������Ҳ������ô���ҳ��ӳ�������span��
		{
			//std::lock_guard<std::mutex> lock(_map_mtx);
			for (PageID i = 0; i < bigSpan->_n; ++i)
			{
				PageID id = bigSpan->_pageId + i;
				_idSpanMap[id] = bigSpan;
			}
		}
		//���ڴ����ӵ�spanlist��
		_spanList[MAX_PAGE - 1].Insert(_spanList[MAX_PAGE - 1].Begin(), bigSpan);
		//�����и�
		return NewSpan(num);
	}
}

//ͨ���ڴ��ַ��map�в��Ҷ�Ӧ��span
Span* PageCache::GetSpanToMap(void* mem)
{
	//�����ڴ����ҳ��
	PageID id = (ADDRES_INT)mem >> PAGE_SHIFT;
	//��map�в���
	auto ret = _idSpanMap.get(id);
	if (ret != nullptr)
	{
		return ret;
	}
	else
	{
		assert(false);
		return  nullptr;
	}
}

// �ͷſ���span�ص�Pagecache�����ϲ����ڵ�span
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	if (span->_n >= MAX_PAGE)
	{
		//����128ҳ���ڴ�ֱ�ӻ���ϵͳ�ڴ�
		_idSpanMap.erase(span->_pageId);
		//����ҳ�ż����ַ
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
		if (preSpan->_n + span->_n >= MAX_PAGE)
		{
			break;
		}

		// �Ӷ�Ӧ��span�����н��������ٺϲ�
		_spanList[preSpan->_n].Erase(preSpan);

		span->_pageId = preSpan->_pageId;
		span->_n += preSpan->_n;

		// ����ҳ֮��ӳ���ϵ
		{
			//std::lock_guard<std::mutex> lock(_map_mtx);
			for (PageID i = 0; i < preSpan->_n; ++i)
			{
				_idSpanMap[preSpan->_pageId + i] = span;
			}
		}

		delete preSpan;
	}

	// ���ϲ�
	while (1)
	{
		PageID nextId = span->_pageId + span->_n;

		Span* nextSpan = _idSpanMap.get(nextId);
		if (nextSpan == nullptr)
		{
			break;
		}

		//Span* nextSpan = ret->second;
		if (nextSpan->_usecount != 0)
		{
			break;
		}

		// ����128ҳ������Ҫ�ϲ���
		if (nextSpan->_n + span->_n >= MAX_PAGE)
		{
			break;
		}

		_spanList[nextSpan->_n].Erase(nextSpan);

		span->_n += nextSpan->_n;

		{
			//std::lock_guard<std::mutex> lock(_map_mtx);
			for (PageID i = 0; i < nextSpan->_n; ++i)
			{
				_idSpanMap[nextSpan->_pageId + i] = span;
			}
		}

		delete nextSpan;
	}

	// �ϲ����Ĵ�span�����뵽��Ӧ��������
	_spanList[span->_n].PushFront(span);
}