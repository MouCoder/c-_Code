//������Դ������һЩ������ͷ�ļ��Լ�������

#pragma once
#include<iostream>
#include<assert.h>
#include <exception>
#include <time.h>
#include <thread>
#include <mutex>
#include <windows.h>
#include <cstddef>

//ҳ��---32λϵͳʹ��size_t,64λϵͳʹ��long long
#ifdef _WIN32
	typedef size_t PageID;
#else
	typedef unsigned long long PageID;
#endif // _WIN32

#ifdef _WIN32
	typedef size_t ADDRES_INT;
#else
	typedef unsigned long long ADDRES_INT;
#endif // _WIN32

using std::cout;
using std::cin;

static const size_t THREAD_MAX_SIZE = 64 * 1024;   //thread cache���ܹ����������ڴ��СΪ64k
static const size_t THREAD_FREELIST_SIZE = 184;  //thread cache�е�freelist����Ĵ�С
static const size_t CENTRAL_SPANLIST_SIZE = 184; //central control cache��spanlist�Ĵ�С
static const size_t PAGE_SHIFT = 12;//һҳ�Ĵ�СΪ2^PAGE_SHIFT
static const size_t MAX_PAGE = 128;//page cache������ҳ��

inline void*& Next(void* mem)
{
	return *((void**)mem);
}

//�����������еĽ�С��
static size_t min(size_t num1, size_t num2)
{
	return num1 < num2 ? num1 : num2;
}

class SizeCalculation
{
	// ������12%���ҵ�����Ƭ�˷�
	// [1,128]					8byte����	 freelist[0,16)
	// [129,1024]				16byte����	 freelist[16,72)
	// [1025,8*1024]			128byte����	 freelist[72,128)
	// [8*1024+1,64*1024]		1024byte����  freelist[128,184)
public:
	//�����ڴ��С�Ͷ����������Ӧ�±�
	static inline size_t _Intex(size_t size, size_t alignmentShift)
	{
		//alignmentShift��ʾ��������λ�������������Ϊ8 = 2^3ʱ��aligmentShift = 3
		//�������Խ�����ת����>>���㣬�������Ч��
		return ((size + (1 << alignmentShift) - 1) >> alignmentShift) - 1;
	}

	//�����ڴ��С�������Ӧ���±�
	static inline size_t Index(size_t size)
	{
		assert(size <= THREAD_MAX_SIZE);

		//ÿ����������Ӧ�������������ֱ��ʾ8 16 128 1024�ֽڶ���
		int groupArray[4] = {0,16,56,56};

		if (size <= 128)
		{
			//8�ֽڶ���
			return _Intex(size, 3) + groupArray[0];
		}
		else if (size <= 1024)
		{
			//16�ֽڶ���
			return _Intex(size, 4) + groupArray[1];
		}
		else if (size <= 8192)
		{
			//128�ֽڶ���
			return _Intex(size, 7) + groupArray[2];
		}
		else if (size <= 65536)
		{
			//1024�ֽڶ���
			return _Intex(size, 10) + groupArray[3];
		}

		assert(false);
		return -1;
	}

	static inline size_t _RoundUp(size_t size, size_t alignment)
	{
		//�Ӻ����������С�Ͷ��������ж���
		return (size + alignment - 1) & ~(alignment - 1);
	}

	//��������ڴ��С�������϶���
	static inline size_t RoundUp(size_t size)
	{
		if (size <= 128)
		{
			//8�ֽڶ���
			return _RoundUp(size,8);
		}
		else if (size <= 1024)
		{
			//16�ֽڶ���
			return _RoundUp(size, 16);
		}
		else if (size <= 8192)
		{
			//128�ֽڶ���
			return _RoundUp(size, 128);
		}
		else if (size <= 65536)
		{
			//1024�ֽڶ���
			return _RoundUp(size, 1024);
		}
		else
			return _RoundUp(size, 1 << PAGE_SHIFT);

		return -1;
	}

	static inline size_t NumMoveSize(size_t memSize)
	{
		if (memSize == 0)
			return 0;

		// [2, 512]��һ�������ƶ����ٸ������(������)����ֵ
		// С����һ���������޸�
		// �����һ���������޵ͣ�����һ�θ���
		size_t num = THREAD_MAX_SIZE / memSize;
		if (num < 2)
			num = 2;

		if (num > 512)
			num = 512;

		return num;
	}

