//�ڴ����ֱ�����û��ṩ�����ڴ���ͷ��ڴ�Ľӿ�
#pragma once
#include"CommonResource.h"
#include"ThreadCache.h"
#include"PageCache.h"

//high concurrent memory pool
static void* hcMalloc(size_t memSize)
{
	try
	{
		if (memSize > THREAD_MAX_SIZE)
		{
			//��page cache�����ڴ�
			size_t npage = SizeCalculation::RoundUp(memSize) >> PAGE_SHIFT;
			Span* span = PageCache::GetInstance()->NewSpan(npage);
			span->_objsize = memSize;

			void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
			return ptr;
		}
		else
		{
			//����tls,ÿ���̶߳���thread cache����thread cache����
			if (tls_threadcache == nullptr)
			{
				tls_threadcache = new ThreadCache;
			}

			return tls_threadcache->Allocate(memSize);
		}
	}
	catch (const std::exception& e)
	{
		cout << e.what() << std::endl;
	}
	return nullptr;
}

static void hcFree(void* mem)
{
	try{
		//�����ڴ��С��map�в��Ҷ�Ӧ��span
		Span* sp = PageCache::GetInstance()->GetSpanToMap(mem);
		//��ȡmem�Ĵ�С
		size_t memSize = sp->_objsize;

		if (memSize > THREAD_MAX_SIZE)
		{
			//����pagecache
			PageCache::GetInstance()->ReleaseSpanToPageCache(sp);
		}
		else
		{
			//����threadcache
			assert(tls_threadcache);
			tls_threadcache->DeAllocate(mem, memSize);
		}
	}
	catch (const std::exception& e)
	{
		cout << e.what() << std::endl;
	}
}