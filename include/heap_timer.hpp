#ifndef __HEAP_TIMER_H_
#define __HEAP_TIMER_H_

#include <iostream>
#include "timer_common.hpp"

#define HEAP_DEFAULT_SIZE 128

// 定时器数据结构的定义
template <typename _User_Data>
class HeapTimer
{
public:
    HeapTimer() = default;
    HeapTimer(int msec)
    {
        timer.setTimeout(msec);
    }

    ~HeapTimer()
    {

    }

    void setTimeout(time_t timeout)
    {
        timer.setTimeout(timeout);
    }

    time_t getExpire()
    {
        return timer.getExpire();
    }

    void setUserData(_User_Data *userData)
    {
        timer.setUserData(userData);
    }

    int getPos()
    {
        return _pos;
    }

    void setPos(int pos)
    {
        this->_pos = pos;
    }

    void handleTimeOut()
    {
        timer.handleTimeOut();    
    }

    using TimeOutCbFunc = void (*)(_User_Data *);
    void setCallBack(TimeOutCbFunc callBack)
    {
        timer.setCallBack(callBack);
    }

public:
    Timer<_User_Data> timer;  

private:
      
    int _pos;                          // 保存该定时器在数组中的位置，以便查找删除操作            
};


// 定时容器，使用最小堆实现
template <typename _UData>
class HeapTimerContainer : public ITimerContainer<_UData> 
{
public:
    HeapTimerContainer();
    HeapTimerContainer(int capacity);
    HeapTimerContainer(HeapTimer<_UData> **initArray, int arrSize, int capacity);
    virtual ~HeapTimerContainer() override;

public:
    virtual void tick() override;               
    Timer<_UData> *addTimer(time_t timeout)  override;
    void delTimer(Timer<_UData> *timer)  override;
    void resetTimer(Timer<_UData> *timer, time_t timeout)  override;
    time_t getMinExpire() override;
    Timer<_UData> *top();
    void popTimer();

private:
    void percolateDown(int hole);
    void percolateUp(int hole);
    void resize();
    bool isEmpty();

private:
    HeapTimer<_UData> **_array;         /*  _array存放的是地址的地址，解引用 *_array一级指针指向指针数组里面的指针，**_array二级指针指向指针数组里面的指针指向的值*/     
                                        /*堆数据,二级指针，接的数是指针的指针，比如后面的new一个指针数组，new会返回指针数组的首地址。要记住指针数组里面存的是地址，是地址，是地址。
                                        如果要使用指针数组里面的指针指向的值,也就是数组里面指针指向的数值，需要**_array(0)=5,这是使用二级指针处理指针数组的第0个成员指针指向的值赋值为5。
                                        如果要把指针数组里面的成员替换成其他的，也就是其他指针换成新的指针，需要*_arrary=&p,*_arrary(0)=arrary[0]这是使用二级指针处理指针数组的第0个成员指针换成*指针*/
    int _capacity;                           // 堆数组的容量
    int _size;                               // 当前包含的元素
};

template <typename _UData>
HeapTimerContainer<_UData>::HeapTimerContainer() : HeapTimerContainer(HEAP_DEFAULT_SIZE)/*使用委托构造函数来避免重复初始化的代码*/
{

}

/*用 HeapTimerContainer<_UData>::HeapTimerContainer() 这个构造函数时，不需要传入参数。这个构造函数是一个无参构造函数，
    并且在内部调用了带有参数的构造函数 HeapTimerContainer<_UData>::HeapTimerContainer(int capacity)，默认使用预定义的 HEAP_DEFAULT_SIZE 作为参数进行初始化。
    因此，你可以直接通过 HeapTimerContainer<_UData> container; 的方式来创建一个 HeapTimerContainer 对象，并且会自动使用默认值进行初始化。*/

// 初始化一个大小为cap的空堆
template <typename _UData>
HeapTimerContainer<_UData>::HeapTimerContainer(int capacity)
{
    this->_capacity = capacity;
    this->_size = 0;

    _array = new HeapTimer<_UData> *[capacity]{nullptr};
}

