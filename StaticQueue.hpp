#ifndef STATICQUEUE_HPP_INCLUDED
#define STATICQUEUE_HPP_INCLUDED

#include <type_traits>
#include <memory>

namespace BlackBox
{
//=========================================================================================
template <typename T, size_t SIZE>
class StaticQueue
{
    // FIFO model

    T internalArray_[SIZE];
    size_t first_;
    size_t last_;
    size_t count_;

public:

    //**********************************************************************

    class Iterator
    {
        size_t desiredId_;
        size_t realIdElement_;
        StaticQueue& aquireObject_;

        void SetValue(size_t desired)
        {
            desiredId_ = desired;
            realIdElement_ = aquireObject_.first_ + desiredId_;
            if(realIdElement_ >= SIZE)
            {
                realIdElement_ %= SIZE;
            }
        }

    public:

        Iterator(StaticQueue& q, size_t id):
            desiredId_{id},
            realIdElement_{},
            aquireObject_{q}
        {
            SetValue(id);
        }

        //################################################################

        T& operator*()
        {
            return aquireObject_.internalArray_[realIdElement_];
        }

        T& operator->()
        {
            return aquireObject_.internalArray_[realIdElement_];
        }

        T const& operator*() const
        {
            return aquireObject_.internalArray_[realIdElement_];
        }

        T const& operator->() const
        {
            return aquireObject_.internalArray_[realIdElement_];
        }

        //################################################################

        friend bool operator==(Iterator const& lhs, Iterator const& rhs)
        {
            return lhs.desiredId_ == rhs.desiredId_ &&
                    &lhs.aquireObject_ == &rhs.aquireObject_;
        }

        friend bool operator!=(Iterator const& lhs, Iterator const& rhs)
        {
            return !(lhs == rhs);
        }

        Iterator& operator++()
        {
            SetValue(desiredId_ + 1);
            return *this;
        }

        Iterator& operator++(int)
        {
            auto thisObject = *this;
            SetValue(desiredId_ + 1);
            return thisObject;
        }

        Iterator& operator--()
        {
            SetValue(desiredId_ - 1);
            return *this;
        }

        Iterator& operator--(int)
        {
            auto thisObject = *this;
            SetValue(desiredId_ - 1);
            return thisObject;
        }
    };

    StaticQueue(): first_{0}, last_{0}, count_{0} { }

    // copy/move semantics depends by T
    StaticQueue(StaticQueue const&)             = default;
    StaticQueue(StaticQueue&&)                  = default;
    StaticQueue& operator=(StaticQueue const&)  = default;
    StaticQueue& operator=(StaticQueue&&)       = default;
    // the class can be inherited
    virtual ~StaticQueue()                      = default;

    Iterator        begin  ()          { return Iterator(*this, 0); }
    Iterator const  begin  ()  const   { return Iterator(*this, 0); }
    Iterator const  cbegin ()  const   { return Iterator(*this, 0); }
    Iterator        end    ()          { return Iterator(*this, count_); }
    Iterator const  end    ()  const   { return Iterator(*this, count_); }
    Iterator const  cend   ()  const   { return Iterator(*this, count_); }

    //**********************************************************************

    // copy assignable version
    bool Push(T const& lobject) volatile
        noexcept(std::is_nothrow_copy_assignable<T>::value)
    {
        if(count_ < SIZE)
        {
            internalArray_[last_] = lobject;

            last_ = (last_ + 1) % SIZE;
            ++count_;

            return true;
        }

        return false;
    }

    // move assignable version
    // if T is not move assignable then called copy assignable
    // if T is not copy assignable then will be error
    bool Push(T&& robject)
         noexcept(std::is_nothrow_move_assignable<T>::value)
    {
        if(count_ < SIZE)
        {
            internalArray_[last_] = std::move(robject);

            last_ = (last_ + 1) % SIZE;
            ++count_;

            return true;
        }

        return false;
    }

    bool Pop(T& value)
        noexcept(std::is_nothrow_copy_assignable<T>::value)
    {
        if(count_)
        {
            // move or copy assign
            value = std::move_if_noexcept(internalArray_[first_]);

            first_ = (first_ + 1) % SIZE;
            --count_;
            return true;
        }

        return false;
    }

    std::shared_ptr<T> Pop()
        noexcept(std::is_nothrow_copy_assignable<T>::value)
    {
        if(count_)
        {
            std::shared_ptr<T> temporary =
                std::make_shared<T>(std::move_if_noexcept(internalArray_[first_]));

            first_ = (first_ + 1) % SIZE;
            --count_;
            return temporary;
        }

        return std::shared_ptr<T>{nullptr};
    }

    bool Empty() const
    {
        return !count_;
    }

    bool Filled() const
    {
        return count_ == SIZE;
    }

    size_t Size() const
    {
        return count_;
    }
};
//=========================================================================================
} // end of namespace GeneticAlgorithm


#endif // STATICQUEUE_HPP_INCLUDED