	//�����������ҳ�ڴ�
	static inline size_t NumMovePage(size_t memSize)
	{
		//����thread cache���������ٸ���������͸����ٸ�����
		size_t num = NumMoveSize(memSize);
		//��ʱ��nPage��ʾ���ǻ�ȡ���ڴ��С
		size_t nPage = num*memSize;
		//��npage������PAGE_SHIFTʱ��ʾ��2��PAGE_SHIFT�η�����ʾ�ľ���ҳ��
		nPage >>= PAGE_SHIFT;

		//���ٸ�һҳ�������˰�ҳ�����ԭ��
		if (nPage == 0)
			nPage = 1;

		return nPage;
	}
};

//��������������С���ڴ���������������й���
class FreeList
{
public:
	//�ж������Ƿ�Ϊ��
	bool empty()
	{
		return _head == nullptr;
	}

	void* pop()
	{
		assert(_head != nullptr);
		//ͷɾ
		void* mem = _head;
		_head = Next(_head);

		_size--;
		return mem;
	}

	//��ȡһ������
	void PopRange(void*& start,void*& end,size_t num)
	{
		//���ﱣ֤һ����num������ǰ���ͷ��ڴ�ǰ�Ѿ��жϹ��ˣ�
		start = _head;
		for (size_t i = 0; i < num; ++i)
		{
			end = _head;
			_head = Next(_head);
		}

		Next(end) = nullptr;
		_size -= num;
	}

	void push(void* mem)
	{
		//ͷ��
		Next(mem) = _head;
		_head = mem;

		_size++;
	}

	void PushRange(void* start, void* end, size_t size)
	{
		//����һ������
		Next(end) = _head;
		_head = start;

		_size += size;
	}

	size_t MaxSize()
	{
		return _maxSize;
	}

	size_t Size()
	{
		return _size;
	}

	void SetMaxSize(size_t newVal)
	{
		_maxSize = newVal;
	}
private:
	void* _head = nullptr; //����ͷ�ڵ�

	size_t _maxSize = 1;//ÿ�λ�ȡ���ڴ������������512
	size_t _size = 0;//�����ж���ĸ���
};

struct Span
{
	PageID _pageId = 0;   // ҳ��
	size_t _n = 0;        // ҳ������

	Span* _next = nullptr;
	Span* _prev = nullptr;

	void* _list = nullptr;  // ����ڴ���С�����������������ջ������ڴ�Ҳ��������
	size_t _usecount = 0;	// ʹ�ü�����==0 ˵�����ж��󶼻�����

	size_t _objsize = 0;	// �г����ĵ�������Ĵ�С
};

//�������ڴ�---˫���ͷѭ������
class SpanList
{
public:
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}
	bool isEmpty()
	{
		return _head->_next == _head;
	}
	
	Span* Begin()
	{
		//��ͷ˫��ѭ�������ͷ��_head->next
		return _head->_next;
	}

	Span* End()
	{
		//��ͷ˫��ѭ��������βָ����_head
		return _head;
	}

	//ͷɾ
	Span* PopFront()
	{
		Span* tmp = _head->_next;
		//��SpanList��ɾ���ڵ�
		Erase(tmp);
		return tmp;
	}
	//ͷ��
	void PushFront(Span* newSpan)
	{
		Insert(Begin(), newSpan);
	}

	//ɾ��һ��span
	void Erase(Span* cur)
	{
		assert(cur != _head);

		//˫���ͷѭ�������ɾ��
		Span* prev = cur->_prev;
		Span* next = cur->_next;

		prev->_next = next;
		next->_prev = prev;
	}

	//��curλ��֮ǰ����һ���ڵ�newspan
	void Insert(Span* cur, Span* newspan)
	{
		Span* prev = cur->_prev;
		prev->_next = newspan;
		newspan->_prev = prev;

		newspan->_next = cur;
		cur->_prev = newspan;
	}
private:
	Span* _head;
public:
	std::mutex _mtx;
};

//��ϵͳ����KPage���ڴ�
static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage*(1 << PAGE_SHIFT),
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// brk mmap��
#endif
	//����ʧ�����쳣
	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}
//�ͷ��ڴ�
static void SystemFree(void* mem)
{
	free(mem);
}