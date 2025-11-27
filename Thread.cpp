#include "Thread.h"

// 定义静态成员变量
// 说明：
// 1. 静态成员变量必须在类外定义（C++ 规则）
// 2. 这会为 m_mapThread 分配内存
// 3. 所有 CThread 对象共享这一个 map
std::map<pthread_t, CThread*> CThread::m_mapThread;
