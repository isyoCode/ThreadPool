#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <unordered_map>
#include <future>
#include <iostream>

const int TASK_MAX_THREADHOLDER = 1024;
const int THREAD_SIZE_MAX_HOLD = 200;
const int THREAD_IDLE_TIME = 10;



enum  class ThreadPoolMode
{
	MODE_FIXED,	//�̶��������߳�
	MODE_CACHED	// �߳������ɶ�̬����
};

// �߳�����
class Thread
{
public:
	// ����һ���������� ���Խ���ThreadPool->threadfunc
	using ThreadFunc = std::function<void(int)>;
	// �̹߳���
	Thread(ThreadFunc func) :
		func_(func),
		ThreadId_(generateId_)
	{
		generateId_++;
	}
	~Thread() = default;
	// �����̷߳���
	void start() {
		// ����һ���߳���ִ���̺߳���
		std::thread t(func_, ThreadId_);
		t.detach();	// ���÷����߳�
	}
	//��ȡ�߳�id
	int getId() const {
		return ThreadId_;
	}
private:
	ThreadFunc func_;
	static int generateId_;
	int ThreadId_;	// �����߳�id
};

int Thread::generateId_ = 0;

// �̳߳�����
class ThreadPool {
public:
	ThreadPool() :
		initThreadSize_(4),
		tasksize_(0),
		idleThreadSize_(0),
		curThreadSize_(0),
		threadSizeMaxHold_(THREAD_SIZE_MAX_HOLD),
		taskQueMaxThreadHold_(TASK_MAX_THREADHOLDER),
		poolMode_(ThreadPoolMode::MODE_FIXED),
		isPoolRunning_(false) 
	{}
	~ThreadPool() {
		isPoolRunning_ = false;
		// �ȴ������̷߳���  ������״̬ ���� & ����ִ��������
		//�����еȴ��̻߳���
		
		std::unique_lock<std::mutex> lock(taskQueMtx);
		notEmpty_.notify_all();
		exitCond_.wait(lock, [&]()->bool {
			return threads_.size() == 0;
			});
	}
	//�����̳߳صĹ���ģʽ
	void setMode(ThreadPoolMode mode) {
		if (checkRunningStates())
		{
			return;
		}
		poolMode_ = mode;
	}
	// ���ó�ʼ�߳�����
	void setInitThreadsSize(int32_t size) {
		initThreadSize_ = size;
	}
	// ����cachedģʽ���߳���ֵ
	void setThreadSizeHold(int threadhold) {
		if (checkRunningStates()) {
			return;
		}
		if (poolMode_ == ThreadPoolMode::MODE_CACHED) {
			threadSizeMaxHold_ = threadhold;
		}
	}
	// ����task����������ֵ
	void setTaskQueMaxThredsHold(int32_t threadHolds) {
		taskQueMaxThreadHold_ = threadHolds;
	}
	//���̳߳��ύ����
	template<typename Func,typename ...Args>
	auto submitTask(Func&& func, Args&&... args) ->std::future<decltype(func(args...))>
	{
		using Rtype = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<Rtype()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
			);
		std::future<Rtype> result = task->get_future();

