#include <iostream>
#include <stdlib.h>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>

using namespace std;
using namespace chrono;

static const auto NUM_TEST = 4000000;
static const auto KEY_RANGE = 1000;

//mutex없이 돌아가면 그게 싱글스레드

class mymutex
{
public:
	void lock() {}
	void Unlock() {}
};

class NODE
{
public:
	int key;
	NODE *next;
	bool marked;
	mutex node_lock;

	NODE()
	{
		next = nullptr;
		marked = false;
	}

	NODE(int x)
	{
		key = x;
		marked = false;
		next = nullptr;
	}
	~NODE() {}

	void Lock() {
		node_lock.lock();
	}
	void Unlock() {
		node_lock.unlock();
	}
};

class SPNODE
{
public:
	int key;
	shared_ptr <SPNODE> next;
	bool marked;
	mutex node_lock;

	SPNODE()
	{
		next = nullptr;
		marked = false;
	}

	SPNODE(int x)
	{
		key = x;
		marked = false;
		next = nullptr;
	}
	~SPNODE() {}

	void Lock() {
		node_lock.lock();
	}
	void Unlock() {
		node_lock.unlock();
	}
};


//Lock Free 알고리즘
class LPNODE
{
public:
	int key;
	unsigned int next;


	LPNODE()
	{
		next = NULL;
	}

	LPNODE(int x)
	{
		key = x;
		next = NULL;
	}

	~LPNODE() {}


	bool CAS(int old_value, int new_value)
	{
		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_int*>(&next), &old_value, new_value);
	}


	bool CAS(LPNODE* old_addr, LPNODE* new_addr, bool old_mark, bool new_mark)
	{
		int old_value = reinterpret_cast<int>(old_addr);
		if (old_mark)
			old_value = old_value | 1;
		else old_value = old_value & 0xFFFFFFFE;

		int new_value = reinterpret_cast<int>(new_addr);
		if (new_mark)
			new_value = new_value | 1;
		else new_value = new_value & 0xFFFFFFFE;

		return CAS(old_value, new_value);
	}

	bool  AttemptMark(LPNODE* ptr)
	{
		return CAS(ptr, ptr, false, true);
	}

	LPNODE* GetNext()
	{
		return reinterpret_cast<LPNODE*>(next & 0xFFFFFFFE);
	}

	LPNODE* GetNextWithMark(bool* removed)
	{
		*removed = (1 == (next & 0x01));
		return GetNext();
	}


};

//Test
class LFNODE
{
public:
	int key;
	int next;

	LFNODE() {
		next = 0;
	}

	LFNODE(int key_value) {
		next = 0;
		key = key_value;
	}

	~LFNODE() {
	}

	LFNODE *GetNext() {
		return reinterpret_cast<LFNODE *>(next & 0xFFFFFFFE);
	}

	LFNODE *GetNextMark(bool *removed) {
		*removed = (next & 1) == 1;
		return reinterpret_cast<LFNODE *>(next & 0xFFFFFFFE);
	}

	bool CAS(int old_value, int new_value) {
		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_int *>(&next), &old_value, new_value);
	}

	bool CAS(LFNODE *old_addr, LFNODE *new_addr, bool old_mark, bool new_mark)
	{
		int old_value = reinterpret_cast<int>(old_addr);
		if (true == old_mark) old_value = old_value | 1;
		else old_value = old_value & 0xFFFFFFFE;

		int new_value = reinterpret_cast<int>(new_addr);
		if (true == new_mark) new_value = new_value | 1;
		else new_value = new_value & 0xFFFFFFFE;
	}

	bool TryRemove(LFNODE *next)
	{
		int old_value = reinterpret_cast<int>(next) & 0xFFFFFFFE;
		int new_value = old_value | 1;
		return CAS(old_value, new_value);
	}

	void SetNext(LFNODE *new_next)
	{
		next = reinterpret_cast<int>(new_next);
	}
};


class LF_SET
{
	LPNODE head, tail;

public:
	LF_SET()
	{
		head.key = 0x80000000;
		head.next = reinterpret_cast<unsigned  int>(&tail);
		tail.key = 0x7FFFFFFF;
		tail.next = NULL;
	}


	~LF_SET()
	{
		Clear();
	}

