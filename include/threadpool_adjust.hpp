#ifndef __THREADPOOL_ADJUST_HPP__
#define __THREADPOOL_ADJUST_HPP__

#include <list>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>


#define DEFAULT_TIME 10                 /*10s检测一次*/
#define MIN_WAIT_TASK_NUM 10            /*如果queue_size > MIN_WAIT_TASK_NUM 添加新的线程到线程池*/ 
#define DEFAULT_THREAD_VARY 10          /*每次创建和销毁线程的个数*/
#define true 1
#define false 0

#ifdef USE_DEBUG
#define printf_DB(fmt, ...) printf(fmt, ##__VA_ARGS__)  // 定义一个宏PRINT，用于打印
#else
#define printf_DB(fmt, ...)  // 如果宏开关未定义，则将PRINT定义为空
#endif

extern void err_exit(const char *reason,bool ExitFlag);
/* 线程池，将它定义为模板类是为了代码复用。模板参数T是任务类 */
template<typename T>
class threadpool
{
public:
    /* 参数thread_number是线程池中线程的数量，max_requests是请求队列中
     * 最多允许的、等待处理的请求的数量 */
    threadpool();
    threadpool(int min_thr_num, int max_thr_num , int max_requests);
    ~threadpool();
    /* 往请求队列中添加任务 */
    bool append(T *request);

private:
    /* 工作线程运行的函数，它不断从工作队列中取出任务并执行之 */
    static void *worker(void *arg);         /*注意此处，此处有个C和C艹最大的不同,this指针，具体看收藏网页或者文档*/
    void run();
    /* 管理线程 */
    static void *manage(void *arg);
    void *adjust_thread();
    bool is_thread_alive(pthread_t tid);
private:
    int live_thr_num;                   /* 当前存活线程个数 */
    int min_thr_num;                    /* 线程池最小线程数 */
    int max_thr_num;                    /* 线程池最大线程数 */
    size_t m_max_requests;              /* 请求队列中允许的最大请求数 */
    int busy_thr_num;                   /* 忙状态线程个数 */
    int wait_exit_thr_num;              /* 要销毁的线程个数 */
    bool shutdown;        /* 是否结束线程 *//* 标志位，线程池使用状态，true或false */
    pthread_t *m_threads;   /* 描述线程池的数组，其大小为m_thread_number */  

    std::list<T *> m_workqueue;     /* 请求队列 */
    pthread_mutex_t lock;               /* 用于锁住本结构体 */    
    pthread_mutex_t lock_busy_counter;     /* 记录忙状态线程个数de琐 -- busy_thr_num */
    pthread_cond_t queue_not_full;      /* 当任务队列满时，添加任务的线程阻塞，等待此条件变量 */
    pthread_cond_t queue_not_empty;     /* 任务队列里不为空时，通知等待任务的线程 */
    pthread_t adjust_tid;               /* 存管理线程tid */ 
             
}; 



template<typename T>
threadpool<T>::threadpool():threadpool(8,50,10000)/*使用委托构造函数来避免重复初始化的代码*/
{

}


template<typename T>
threadpool<T>::threadpool(int min_thr_num, int max_thr_num , int max_requests): 
live_thr_num(min_thr_num),min_thr_num(min_thr_num),max_thr_num(max_thr_num),m_max_requests(max_requests), 
busy_thr_num(0),wait_exit_thr_num(0),shutdown(false), m_threads(NULL)
{
    do
    {
        if (live_thr_num <= 0 || max_requests <= 0)
        {
            err_exit("live_threads and max_requests Can't less than 0",true);
            break;
        }
        /* 根据最大线程上限数， 给工作线程数组开辟空间, 并清零 */
        m_threads = new pthread_t[max_thr_num];
        if (!m_threads)
        {
            delete[] m_threads;
            err_exit("threadpool init fail",true);
            break;
        }
        memset(m_threads, 0, sizeof(pthread_t)*max_thr_num);
        /* 初始化互斥琐、条件变量 */
        if (pthread_mutex_init(&lock, NULL) != 0 || pthread_mutex_init(&lock_busy_counter, NULL)!= 0 ||
                 pthread_cond_init(&(queue_not_empty), NULL) != 0 || pthread_cond_init(&(queue_not_full), NULL) != 0)
        {
            err_exit("init the lock or cond fail",true);
            break;
        }
        /* 创建min_thr_num个工作线程，并将它们都设置为脱离线程 */
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        for (int i = 0; i < min_thr_num; ++i)
        {
        
            /*在 C++ 程序中使用 pthread_create 函数时，该函数的第 3个参数必须指向一个静态函数。
            而要在一个静态函数中使用类的动态成员（包括成员函数和成员变量），则只能通过如下两种方式实现：
            通过类的静态对象来调用。将类的对象作为参数传递给该静态函数，然后在静态函数中引入这个对象，并调用其动态方法。*/
            if (pthread_create(m_threads + i, &attr, worker, this) != 0)         
            {
                delete[] m_threads;
                err_exit("pthread_create fail",true);
                break;
            }
        }
        //创建管理者线程
        pthread_create(&adjust_tid, &attr, manage, (void *)this);
    } while (0);
}




