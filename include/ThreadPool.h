#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <iostream>

class ThreadPool {
public:
    // 构造函数：启动设定数量的打工人（线程）
    ThreadPool(size_t num_threads) : stop(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            // emplace_back 直接在容器尾部构造线程
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;

                    // 加锁去任务队列里拿任务
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        
                        // condition.wait 会阻塞当前线程，直到：
                        // 1. 线程池要停止了 (stop == true) 
                        // 2. 或者任务队列里有任务了 (!tasks.empty())
                        this->condition.wait(lock, [this] {
                            return this->stop || !this->tasks.empty();
                        });

                        // 如果线程池停止且任务队列空了，打工人就可以下班退出了
                        if (this->stop && this->tasks.empty()) {
                            return;
                        }

                        // 从队列里抢到了一个任务
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }

                    // 释放锁之后，开始干活！(执行任务)
                    task(); 
                }
            });
        }
        std::cout << "🚀 ThreadPool initialized with " << num_threads << " worker threads." << std::endl;
    }

    // 析构函数：优雅地关闭所有线程
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all(); // 唤醒所有正在睡觉的打工人，告诉他们要下班了
        
        // 等待所有打工人干完手头的活并退出
        for (std::thread &worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    // 老板派发任务的接口
    void enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            tasks.emplace(task); // 把任务塞进队列
        }
        condition.notify_one(); // 随便叫醒一个在睡觉的打工人来干活
    }

private:
    std::vector<std::thread> workers;            // 存放打工人的数组
    std::queue<std::function<void()>> tasks;     // 任务队列

    std::mutex queue_mutex;                      // 保护任务队列的互斥锁
    std::condition_variable condition;           // 条件变量（用来叫醒睡觉的打工人）
    bool stop;                                   // 线程池是否停止的标志
};