// 用已有数组来初始化堆 
template <typename _UData>
HeapTimerContainer<_UData>::HeapTimerContainer(HeapTimer<_UData> **initArray, int arrSize, int capacity) :
    _size(arrSize)
{
    if(capacity < arrSize) 
    {
        this->_capacity = capacity = 2 * arrSize;
    }

    _array = new HeapTimer<_UData> *[capacity];/*二级指针指向指针数组的首地址*/
    for (int i = 0; i < capacity; i++)
    {
        _array[i] = nullptr;
    }

    if(arrSize > 0) 
    {
        for (int i = 0; i < arrSize; i++)
        {
           _array[i] = initArray[i]; 
        }
        
        for(int i = (_size - 1) / 2; i >= 0; i--)
        {
            percolateDown(i);       //对数组中的第(_size - 1) / 2 ~ 0个元素执行下滤操作
        }
    }
    
}

template <typename _UData>
HeapTimerContainer<_UData>::~HeapTimerContainer()
{
    if(_array)
    {
        for(int i = 0; i < _size; i++) 
        {
            delete _array[i];
        }
        delete []_array;
    }
    /*需要注意的是，释放指针数组内存时，也需要遍历数组元素，逐个释放每个指针指向的内存，然后再释放指针数组本身的内存。*/
}

template <typename _UData>
void HeapTimerContainer<_UData>::tick()
{
//    std::cout << "----------tick----------" << std::endl;
    HeapTimer<_UData> *tmp = _array[0];
    time_t cur = getMSec();
    // 循环处理到期的定时器
    while(!isEmpty())
    {
        if(!tmp)
        {
            break;
        }
        // 如果定时器没到期，则退出循环
        if(tmp->getExpire() > cur)
        {
            break;
        }
         
        tmp->handleTimeOut();
        // 将堆顶元素删除，同时生成新的堆顶定时器
        popTimer();
        tmp = _array[0];
    }
}

// 获取一个定时器
template <typename _UData>
Timer<_UData> *HeapTimerContainer<_UData>::addTimer(time_t timeout)
{
    if(_size >= _capacity)
    {
        this->resize();             //如果容量不够，则进行扩容
    }

    // hole是新建空穴的位置
    int hole = _size++;
    HeapTimer<_UData> *timer = new HeapTimer<_UData>(timeout);
    _array[hole] = timer;           /*指针数组里面的指针用timer赋值。如果要把指针数组里面的成员替换成其他的，也就是其他指针换成新的指针，需要*_arrary=&p即*_arrary=arrary[0]这是使用二级指针处理指针数组的第0个成员指针换成*指针*/

    percolateUp(hole);

    return &timer->timer;/*&(timer->timer)*/
}

// 删除目标定时器
template <typename _UData>
void HeapTimerContainer<_UData>::delTimer(Timer<_UData> *timer)
{
    if(!timer) 
    {
        return ;
    }

    /* 仅仅将目标定时器的数据设置为空，延迟销毁
       等定时器超时再删除该定时器
     */
    timer->setCallBack(nullptr);
    timer->setUserData(nullptr);

}

// 重置一个定时器
template <typename _UData>
void HeapTimerContainer<_UData>::resetTimer(Timer<_UData> *timer, time_t timeout)
{
/*类型强转,reinterpret_cast可以通过成员访问到完整结构体对象。首先，timer是HeapTimer的公共成员，且resetTimer是HeapTimer成员函数，所以可以强转
 可以把参数Timer<_UData> *timer当成HeapTimer里面的成员。因为成员变量和成员函数是通过本身就是通过this指针联系的。
                                                                其实我还是不理解，暂且这样解释2023年12月20日*/

/*其实观察Timer<_UData> *HeapTimerContainer<_UData>::addTimer(time_t timeout)函数可以看到new了一个heaptimer,但是返回的是heaptimer->timer。
在resetTimer也就此函数里面，传递的参数正是这个heaptimer->timer,于是通过reinterpret_cast强转HeapTimer<_UData>*之后，
就会把timer前后的内容包含在内，因为timer前后的内容在内存中本来就属于heaptimer,而在经过上滤,下沉后,pos处就有了数据。
有点像Linux的那个天下第一宏contain_of,通过成员知道结构体首地址      新的解释--2024年2月24日15点41分*/
    HeapTimer<_UData> *htimer = reinterpret_cast< HeapTimer<_UData>* >(timer);

    /*reinterpret_cast 可以让程序员修改一个指针或者变量的类型，通常用于底层的指针和内存操作中。
    可以把一个指针转换成一个整数，或者把一个整数转换成指针。对于 reinterpret_cast 操作而言，并没有进行任何的类型检查，
    因此很容易发生由于指针类型不匹配而引发的内存错误，这就要求程序员自己保证转换的正确性。因此 reinterpret_cast 是一种比较危险的类型转换，适用场景非常有限。*/

    // 找到该定时器在数组中的位置，将其与最后一个定时器的位置交换，然后先进行下滤操作，再进行上滤操作
    int pos = htimer->getPos();
    int lastPos = _size - 1;
    if(pos != lastPos)
    {
        HeapTimer<_UData> *temp = _array[pos];
        _array[pos] = _array[lastPos];
        _array[lastPos] = temp;
    }
    timer->setTimeout(timeout);

    // 下滤 上滤
    percolateDown(pos);
    percolateUp(lastPos);

}

