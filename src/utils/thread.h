﻿#ifndef UTILS_THREAD_H_
#define UTILS_THREAD_H_

#include "utils.h"

namespace utils {
	typedef std::function<void()> ThreadCallback;

	class Thread;
	class Runnable {
	public:
		Runnable() {}
		virtual ~Runnable() {}

		virtual void Run(Thread *this_thread) = 0;
	};

	//thread 
	class Thread {
	public:
		explicit Thread() :
			target_(NULL)
			, enabled_(false)
			, running_(false)
			, handle_(Thread::INVALID_HANDLE)
			, thread_id_(0) {}

		explicit Thread(Runnable *target)
			: target_(target)
			, enabled_(false)
			, running_(false)
			, handle_(Thread::INVALID_HANDLE)
			, thread_id_(0) {}

		~Thread() {
		}

		//stop thread, return true if succeed
		bool Stop();

		//force to terminate thread
		bool Terminate();

		bool Start(std::string name = "");

		//stop and waiting for thead stopping
		bool JoinWithStop();

		bool enabled() const { return enabled_; };

		size_t thread_id() const { return thread_id_; };

		bool IsRunning() const { return running_; };

		//get current thread id
		static size_t current_thread_id();

		bool IsObjectValid() const { return Thread::INVALID_HANDLE != handle_; }

		static bool SetCurrentThreadName(std::string name);

		const std::string &GetName() const { return name_; }

	protected:
		UTILS_DISALLOW_EVIL_CONSTRUCTORS(Thread);
		std::string name_;

#ifdef WIN32
		static DWORD WINAPI threadProc(LPVOID param);
#else
		static void *threadProc(void *param);
#endif
		volatile bool enabled_;
		bool running_;

		Runnable *target_;

#ifdef WIN32
		HANDLE handle_;
		static const HANDLE INVALID_HANDLE;
#else
		pthread_t handle_;
		static const pthread_t INVALID_HANDLE;
#endif
		size_t thread_id_;

	protected:
		virtual void Run();
	};

	//thread group
	class ThreadGroup {
	public:
		ThreadGroup() {}
		~ThreadGroup() {
			JoinAll();
			for (size_t i = 0; i < _threads.size(); ++i) {
				delete _threads[i];
			}

			_threads.clear();
		}

		void AddThread(Thread *thread) {
			_threads.push_back(thread);
		}

		void StartAll() {
			for (size_t i = 0; i < _threads.size(); ++i) {
				_threads[i]->Start();
			}
		}

		void JoinAll() {
			for (size_t i = 0; i < _threads.size(); ++i) {
				_threads[i]->JoinWithStop();
			}
		}

		void StopAll() {
			for (size_t i = 0; i < _threads.size(); ++i) {
				_threads[i]->Stop();
			}
		}

		size_t size() const { return _threads.size(); }

	private:
		UTILS_DISALLOW_EVIL_CONSTRUCTORS(ThreadGroup);
		std::vector<Thread *> _threads;
	};

	class Mutex {
	public:
		Mutex();
		~Mutex();

		void Lock();
		void Unlock();
		pthread_mutex_t *mutex_pointer() { return &mutex_; }

	private:
		UTILS_DISALLOW_EVIL_CONSTRUCTORS(Mutex);

		uint32_t thread_id_;
		pthread_mutex_t mutex_;
	};

	class MutexGuard {
	public:
		MutexGuard(Mutex &mutex)
			: _mutex(mutex) {
			_mutex.Lock();
		}

		~MutexGuard() {
			_mutex.Unlock();
		}
	private:
		UTILS_DISALLOW_EVIL_CONSTRUCTORS(MutexGuard);
		Mutex &_mutex;
	};

	class ReadWriteLock {
	public:
		ReadWriteLock();
		~ReadWriteLock();

		void ReadLock();
		void ReadUnlock();
		void WriteLock();
		void WriteUnlock();