	void Clear()
	{
		while (head.next != reinterpret_cast<int>(&tail))
		{
			LPNODE *ptr = reinterpret_cast<LPNODE*>(head.next);
			head.next = reinterpret_cast<int>(ptr->GetNext());
			delete ptr;
		}

	}

	void Print20()
	{
		LPNODE *ptr = head.GetNext();
		for (int i = 0; i < 20; ++i)
		{
			if (&tail == ptr)
				break;

			cout << ptr->key << ", ";
			ptr = ptr->GetNext();
		}

		cout << endl;
	}

	void Find(int x, LPNODE** prev, LPNODE** curr)
	{
	RETRY:
		LPNODE* pr = &head;
		LPNODE* cu = pr->GetNext();

		while (true)
		{
			bool marked;

			LPNODE* su = cu->GetNextWithMark(&marked);

			while (true == marked)
			{
				if (pr->CAS(cu, su, false, false) == false)
					goto RETRY;

				cu = su;
				su = cu->GetNextWithMark(&marked);
			}

			if (cu->key >= x)
			{
				*prev = pr;
				*curr = cu;

				return;
			}

			pr = cu;
			cu = su;

		}
	}

	bool Add(int x)
	{
		LPNODE *prev, *curr;

		while (true)
		{
			Find(x, &prev, &curr);

			if (x == curr->key)
				return false;
			else
			{
				LPNODE* node = new LPNODE{ x };

				node->next = reinterpret_cast<unsigned int>(curr);

				if (true == prev->CAS(curr, node, false, false))
					return true;
			}
		}
	}




	bool Remove(int x)
	{
		LPNODE *prev, *curr;

		while (true)
		{
			Find(x, &prev, &curr);

			if (x != curr->key)
			{
				return false;
			}
			else
			{
				LPNODE* succ = curr->GetNext();

				if (curr->AttemptMark(succ) == true)
				{
					prev->CAS(curr, succ, false, false);
					return true;
				}

			}


		}
	}


	bool Contains(int x)
	{
		LPNODE *curr;

		curr = head.GetNext();

		while (curr->key < x)
		{
			curr = curr->GetNext();
		}

		return (curr->key == x) && (false == (curr->next & 0x1));
	}
};

class C_SET
{
	NODE head, tail;
	mutex mlock;
public:
	C_SET()
	{
		head.key = 0x80000000;
		head.next = &tail;
		tail.key = 0x7fffffff;
		tail.next = nullptr;
	}
	~C_SET() {
		Clear();
	}
	void Clear()
	{
		mlock.lock();
		while (head.next != &tail) {
			NODE *ptr = head.next;
			head.next = ptr->next;
			delete ptr;
		}
		mlock.unlock();
	}
	void Print20()
	{
		mlock.lock();
		NODE *ptr = head.next;
		for (int i = 0; i < 20; ++i) {
			if (&tail == ptr) break;
			cout << ptr->key << ", ";
			ptr = ptr->next;
		}
		mlock.unlock();
		cout << endl;
	}
	bool Add(int x)
	{
		NODE *prev, *curr;
		prev = &head;
		mlock.lock();
		curr = head.next;
		while (curr->key < x) {
			prev = curr;
			curr = curr->next;
		}
		if (x == curr->key) {
			mlock.unlock();
			return false;
		}
		else {
			NODE *node = new NODE{ x };
			node->next = curr;
			prev->next = node;
			mlock.unlock();
			return true;
		}
	}
	bool Remove(int x)
	{
		NODE *prev, *curr;
		prev = &head;
		mlock.lock();
		curr = head.next;
		while (curr->key < x) {
			prev = curr;
			curr = curr->next;
		}
		if (x != curr->key) {
			mlock.unlock();
			return false;
		}
		else {
			prev->next = curr->next;
			delete curr;
			mlock.unlock();
			return true;
		}
	}
	bool Contains(int x)
	{
		NODE *prev, *curr;
		prev = &head;
		mlock.lock();
		curr = head.next;
		while (curr->key < x) {
			prev = curr;
			curr = curr->next;
		}
		if (x != curr->key) {
			mlock.unlock();
			return false;
		}
		else {
			mlock.unlock();
			return true;
		}
	}
};

