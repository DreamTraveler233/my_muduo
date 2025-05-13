//
// Created by shuzeyong on 2025/4/30.
//

#ifndef MY_MUDUO_NONCOPYABLE_H
#define MY_MUDUO_NONCOPYABLE_H

namespace net
{ /**
 * noncopyable被继承以后，派生类对象能正常的构造与析构，但是不能进行拷贝构造与赋值
 */
    class NonCopyable
    {
    public:
        NonCopyable(const NonCopyable &) = delete;
        void operator=(const NonCopyable &) = delete;

    protected:
        NonCopyable() = default;
        ~NonCopyable() = default;
    };
}

#endif//MY_MUDUO_NONCOPYABLE_H
