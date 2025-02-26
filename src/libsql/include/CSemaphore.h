#ifndef __INC_METIN_II_SEMAPHORE_H__
#define __INC_METIN_II_SEMAPHORE_H__

#include <semaphore.h>

class CSemaphore
{
	private:
		sem_t *	m_hSem;


	public:
		CSemaphore();
		~CSemaphore();

		int	Initialize();
		void	Clear();
		void	Destroy();
		int	Wait();
		int	Release(int count = 1);
};
#endif
