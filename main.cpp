
#include "threadpool.cc"

#include <iostream>
#include <functional>
#include <thread>
#include <future>
using namespace std;

/*

������̳߳��ύ������ӷ���

1. pool.submitTask(sum1,10,20)
�ɱ����ģ����

2. ���Ǵ�����һ��Result�Լ���ص����ͣ�����ͦ��

ʹ��future������Result��ʡ�̳߳ش���

*/


int sum1(int a, int b) {
	return a + b;
}
int sum2(int a, int b, int c) {
	return a + b + c;
}

int main() {

	{
		ThreadPool pool;
		//pool.setMode(ThreadPoolMode::MODE_CACHED);
		pool.start(2);
		std::future<int> r1 = pool.submitTask(sum1, 1, 2);
		std::future<int> r2 = pool.submitTask(sum1, 1, 2);
		std::future<int> r3 = pool.submitTask(sum2, 1, 2, 3);
		std::future<int> r4 = pool.submitTask(sum1, 1, 2);

		std::cout << r1.get() << std::endl;
		std::cout << r3.get() << std::endl;
		std::cout << r2.get() << std::endl;
		std::cout << r4.get() << std::endl;
	}

	getchar();
} 