class F_SET
{
	NODE head, tail;
public:
	F_SET()
	{
		head.key = 0x80000000;
		head.next = &tail;
		tail.key = 0x7fffffff;
		tail.next = nullptr;
	}
	~F_SET() {
		head.Unlock();
		tail.Unlock();
		Clear();
	}
	void Clear()
	{
		while (head.next != &tail) {
			NODE *ptr = head.next;
			head.next = ptr->next;
			delete ptr;
		}
	}
	void Print20()
	{
		NODE *ptr = head.next;
		for (int i = 0; i < 20; ++i) {
			if (&tail == ptr) break;
			cout << ptr->key << ", ";
			ptr = ptr->next;
		}
		cout << endl;
	}
	bool Add(int x)
	{
		NODE *prev, *curr;
		prev = &head;
		prev->Lock();
		curr = head.next;
		curr->Lock();
		while (curr->key < x) {
			prev->Unlock();
			prev = curr;
			curr = curr->next;
			curr->Lock();
		}

		if (x == curr->key) {
			curr->Unlock();
			prev->Unlock();
			return false;
		}
		else {
			NODE *node = new NODE{ x };
			node->next = curr;
			prev->next = node;
			curr->Unlock();
			prev->Unlock();
			return true;
		}
	}
	bool Remove(int x)
	{
		NODE *prev, *curr;
		prev = &head;
		prev->Lock();
		curr = head.next;
		curr->Lock();
		while (curr->key < x) {
			prev->Unlock();
			prev = curr;
			curr = curr->next;
			curr->Lock();
		}
		if (x != curr->key) {
			curr->Unlock();
			prev->Unlock();
			return false;
		}
		else {
			prev->next = curr->next;
			curr->Unlock();
			prev->Unlock();
			delete curr;
			return true;
		}
	}
	bool Contains(int x)
	{
		NODE *prev, *curr;
		prev = &head;
		prev->Lock();
		curr = head.next;
		curr->Lock();
		while (curr->key < x) {
			prev->Unlock();
			prev = curr;
			curr = curr->next;
			curr->Lock();
		}
		if (x != curr->key) {
			curr->Unlock();
			prev->Unlock();
			return false;
		}
		else {
			curr->Unlock();
			prev->Unlock();
			return true;
		}
	}
};

class O_SET
{
	NODE head, tail;
public:
	O_SET()
	{
		head.key = 0x80000000;
		head.next = &tail;
		tail.key = 0x7fffffff;
		tail.next = nullptr;
	}
	~O_SET() {
		head.Unlock();
		tail.Unlock();
		Clear();
	}
	void Clear()
	{
		while (head.next != &tail) {
			NODE *ptr = head.next;
			head.next = ptr->next;
			delete ptr;
		}
	}
	void Print20()
	{
		NODE *ptr = head.next;
		for (int i = 0; i < 20; ++i) {
			if (&tail == ptr) break;
			cout << ptr->key << ", ";
			ptr = ptr->next;
		}
		cout << endl;
	}
	bool Validate(NODE *prev, NODE *curr)
	{
		NODE *ptr = &head;
		while (prev->key > ptr->key) ptr = ptr->next;
		if (prev != ptr) return false;
		return ptr->next == curr;
	}

	bool Add(int x)
	{
		NODE *prev, *curr;

		while (true)
		{
			prev = &head;
			curr = head.next;
			while (curr->key < x) {
				prev = curr;
				curr = curr->next;
			}
			prev->Lock();
			curr->Lock();
			if (false == Validate(prev, curr))
			{
				curr->Unlock();
				prev->Unlock();
				continue;
			}

			if (x == curr->key) {
				curr->Unlock();
				prev->Unlock();
				return false;
			}
			else {
				NODE *node = new NODE{ x };
				node->next = curr;
				prev->next = node;
				curr->Unlock();
				prev->Unlock();
				return true;
			}
		}
	}
	bool Remove(int x)
	{
		NODE *prev, *curr;

		while (true)
		{
			prev = &head;
			curr = head.next;
			while (curr->key < x) {
				prev = curr;
				curr = curr->next;
			}
			prev->Lock();
			curr->Lock();
			if (false == Validate(prev, curr))
			{
				curr->Unlock();
				prev->Unlock();
				continue;
			}

			if (x != curr->key) {
				curr->Unlock();
				prev->Unlock();
				return false;
			}
			else {
				prev->next = curr->next;
				curr->Unlock();
				prev->Unlock();
				return true;
			}
		}
	}
	bool Contains(int x)
	{
		NODE *prev, *curr;

		while (true)
		{
			prev = &head;
			curr = head.next;
			while (curr->key < x) {
				prev = curr;
				curr = curr->next;
			}
			prev->Lock();
			curr->Lock();
			if (false == Validate(prev, curr))
			{
				curr->Unlock();
				prev->Unlock();
				continue;
			}

			if (x == curr->key) {
				curr->Unlock();
				prev->Unlock();
				return true;
			}
			else {
				curr->Unlock();
				prev->Unlock();
				return false;
			}
		}
	}
};


