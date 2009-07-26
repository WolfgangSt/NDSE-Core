#ifndef _FIFO_H_
#define _FIFO_H_

template <typename T> struct fifo
{
	enum { SIZE = 16 };
	static unsigned long readpos;
	static unsigned long writepos;
	static unsigned long queue[SIZE];
	
	static unsigned long size()
	{
		return (writepos - readpos) % SIZE;
	}

	static bool empty()
	{
		return writepos == readpos;
	}

	static bool full()
	{
		return (!empty() && (size() == 0));
	}

	static void write(unsigned long data)
	{
		queue[writepos % SIZE] = data;
		writepos++;
	}

	static unsigned long top()
	{
		return queue[readpos % SIZE];
	}

	static unsigned long read()
	{
		unsigned long res = queue[readpos % SIZE];
		readpos++;
		return res;
	}

};

template <typename T> unsigned long fifo<T>::queue[fifo<T>::SIZE];
template <typename T> unsigned long fifo<T>::readpos;
template <typename T> unsigned long fifo<T>::writepos;

#endif