	private:
		UTILS_DISALLOW_EVIL_CONSTRUCTORS(ReadWriteLock);
		volatile long _reads;
		Mutex _enterLock;
	};

	class ReadLockGuard {
	public:
		ReadLockGuard(ReadWriteLock &lock)
			: lock_(lock) {
			lock_.ReadLock();
		}
		~ReadLockGuard() { lock_.ReadUnlock(); }
	private:
		UTILS_DISALLOW_EVIL_CONSTRUCTORS(ReadLockGuard);
		ReadWriteLock &lock_;
	};

	class WriteLockGuard {
	public:
		WriteLockGuard(ReadWriteLock &lock)
			: lock_(lock) {
			lock_.WriteLock();
		}
		~WriteLockGuard() { lock_.WriteUnlock(); }
	private:
		UTILS_DISALLOW_EVIL_CONSTRUCTORS(WriteLockGuard);
		ReadWriteLock &lock_;
	};


	// implement a spin lock using lock free.

	class SpinLock {
	public:
		/** ctor */
		SpinLock() :m_busy(SPINLOCK_FREE) {

		}

		/** dtor */
		virtual	~SpinLock() {

		}

		// lock
		inline void Lock() {
			while (SPINLOCK_BUSY == BUBI_CAS(&m_busy, SPINLOCK_BUSY, SPINLOCK_FREE)) {
				BUBI_YIELD();
			}
		}

		// unlock
		inline void Unlock() {
			BUBI_CAS(&m_busy, SPINLOCK_FREE, SPINLOCK_BUSY);
		}

	private:
		SpinLock(const SpinLock&);
		SpinLock& operator = (const SpinLock&);
		volatile uint32_t m_busy;
		static const int SPINLOCK_FREE = 0;
		static const int SPINLOCK_BUSY = 1;
	};

#define ReadLockGuard(x) error "Missing guard object name"
#define WriteLockGuard(x) error "Missing guard object name"

	class Semaphore {
	public:
#ifdef WIN32
		static const uint32_t kInfinite = INFINITE;
		typedef HANDLE sem_t;
#else
		static const uint32_t kInfinite = UINT_MAX;
#endif

		Semaphore(int32_t num = 0);
		~Semaphore();

		// P
		bool Wait(uint32_t millisecond = kInfinite);

		// V
		bool Signal();

	private:
		UTILS_DISALLOW_EVIL_CONSTRUCTORS(Semaphore);
		sem_t sem_;
	};

	class ThreadTaskQueue {
	public:
		ThreadTaskQueue();
		~ThreadTaskQueue();

		int PutFront(Runnable *task);
		int Put(Runnable *task);
		int Size();
		Runnable *Get();

	private:
		UTILS_DISALLOW_EVIL_CONSTRUCTORS(ThreadTaskQueue);
		typedef std::list<Runnable *> Tasks;
		Tasks tasks_;
		SpinLock spinLock_;
		Semaphore sem_;
	};

	class ThreadPool : public Runnable {
	public:
		ThreadPool();
		~ThreadPool();

		bool Init(const std::string &name, int threadNum = kDefaultThreadNum);
		bool Exit();

		//add task
		void AddTask(Runnable *task);

		void JoinwWithStop();

		/// wait all tasks has been 
		bool WaitAndJoin();

		// get thread's size
		size_t Size() const { return threads_.size(); }

		bool WaitTaskComplete();

		//terminate
		void Terminate();

	private:
		UTILS_DISALLOW_EVIL_CONSTRUCTORS(ThreadPool);
		typedef std::vector<Thread *> ThreadVector;

		//add worker
		bool AddWorker(int threadNum);

		void Run(Thread *this_thread);

		ThreadVector threads_;
		ThreadTaskQueue tasks_;
		bool enabled_;
		std::string name_;

		static const int32_t kDefaultThreadNum = 10;
	};

}

#endif // _UTILS_THREAD_H_
