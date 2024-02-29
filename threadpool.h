#ifndef __THREADPOOL_H
#define __THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"     /*第十四章介绍的线程同步机制的包装类*/
/* 线程池，将它定义为模板类是为了代码复用。模板参数T是任务类 */
template<typename T>
class threadpool
{
public:
    /* 参数thread_number是线程池中线程的数量，max_requests是请求队列中
     * 最多允许的、等待处理的请求的数量 */
    threadpool(int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    /* 往请求队列中添加任务 */
    bool append(T *request);

private:
    /* 工作线程运行的函数，它不断从工作队列中取出任务并执行之 */
    static void *worker(void *arg);         /*注意此处，此处有个C和C艹最大的不同,this指针，具体看游双书或者文档*/
    void run();

private:
    int m_thread_number;    /* 线程池中的线程数 */
    int m_max_requests;     /* 请求队列中允许的最大请求数 */
    pthread_t *m_threads;   /* 描述线程池的数组，其大小为m_thread_number */
    std::list<T *> m_workqueue;     /* 请求队列 */
    locker m_queuelocker;   /* 保护请求队列的互斥锁 */
    sem m_queuestat;    /* 是否有任务需要处理 */
    bool m_stop;        /* 是否结束线程 */
};

template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests): m_thread_number(thread_number),
    m_max_requests(max_requests), m_stop(false), m_threads(NULL)
{
    if (thread_number <= 0 || max_requests <= 0)
    {
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
    {
        throw std::exception();
    }

    /* 创建thread_number个线程，并将它们都设置为脱离线程 */
    for (int i = 0; i < thread_number; ++i)
    {
       
            /*在 C++ 程序中使用 pthread_create 函数时，该函数的第 3个参数必须指向一个静态函数。
            而要在一个静态函数中使用类的动态成员（包括成员函数和成员变量），则只能通过如下两种方式实现：
            通过类的静态对象来调用。将类的对象作为参数传递给该静态函数，然后在静态函数中引入这个对象，并调用其动态方法。*/
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)         
        {
            delete[] m_threads;
            throw std::exception();
        }
        printf("create the %dth thread,tid %ld\n", i,*m_threads);  
        if (pthread_detach(m_threads[i]))  //脱离
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop = true;
}

template<typename T>
bool threadpool<T>::append(T *request)
{
    /* 操作工作队列一定要加锁，因为它被所有线程共享 */
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }

    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();

    return true;
}

template<typename T>
void *threadpool<T>::worker(void *arg)  /*声明时使用static void *worker(void *arg)就行，定义时不用static。*/
{
    threadpool *pool = (threadpool *)arg;   /*arg是传进来的threadpool<T>类型的this指针*/
    pool->run();
    return pool;                        /*为什么要return pool? 因为pthread_create要求第三个函数指针参数返回的是void*, void *(*start_routine) (void *)*/
}

template<typename T>
void threadpool<T>::run()
{
    while(!m_stop)
    {
        printf("block here %ld\n",pthread_self());
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        printf("\nthe thread is used ,tidblock  %ld\n",pthread_self());
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request)
        {
            continue;
        }
        request->process();
    }
}

#endif
