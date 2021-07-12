#pragma once

#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"

//void* tcmalloc(size_t size)
static void* hcAlloc(size_t memSize)
{
	try
	{
		//����64*1024�ֽڵ��ڴ�ֱ����pagecache���루Ҳ����ֱ����ϵͳ�����룩
		if (memSize > MAX_BYTES)
		{
			//��������ڴ����϶���
			size_t npage = SizeClass::RoundUp(memSize) >> PAGE_SHIFT;
			//��pagecache�����ڴ棨���뵽����һ��span��
			Span* span = PageCache::GetInstance()->NewSpan(npage);
			span->_objsize = memSize;
			//ʹ��ҳ�ż����ڴ��ַ
			void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
			return ptr;
		}
		else
		{
			//ÿ���̴߳����Լ���threadcache
			if (tls_threadcache == nullptr)
			{
				tls_threadcache = new ThreadCache;
			}
			//ȥthreadcache�������ڴ�
			return tls_threadcache->Allocate(memSize);
		}
	}
	catch (const std::exception& e)
	{
		cout << e.what() << endl;
	}
	return nullptr;
}

static void hcFree(void* ptr)
{
	try
	{
		//ͨ���ڴ��ַ��map���ҵ���Ӧ��span
		Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
		size_t size = span->_objsize;

		if (size > MAX_BYTES)
		{
			//��������Ĵ�С����64*1024�ֽ�ֱ���ͷŵ�pagecache
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		}
		else
		{
			assert(tls_threadcache);
			//���ڴ�ҵ�threadcache��Ӧ��freelist������
			tls_threadcache->Deallocate(ptr, size);
		}
	}
	catch (const std::exception& e)
	{
		cout << e.what() << endl;
	}
}