class SPZ_SET
{
	shared_ptr<SPNODE> head, tail;
public:
	SPZ_SET()
	{
		head = make_shared<SPNODE>(0x80000000);
		tail = make_shared<SPNODE>(0x7fffffff);
		head->next = tail;
	}

	~SPZ_SET() {
		head = nullptr;
		tail = nullptr;
	}
	void Clear()
	{
		head->next = tail;
	}
	void Print20()
	{
		shared_ptr<SPNODE> ptr = head->next;
		for (int i = 0; i < 20; ++i) {
			if (tail == ptr) break;
			cout << ptr->key << ", ";
			ptr = ptr->next;
		}
		cout << endl;
	}

	bool Validate(const shared_ptr<SPNODE>& prev, const shared_ptr<SPNODE>& curr)
	{
		return (false == prev->marked) && (false == curr->marked) &&
			(prev->next == curr);
	}

	bool Add(int x)
	{
		shared_ptr<SPNODE> prev, curr;

		while (true)
		{
			prev = head;
			curr = head->next;
			while (curr->key < x) {
				prev = curr;
				curr = curr->next;
			}
			prev->Lock();
			curr->Lock();
			if (false == Validate(prev, curr))
			{
				curr->Unlock();
				prev->Unlock();
				continue;
			}

			if (x == curr->key) {
				curr->Unlock();
				prev->Unlock();
				return false;
			}
			else {
				shared_ptr<SPNODE> node = make_shared<SPNODE>(x);
				node->next = curr;
				prev->next = node;
				curr->Unlock();
				prev->Unlock();
				return true;
			}
		}
	}

	bool Remove(int x)
	{
		shared_ptr<SPNODE> prev, curr;

		while (true)
		{
			prev = head;
			curr = head->next;
			while (curr->key < x) {
				prev = curr;
				curr = curr->next;
			}
			prev->Lock();
			curr->Lock();
			if (false == Validate(prev, curr))
			{
				curr->Unlock();
				prev->Unlock();
				continue;
			}

			if (x != curr->key) {
				curr->Unlock();
				prev->Unlock();
				return false;
			}
			else {
				curr->marked = true;
				prev->next = curr->next;
				curr->Unlock();
				prev->Unlock();
				return true;
			}
		}
	}

	bool Contains(int x)
	{
		shared_ptr<SPNODE> curr;

		curr = head->next;
		while (curr->key < x) curr = curr->next;
		return (curr->key == x) && (false == curr->marked);
	}
};


static const int MAX_HEIGHT = 10;

// Skip List Node
class SNODE
{
public:
	int key;
	SNODE* next[MAX_HEIGHT];
	int toplevel; //Index of last link

public:
	SNODE()
	{
		key = 0;
		toplevel = MAX_HEIGHT;
		memset(next, 0, sizeof(next));
		// for (auto &p : next) { p = nullptr; }
	}
	SNODE(int value)
	{
		key = value;
		toplevel = MAX_HEIGHT;
		memset(next, 0, sizeof(next));
		// for (auto &p : next) { p = nullptr; }
	}

	SNODE(int value, int max_index)
	{
		key = value;
		toplevel = max_index;
		memset(next, 0, sizeof(next));
		// for (auto &p : next) { p = nullptr; }
	}
};

class SKIP_LIST_SET {
	SNODE head, tail;
	mutex m_lock;

public:
	SKIP_LIST_SET()
	{
		head.key = 0x80000000;
		tail.key = 0x7fffffff;
		head.toplevel = tail.toplevel = MAX_HEIGHT; // 전체 노드를 일정하게.
		for (auto &p : head.next) { p = &tail; }
	}
	~SKIP_LIST_SET() 
	{
		Clear();
	}
	void Clear()
	{
		SNODE* ptr;
		while (head.next[0] != &tail) {
			ptr = head.next[0];
			head.next[0] = head.next[0]->next[0];
			delete ptr;
		}
		for (auto &p : head.next) { p = &tail; }
	}