		// ��ȡ��
		std::unique_lock<std::mutex> lock(taskQueMtx);
		// �߳�ͨ�� �ȴ���������п���
		if (!notFull_.wait_for(lock, std::chrono::seconds(1),
			[&]() -> bool { return taskQue_.size() < taskQueMaxThreadHold_; }))
		{
			// ��ʾ notFull �ȴ�1s ������Ȼû������ (û������)
			std::cerr << "taskQue is full, submit task fail" << std::endl;
			auto task = std::make_shared<std::packaged_task<Rtype()>>(
				[]() -> bool { return -5678; }
			);
			(*task)();
			return task->get_future();
		}
		// ����п��� ����������������֮��
		taskQue_.emplace([task]() { (*task)(); });
		tasksize_++;
		// ��Ϊ�·������� ������п϶������� notEmpty֪ͨ
		notEmpty_.notify_all();
		// cachedģʽ  �������ȽϽ��� ������С���죬��Ҫ�������������Ϳ����̵߳�����
		if (poolMode_ == ThreadPoolMode::MODE_CACHED && tasksize_ > idleThreadSize_
			&& curThreadSize_ < threadSizeMaxHold_) {
			std::cout << "creat new thread  " << std::this_thread::get_id() << " exit" << std::endl;
			// �������߳�
			auto ptr{ std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this,std::placeholders::_1)) };
			//threads_.emplace_back(std::move(ptr));
			int threadid = ptr->getId();
			threads_.insert({ threadid, std::move(ptr) });
			//�����߳�
			threads_[threadid]->start();
			idleThreadSize_++;
			curThreadSize_++;
		}
		// ���������result����
		return result;
	}

	// �����̳߳�
	void start(int initThreadsize = std::thread::hardware_concurrency()) {
		// �����̳߳�����״̬
		isPoolRunning_ = true;
		//��ʼ�̸߳��� 
		initThreadSize_ = initThreadsize;
		curThreadSize_ = initThreadsize;
		//�����̶߳���
		for (size_t i = 0; i < initThreadSize_; i++)
		{
			// �̳߳��е��õ��̷߳���Ӧ�����̳߳ؾ��� --> ����Ӧ�ð�threadpool::threadFunc
			auto ptr{ std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this,std::placeholders::_1)) };
			//threads_.emplace_back(std::move(ptr));
			int threadid = ptr->getId();
			threads_.insert({ threadid, std::move(ptr) });
		}
		// ���������߳�
		for (size_t i = 0; i < initThreadSize_; i++)
		{
			threads_[i]->start();
			idleThreadSize_++;  // ��¼��ʼ�����̵߳�����
		}
	}
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	//�����̺߳���
	void threadFunc(int threadid) {
		auto lastTime = std::chrono::high_resolution_clock().now();
		while (1)
		{
			Task task;
			{
				// �Ȼ�ȡ�� 
				std::unique_lock<std::mutex> lock(taskQueMtx);
				// �ȴ� notempty����
				std::cout << " ready to get task .... " << std::this_thread::get_id() << std::endl;
				// cachedģʽ�� �п����Ѿ������˺ܶ���̣߳����ǿ���ʱ�䳬��60s
				// Ӧ�ðѶ�����߳̽������յ� (���� initThreadSize_�������߳̽��л��գ�
				// ��ǰʱ�� - ��һ���߳�ִ�е�ʱ�� > 60 s
				// ÿһ���ӷ���һ��  ��ô����? ��ʱ���أ� ����������ִ�з���
				while (taskQue_.empty())
				{
					if (!isPoolRunning_) {
						threads_.erase(threadid);
						std::cout << "threadid : " << std::this_thread::get_id() << "exit!" << std::endl;
						curThreadSize_--;
						exitCond_.notify_all();
						return;
					}
					if (poolMode_ == ThreadPoolMode::MODE_CACHED) {
						if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1))) {
							// ��ʱ����
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
							if (dur.count() >= THREAD_IDLE_TIME && curThreadSize_ > initThreadSize_) {
								// ��ʼ���յ�ǰ�߳�
								// ��¼�߳���������ر�����ֵ
								// ���̶߳�����б���ɾ��
								threads_.erase(threadid);
								curThreadSize_--;
								idleThreadSize_--;
								std::cout << "�����̸߳���" << std::endl;
								return;
							}
						}
					}
					else {
						// �ȴ� ����
						notEmpty_.wait(lock);
					}
				}
				idleThreadSize_--;
				std::cout << "get task successful ... " << std::this_thread::get_id() << std::endl;
				// �����������ȡһ���������
				task = taskQue_.front();
				taskQue_.pop();
				tasksize_--;
				// ��Ȼ��ʣ������ ֪ͨ�����߳̽��д��� 
				if (!taskQue_.empty()) {
					notEmpty_.notify_all();
				}
				// ȡ��һ������ ����֪ͨ, ���Լ���������
				notFull_.notify_all();
			}
			//��ǰ�̸߳����������
			if (task != nullptr) {
				//task->run();	// ִ������ ������ķ���ֵsetval���� ����Result
				task();
			}
			idleThreadSize_++;
			lastTime = std::chrono::high_resolution_clock().now(); // �����߳�ִ���������ʱ��
		}
	}

	// ���pool������״̬
	bool checkRunningStates() const {
		return isPoolRunning_;
	}

private:

	//std::vector<std::unique_ptr<Thread>> threads_;	// �߳��б�
	std::unordered_map<int, std::unique_ptr<Thread>> threads_; // �߳��б�

	int32_t initThreadSize_;    //��ʼ�߳�����
	//��Ҫ�������
	using Task = std::function<void()>;
	std::queue<Task> taskQue_;

	//��¼�������
	std::atomic_int tasksize_;
	//��¼��ǰ�̳߳�����
	std::atomic_char32_t curThreadSize_;
	// ��¼�߳��������ֵ
	int32_t threadSizeMaxHold_;

	//��������������
	int32_t taskQueMaxThreadHold_;

	// ��֤������е��̰߳�ȫ
	std::mutex taskQueMtx;
	// ��Ҫһ�������ź� 
	std::condition_variable notFull_;	//������в���
	std::condition_variable notEmpty_;  //������зǿ�
	std::condition_variable exitCond_;  // �ȴ��߳���Դȫ������ 
	//��¼��ǰ���̳߳�ģʽ
	ThreadPoolMode poolMode_;
	// ��ʾ��ǰ�̳߳ص�����״̬
	std::atomic_bool isPoolRunning_;
	//��¼�����̵߳�����
	std::atomic_int idleThreadSize_;
};



#endif