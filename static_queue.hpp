#ifndef STATIC_QUEUE_HPP_INCLUDED
#define STATIC_QUEUE_HPP_INCLUDED

#include <type_traits>
#include <memory>

namespace black_box
{
//=========================================================================================
template <typename T, size_t SIZE>
class static_queue
{
    // FIFO model

    T internalArray_[SIZE];
    size_t first_;
    size_t last_;
    size_t count_;

public:

    typedef T value_type;
    typedef T& reference;
    typedef T const& const_reference;


    //**********************************************************************

    class iterator
    {
        size_t desiredId_;
        size_t realIdElement_;
        static_queue& aquireObject_;

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
        iterator(static_queue& q, size_t id):
            desiredId_{id},
            realIdElement_{},
            aquireObject_{q}
        {
            SetValue(id);
        }

        //################################################################

        reference operator*()
        {
            return aquireObject_.internalArray_[realIdElement_];
        }

        reference operator->()
        {
            return aquireObject_.internalArray_[realIdElement_];
        }

        const_reference operator*() const
        {
            return aquireObject_.internalArray_[realIdElement_];
        }

        const_reference operator->() const
        {
            return aquireObject_.internalArray_[realIdElement_];
        }

        //################################################################

        friend bool operator==(iterator const& lhs, iterator const& rhs)
        {
            return lhs.desiredId_ == rhs.desiredId_ &&
                    &lhs.aquireObject_ == &rhs.aquireObject_;
        }

        friend bool operator!=(iterator const& lhs, iterator const& rhs)
        {
            return !(lhs == rhs);
        }

        iterator& operator++()
        {
            SetValue(desiredId_ + 1);
            return *this;
        }

        iterator& operator++(int)
        {
            auto thisObject = *this;
            SetValue(desiredId_ + 1);
            return thisObject;
        }

        iterator& operator--()
        {
            SetValue(desiredId_ - 1);
            return *this;
        }

        iterator& operator--(int)
        {
            auto thisObject = *this;
            SetValue(desiredId_ - 1);
            return thisObject;
        }
    };

    static_queue(): first_{0}, last_{0}, count_{0} { }

    // copy/move semantics depends by T
    static_queue(static_queue const&)             = default;
    static_queue(static_queue&&)                  = default;
    static_queue& operator=(static_queue const&)  = default;
    static_queue& operator=(static_queue&&)       = default;
    // the class can be inherited
    virtual ~static_queue()                      = default;

    iterator        begin  ()          { return iterator(*this, 0); }
    iterator const  begin  ()  const   { return iterator(*this, 0); }
    iterator const  cbegin ()  const   { return iterator(*this, 0); }
    iterator        end    ()          { return iterator(*this, count_); }
    iterator const  end    ()  const   { return iterator(*this, count_); }
    iterator const  cend   ()  const   { return iterator(*this, count_); }

    //**********************************************************************

    // copy assignable version
    bool push(const_reference lobject)
        noexcept(std::is_nothrow_copy_assignable<value_type>::value)
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
    bool push(value_type&& robject)
         noexcept(std::is_nothrow_move_assignable<value_type>::value)
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

    bool pop(reference value)
        noexcept(std::is_nothrow_copy_assignable<value_type>::value)
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

    std::shared_ptr<value_type> pop()
        noexcept(std::is_nothrow_copy_assignable<T>::value)
    {
        if(count_)
        {
            std::shared_ptr<value_type> temporary =
                std::make_shared<value_type>(std::move_if_noexcept(internalArray_[first_]));

            first_ = (first_ + 1) % SIZE;
            --count_;
            return temporary;
        }

        return std::shared_ptr<value_type>{ nullptr };
    }

    bool empty() const
    {
        return !count_;
    }

    bool full() const
    {
        return count_ == SIZE;
    }

    size_t size() const
    {
        return count_;
    }
};
//=========================================================================================
} // end of namespace black_box


#endif // STATIC_QUEUE_HPP_INCLUDED