	void Find(int key, SNODE* prev[MAX_HEIGHT], SNODE* curr[MAX_HEIGHT])
	{
		int cNumber = MAX_HEIGHT - 1;
		while (true) {
			if (MAX_HEIGHT - 1 == cNumber) {
				prev[cNumber] = &head;

			}
			else {
				prev[cNumber] = prev[cNumber + 1];
			}
			curr[cNumber] = prev[cNumber]->next[cNumber];

			while (curr[cNumber]->key < key) {
				prev[cNumber] = curr[cNumber];
				curr[cNumber] = curr[cNumber]->next[cNumber];
			}

			if (cNumber != 0) {
				cNumber--;
			}
			else {
				return;
			}
		}
	}
	bool Add(int key)
	{
		SNODE *prev[MAX_HEIGHT], *curr[MAX_HEIGHT];
		m_lock.lock();
		Find(key, prev, curr);
		if (key == curr[0]->key) {
			// 이미 있는 데이터는 예외처리 .
			printf("Already Key Value .");
			m_lock.unlock();
			return false;
		}
		else {
			int level = 1; // level = 0이 되면 안되기 때문에 1로 지정.
			while (rand() % 2 == 0) {
				// Level이 올라갈수록 확률이 적어지게 해야한다.
				// 쓸데없는 Level이 많이 존재하게 되면 search, add, delete등 굉장히 많은 부하가 발생하기 때문이다.
				level++;
				if (MAX_HEIGHT == level) {
					break;
				}
			}
			SNODE* node = new SNODE(key, level);

			for (size_t i = 0; i < level; ++i)
			{
				node->next[i] = curr[i];
				prev[i]->next[i] = node;
			}

		}
		m_lock.unlock();
		return true;
	}
	bool Remove(int key) 
	{
		SNODE *prev[MAX_HEIGHT], *curr[MAX_HEIGHT];
		m_lock.lock();
		Find(key, prev, curr);

		if (key == curr[0]->key) {
			// 찾으면 가차없이 삭제.
			for (size_t i = 0; i < curr[0]->toplevel; ++i) {
				prev[i]->next[i] = curr[i]->next[i];
			}
			delete curr[0];
			m_lock.lock();
			return true;
		}
		else {
			m_lock.unlock();
			printf("Wrong Key Value .");
			return false;
		}
	}
	bool Contains(int key)
	{
		SNODE *prev[MAX_HEIGHT], *curr[MAX_HEIGHT];
		m_lock.lock();
		Find(key, prev, curr);

		if (key == curr[0]->key) {
			m_lock.unlock();
			return true;
		}
		else {
			m_lock.unlock();
			return false;
		}
	}

	void Print20()
	{
		SNODE *ptr = head.next[0];
		for (int i = 0; i < 20; ++i)
		{
			if (&tail == ptr)
				break;

			cout << ptr->key << ", ";
			ptr = ptr->next[0];
		}

		cout << endl;
	}
};

