
#ifndef TANGOUPSAMPLE_UTIL_H
#define TANGOUPSAMPLE_UTIL_H

#include <pthread.h>
#include <sstream>
#include <stdlib.h>
#include <string>
#include "oddcore/OddLog.h"
#include "oddcore/OddSharedPtr.h"

#define SharedPtr odd::SharedPtr

class Mutex
{
public:

	Mutex()
	{
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&mutex_, &attr);
	}

	void lock()
	{
		pthread_mutex_lock(&mutex_);
	}

	void unlock()
	{
		pthread_mutex_unlock(&mutex_);
	}
private:
	pthread_mutex_t mutex_;
};

class ScopedMutex
{
	Mutex& mutex;
public:
	ScopedMutex(Mutex& m) : mutex(m)
	{
		mutex.lock();
	}

	~ScopedMutex()
	{
		mutex.unlock();
	}
};

#endif  // TANGOUPSAMPLE_UTIL_H
