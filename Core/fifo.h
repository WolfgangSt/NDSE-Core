#ifndef _FIFO_H_
#define _FIFO_H_

template <int n> struct fifo
{
	enum { SIZE = n };
	unsigned long readpos;
	unsigned long writepos;
	unsigned long queue[SIZE];
	
	unsigned long size() const
	{
		return (writepos - readpos) % SIZE;
	}

	bool empty() const
	{
		return writepos == readpos;
	}

	bool full() const
	{
		return (!empty() && (size() == 0));
	}

	void write(unsigned long data)
	{
		queue[writepos % SIZE] = data;
		writepos++;
	}

	unsigned long top() const
	{
		return queue[readpos % SIZE];
	}

	unsigned long read()
	{
		unsigned long res = queue[readpos % SIZE];
		readpos++;
		return res;
	}

	void reset()
	{
		readpos = 0;
		writepos = 0;
	}
};


#endif