//class C_SK_SET
//{
//	SNODE head, tail;
//	mutex mlock;
//public:
//	C_SK_SET()
//	{
//		head.key = 0x80000000;
//
//		for (int i = 0; i < MAX_HEIGHT; ++i) head.next[i] = &tail;
//
//		tail.key = 0x7fffffff;
//
//		for (int i = 0; i < MAX_HEIGHT; ++i) tail.next[i] = nullptr;
//
//	}
//
//	~C_SK_SET()
//	{
//		Clear();
//	}
//
//	void Clear()
//	{
//
//		while (head.next[0] != &tail)
//		{
//			SNODE *ptr = head.next[0];
//			head.next[0] = ptr->next[0];
//			delete ptr;
//		}
//
//		for (int i = 0; i < MAX_HEIGHT; ++i)
//		{
//			head.next[i] = &tail;
//		}
//	}
//
//	void Print20()
//	{
//		SNODE *ptr = head.next[0];
//		for (int i = 0; i < 20; ++i)
//		{
//			if (&tail == ptr) break;
//			cout << ptr->key << ", ";
//			ptr = ptr->next[0];
//		}
//		cout << endl;
//	}
//
//	void Find(int x, SNODE* prev[], SNODE* curr[])
//	{
//		prev[MAX_HEIGHT - 1] = &head;
//		curr[MAX_HEIGHT - 1] = head.next[MAX_HEIGHT];
//
//		for (int level = MAX_HEIGHT - 1; level <= 0; ++level)
//		{
//			while (curr[level]->key < x)
//			{
//				prev[level] = curr[level];
//				curr[level] = curr[level]->next[level];
//			}
//
//			if (0 == level)
//				break;
//
//			prev[level - 1] = prev[level];
//			curr[level - 1] = curr[level - 1]->next[level - 1];
//		}
//	}
//
//	bool Add(int x)
//	{
//		SNODE *prev[MAX_HEIGHT], *curr[MAX_HEIGHT];
//
//
//		mlock.lock();
//
//		Find(x, prev, curr);
//
//		if (x == curr[0]->key)
//		{
//			mlock.unlock();
//			return false;
//		}
//		else
//		{
//			int height = rand() % MAX_HEIGHT;
//			SNODE *node = new SNODE{ x, height };
//			//여기서부터 수정하면됨
//			for (int i = 0; i < height; ++i)
//			{
//				node->next[i] = curr[i];
//				prev[i]->next[i] = node;
//			}
//			//->수정
//			for (int i = 0; i < height; ++i)
//			{
//				node->next[i] = curr[i];
//				prev[i]->next[i] = node;
//			}
//			//->수정
//			mlock.unlock();
//			return true;
//		}
//	}
//	bool Remove(int x)
//	{
//		SNODE *prev[MAX_HEIGHT], *curr[MAX_HEIGHT];
//
//		//prev = &head;
//		mlock.lock();
//
//
//		Find(x, prev, curr);
//
//		if (x != curr[0]->key)
//		{
//			mlock.unlock();
//			return false;
//		}
//		else
//		{
//			for (int i = 0; i < curr[0]->toplevel; ++i)
//			{
//				prev[i]->next[i] = curr[i]->next[i];
//			}
//
//			delete curr[0];
//			mlock.unlock();
//			return true;
//		}
//	}
//	bool Contains(int x)
//	{
//		SNODE *prev[MAX_HEIGHT], *curr[MAX_HEIGHT];
//
//		//prev = &head;
//		mlock.lock();
//		//curr = head.next;
//		Find(x, prev, curr);
//		if (x != curr[0]->key)
//		{
//			mlock.unlock();
//			return false;
//		}
//		else
//		{
//			mlock.unlock();
//			return true;
//		}
//	}
//};


//C_SET myset;
//F_SET myset;
//O_SET myset;
//Z_SET myset;
//LF_SET myset;
//C_SK_SET myset;
SKIP_LIST_SET myset;

// 자동으로 돌게끔 하기 위함이다.
// Random으로 하든 말든 딱히 차이는 없다.
void Benchmark(int num_thread)
{
	int key;

	for (int i = 0; i < NUM_TEST / num_thread; ++i) {
		switch (rand() % 3) {
		case 0: key = rand() % KEY_RANGE;
			myset.Add(key);
			break;
		case 1: key = rand() % KEY_RANGE;
			myset.Remove(key);
			break;
		case 2: key = rand() % KEY_RANGE;
			myset.Contains(key);
			break;
		default: cout << "ERROR\n";
			exit(-1);
		}
	}
}

int main()
{
	vector <thread *> worker_threads;

	for (int num_thread = 1; num_thread <= 16; num_thread *= 2)
	{
		myset.Clear();

		auto start = high_resolution_clock::now();
		for (int i = 0; i < num_thread; ++i)
			//worker_threads.emplace_back(Benchmark, num_thread);
			worker_threads.push_back(new thread{ Benchmark, num_thread });
		for (auto pth : worker_threads) pth->join();
		auto du = high_resolution_clock::now() - start;


		for (auto pth : worker_threads) delete pth;
		worker_threads.clear();
		myset.Print20();
		cout << num_thread << " Threads   ";
		cout << duration_cast<milliseconds>(du).count() << "ms \n";
		
	}
}