// 获取容器中超时值最小的值
template <typename _UData>
time_t HeapTimerContainer<_UData>::getMinExpire()
{
    Timer<_UData> * timer = top();
    if(timer)
    {
        return timer->getExpire();
    }

    return -1;
}

// 获得顶部的定时器
template <typename _UData>
Timer<_UData> *HeapTimerContainer<_UData>::top()
{
    if(isEmpty())
    {
        return nullptr;
    }

    return &_array[0]->timer;
}

template <typename _UData>
void HeapTimerContainer<_UData>::popTimer()
{
    if(isEmpty())
    {
        return;
    }

    if(_array[0])
    {
        delete _array[0];
        // 将原来的堆顶元素替换为堆数组中最后一个元素
        _array[0] = _array[--_size];
        // 对新的堆顶元素执行下滤操作
        percolateDown(0);
    }
}

// 最小堆的下滤操作，它确保数组中以第hole个节点作为根的子树拥有最小堆性质
template <typename _UData>
void HeapTimerContainer<_UData>::percolateDown(int hole)
{
    if(_size == 0)
    {
        return ;
    }
    HeapTimer<_UData> *temp = _array[hole];
    int child = 0;
    for(; ((hole * 2 + 1) <= _size - 1); hole = child)
    {
        child = hole * 2 + 1;
        if((child < (_size - 1)) && (_array[child + 1]->getExpire() < _array[child]->getExpire()))
        {
            child++;
        }

        if(_array[child]->getExpire() < temp->getExpire())
        {
            _array[hole] = _array[child];
            _array[hole]->setPos(hole);             // 调整定时器的位置时，重新设置timer中pos保存的其在数组中的位置
        }
        else 
        {
            break;
        }
    }

    _array[hole] = temp;
    _array[hole]->setPos(hole);
}

template <typename _UData>
void HeapTimerContainer<_UData>::percolateUp(int hole)
{
    int parent = 0;
    HeapTimer<_UData> *temp = _array[hole];

    // 对从空穴到根节点的路径上的所有节点执行上滤操作
    for(; hole > 0; hole = parent)
    {
        parent = (hole - 1) / 2;
        // 将新插入节点的超时值与父节点比较，如果父节点的值小于等于该节点的值，那么就无需再调整了。否则将父节点下移，继续这个操作。
        if(_array[parent]->getExpire() <= temp->getExpire())   
        {
            break;
        }
        _array[hole] = _array[parent];
        _array[hole]->setPos(hole);
    }

    _array[hole] = temp;
    _array[hole]->setPos(hole);
    
}

// 将数组的容量扩大一倍
template <typename _UData>
void HeapTimerContainer<_UData>::resize()
{
    HeapTimer<_UData> **temp = new HeapTimer<_UData> *[2 * _capacity];
    _capacity = 2 * _capacity;
    

    for(int i = 0; i < _size; i++)
    {
        temp[i] = _array[i];
    }

    for(int i = _size; i < _capacity; i++)
    {
        temp[i] = nullptr;
    }

    delete []_array;
    _array = temp;
}

template <typename _UData>
bool HeapTimerContainer<_UData>::isEmpty()
{
    return _size == 0;
}

#endif
