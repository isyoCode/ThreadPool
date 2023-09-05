
#include "threadpool.cc"

#include <iostream>
#include <functional>
#include <thread>
#include <future>
using namespace std;

/*

如何让线程池提交任务更加方便

1. pool.submitTask(sum1,10,20)
可变参数模板编程

2. 我们创建了一个Result以及相关的类型，代码挺多

使用future来代替Result节省线程池代码

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