template<typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    pthread_mutex_destroy(&lock);
    pthread_mutex_destroy(&lock_busy_counter);
    pthread_cond_destroy(&queue_not_empty);
    pthread_cond_destroy(&queue_not_full);
    shutdown = true;
}


template<typename T>
bool threadpool<T>::append(T *request)/*这个函数是在主线程调用的,按理说不该阻塞住,若达到最大请求数,应返回false
                                        并应向客户端发送"请求过多请稍后再试"",但此时用第二种做法,用条件变量阻塞住,我觉得稍后处理比不处理强*/
{
    /* 操作工作队列一定要加锁，因为它被所有线程共享 */
     pthread_mutex_lock(&lock);
    /* >=为真，队列已经满， 调wait阻塞 */
    while ((m_workqueue.size() >= m_max_requests) && (!shutdown)) 
	{
        printf_DB("m_workqueue.size() >= m_max_requests,阻塞在此处\n");
        pthread_cond_wait(&queue_not_full, &lock);
    }
    if (shutdown) 
	{
        printf_DB("threadpool has shutdown\n");
        pthread_cond_broadcast(&queue_not_empty);
        pthread_mutex_unlock(&lock);
        return false;
    }
    m_workqueue.push_back(request);
    printf_DB("m_workqueue.size() %ld\n",m_workqueue.size());
    /*添加完任务后，队列不为空，唤醒线程池中 等待处理任务的线程*/
    pthread_cond_signal(&queue_not_empty);
    pthread_mutex_unlock(&lock);
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
void *threadpool<T>::manage(void *arg)  
{
    threadpool *pool = (threadpool *)arg;   /*arg是传进来的threadpool<T>类型的this指针*/
    pool->adjust_thread();
    return pool;                        /*为什么要return pool? 因为pthread_create要求第三个函数指针参数返回的是void*, void *(*start_routine) (void *)*/
}


template<typename T>
void threadpool<T>::run()
{
    while(!shutdown)
    {
        /*queue_size == 0 说明没有任务，调 wait 阻塞在条件变量上, 若有任务，跳过该while*/
        pthread_mutex_lock(&lock);
        while ((m_workqueue.size() == 0) && (!shutdown))    /*若工作队列为0且未关闭,则线程进去此while，表示空闲线程*/
		{  
            printf_DB("thread 0x%x is waiting\n", (unsigned int)pthread_self());
            pthread_cond_wait(&queue_not_empty, &lock);//暂停到这
            /*清除指定数目的空闲线程，如果要结束的线程个数大于0，结束线程*/
            if (wait_exit_thr_num > 0) 
			{
                pthread_mutex_lock(&lock);
                wait_exit_thr_num--;
                pthread_mutex_unlock(&lock);
                /*如果线程池里线程个数大于最小值时可以结束当前线程*/
                if (live_thr_num > min_thr_num) 
				{
                    printf_DB("thread 0x%x is exiting\n", (unsigned int)pthread_self());
                    pthread_mutex_lock(&lock);
                    live_thr_num--;
                    pthread_mutex_unlock(&lock);
                    pthread_exit(NULL);
                }
            }
        }

        /*如果指定了true，要关闭线程池里的每个线程，自行退出处理---销毁线程池*/
        if (shutdown) 
		{
            pthread_mutex_unlock(&lock);
            printf_DB("thread 0x%x is exiting,Reason:shutdown is true\n", (unsigned int)pthread_self());
            pthread_exit(NULL);     /* 线程自行结束 */
        }
        T *request = m_workqueue.front();
        /*注意此处,如果线程操作顺序不对,可能会出现段错误或者内存泄漏,如果出现最好先判断下m_workqueue.size()==0*/
        m_workqueue.pop_front();    
        // if(m_workqueue.size() == 0)
        // {
        // }else{
        //     m_workqueue.pop_front();
        // }
         /*通知可以有新的任务添加进来*/
        pthread_cond_broadcast(&queue_not_full);
        /*任务取出后，立即将 线程池锁 释放*/
        pthread_mutex_unlock(&lock);
        printf_DB("thread 0x%x start working\n", (unsigned int)pthread_self());
        if (!request)
        {
            continue;
        }
        pthread_mutex_lock(&lock_busy_counter);
        busy_thr_num++;                                     /*忙状态线程数+1*/
        pthread_mutex_unlock(&lock_busy_counter);                                    
        request->process();
        /*任务结束处理*/ 
        printf_DB("thread 0x%x end working\n", (unsigned int)pthread_self());
        pthread_mutex_lock(&lock_busy_counter);
        busy_thr_num--;                                       /*处理掉一个任务，忙状态数线程数-1*/
        pthread_mutex_unlock(&lock_busy_counter); 
    }
    pthread_exit(NULL);
}


/* 管理线程 */
template<typename T>
void* threadpool<T>::adjust_thread()
{
    pthread_mutex_t lock_adjust;   
    pthread_mutex_init(&lock, NULL);
    while (!shutdown) 
	{
        sleep(DEFAULT_TIME);                                    /*定时 对线程池管理*/
        /*这里锁住并新建变量存储的原因是想只关心这一刻的线程数量变化,防止因获取变量而不断阻塞其他工作线程*/  
        pthread_mutex_lock(&lock_adjust);
        int queue_size = m_workqueue.size();             /* 关注 任务数 */
        int liveThrNum = live_thr_num;                   /* 存活 线程数 */
        int busyThrNum =busy_thr_num;                    /* 忙着的线程数 */
        pthread_mutex_unlock(&lock_adjust);
        /* 创建新线程 算法： 任务数大于最小线程池个数, 且存活的线程数少于最大线程个数时 如：30>=10 && 40<100*/
        if (queue_size >= MIN_WAIT_TASK_NUM && liveThrNum <max_thr_num) 
		{
            pthread_mutex_lock(&lock_adjust);
            int add = 0;
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            /*一次增加 DEFAULT_THREAD 个线程*/
            for (int i = 0; i < max_thr_num && add < DEFAULT_THREAD_VARY && live_thr_num <max_thr_num; i++) 
			{
                if (m_threads[i] == 0 || !is_thread_alive(m_threads[i])) /*寻找空闲位置*/
				{
                    pthread_create(&m_threads[i], &attr, worker, (void *)this);/*设置分离属性*/
                    add++;
                    live_thr_num++;
                }
            }
            printf_DB("**add %d threads\n",add);
          pthread_mutex_unlock(&lock_adjust);
        }

        /* 销毁多余的空闲线程 算法：忙线程X2 小于 存活的线程数 且 存活的线程数 大于 最小线程数时*/
        if ((busyThrNum * 2) < liveThrNum  &&  liveThrNum >min_thr_num) 
		{
            /* 一次销毁DEFAULT_THREAD个线程, 隨機10個即可 */
            pthread_mutex_lock(&lock_adjust);
            wait_exit_thr_num = DEFAULT_THREAD_VARY;      /* 要销毁的线程数 设置为10,这样太鲁莽了,我觉得不如liveThrNum -min_thr_num */
           pthread_mutex_unlock(&lock_adjust);
            printf_DB("**destroy  %d threads\n",wait_exit_thr_num);
            for (int i = 0; i < DEFAULT_THREAD_VARY; i++) 
			{
                /* 通知处在空闲状态的线程, 他们会自行终止,其实这发送的是虚假信号,并没有请求插入队列*/
                pthread_cond_signal(&queue_not_empty);
            }
        }
    }
    pthread_mutex_destroy(&lock_adjust);
    return NULL;
}
template<typename T>
bool threadpool<T>::is_thread_alive(pthread_t tid)
{
    int kill_rc = pthread_kill(tid, 0);     //发0号信号，测试线程是否存活
    if (kill_rc == ESRCH) 
	{
        return false;
    }
    return true;
}

#endif