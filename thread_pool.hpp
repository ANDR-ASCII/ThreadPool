#ifndef THREAD_POOL_HPP_INCLUDED
#define THREAD_POOL_HPP_INCLUDED

#include <condition_variable>
#include <future>
#include <thread>
#include <mutex>
#include <queue>
#include <map>
#include <stddef.h> /** for type size_t which out of namespace std */

namespace black_box
{
//--------------------------------------------------------------

template <typename ReturnT>
class thread_pool
{
    /** DATA AND INTERNAL DATA TYPES **/

    // releases only move semantic
    struct task_wrapper
    {
        //-------------------------------------------------------------------
        std::packaged_task<ReturnT()> task_;
        bool complete_signal_;
        //-------------------------------------------------------------------
    };

    // releases only move semantic
    struct thread_synchronization
    {
        //-------------------------------------------------------------------
        std::condition_variable condition_;
        std::mutex mutable mutex_;
        std::queue<task_wrapper> queue_;
        //-------------------------------------------------------------------
    };

    size_t                                          threads_number_;
    std::vector <std::thread>                       threads_;
    std::map    <size_t, thread_synchronization>    threads_synchronize_;
    /**^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^**/
    /**^^^^^^^^^^id of thread and synchronization variables^^^^^^^^^^^**/

    /** id == 0:
    *
    *   threads_[0] - thread
    *   threads_synchronize_[0] - its own synchronization entities
    *
    **/

public:

    typedef std::shared_future<ReturnT> result_type;

    thread_pool() : thread_pool{ std::thread::hardware_concurrency() }
        { }

    thread_pool(size_t threads_n):
        threads_number_{ threads_n },
        threads_{ threads_number_ }
    {
        for(size_t i = 0; i < threads_number_; ++i)
        {
            threads_synchronize_[i];
            threads_[i] = std::thread{ &thread_pool::task_handler, this, i };
        }
    }

    ~thread_pool()
    {
        for(size_t i = 0; i < threads_number_; ++i)
        {
            complete_thread(i);

            if(threads_[i].joinable())
            {
                threads_[i].join();
            }
        }
    }

    template <typename F, typename... Args>
    result_type add_task(F f, Args&&... params)
    {
        task_wrapper new_task = binder(f, std::forward<Args>(params)...);
        result_type future_object = new_task.task_.get_future();
        thread_synchronization& ts = find_minimum_load_queue();

        std::unique_lock<std::mutex> locker{ ts.mutex_ };
        put_task(ts, std::move(new_task), std::defer_lock);
        ts.condition_.notify_one();
        locker.unlock();

        return future_object;
    }

private:

    /** executes passed tasks */
    void task_handler(size_t id)
    {
        for(;;)
        {
            //-------------------------------------------------------------------
            thread_synchronization& ts = threads_synchronize_[id];
            std::unique_lock<std::mutex> locker{ ts.mutex_ };
            //-----------------------------------------------------------------
            ts.condition_.wait(locker, [&ts]{ return !ts.queue_.empty(); });
            auto callable = get_task(ts, std::defer_lock);
            //-----------------------------------------------------------------
            locker.unlock();

            // first check
            // if thread was slept and received the complete signal
            if(callable.second.complete_signal_)
            {
                break;
            }

            // for correct exit from loop
            bool double_check_complete = false;

            // executes all tasks from queue
            for(;;)
            {
                if(callable.first == false)
                {
                    break;
                }

                // double check
                // if this thread works and gets the complete signal
                // complete signal will be sent without valid packaged_task
                // and if that task to attempt to execute
                // the future_error exception will be thrown
                if(callable.second.complete_signal_)
                {
                    double_check_complete = true;
                    break;
                }

                callable.second.task_();
                callable = get_task(ts);
            }

            // for correct exit from loop
            if(double_check_complete)
            {
                break;
            }
        }
    }

    // send complete signal for specified thread
    void complete_thread(size_t id)
    {
        thread_synchronization& ts = threads_synchronize_[id];

        task_wrapper cs{ {}, true };
        put_task(ts, std::move(cs));

        ts.condition_.notify_one();
    }

    /** ATOMIC OPERATIONS **/
    //*******************************************************************************
    // pops the task with blocking
    std::pair<bool, task_wrapper> get_task(thread_synchronization& ts)
    {
        std::lock_guard<std::mutex> locker{ ts.mutex_ };
        bool success_flag = ts.queue_.size();
        task_wrapper tw{ {}, false };

        if(success_flag)
        {
            tw = std::move(ts.queue_.front());
            ts.queue_.pop();
        }

        return std::make_pair(success_flag, std::move(tw));
    }

    // pops the task without blocking
    std::pair<bool, task_wrapper> get_task(thread_synchronization& ts, std::defer_lock_t strategy)
    {
        (void)strategy;

        bool success_flag = ts.queue_.size();
        task_wrapper tw{ {}, false };

        if(success_flag)
        {
            tw = std::move(ts.queue_.front());
            ts.queue_.pop();
        }

        return std::make_pair(success_flag, std::move(tw));
    }

    void put_task(thread_synchronization& ts, task_wrapper tw)
    {
        std::lock_guard<std::mutex> locker{ ts.mutex_ };
        ts.queue_.push(std::move(tw));
    }

    void put_task(thread_synchronization& ts, task_wrapper tw, std::defer_lock_t strategy)
    {
        (void)strategy;
        ts.queue_.push(std::move(tw));
    }
    //*******************************************************************************

    /** HELPER FUNCTIONS **/

    template <typename F, typename... Args>
    task_wrapper binder(F f, Args&&... params)
    {
        //-------------------------------------------------------------------
        auto new_callable_object = std::bind(f, std::forward<Args>(params)...);
        task_wrapper new_wrapper{ std::packaged_task<ReturnT()>{new_callable_object}, false };
        //-------------------------------------------------------------------
        return new_wrapper;
    }

    thread_synchronization& find_minimum_load_queue()
    {
        threads_synchronize_[0].mutex_.lock();
        auto smallest = threads_synchronize_[0].queue_.size();
        threads_synchronize_[0].mutex_.unlock();

        size_t min_elem = 0;

        for(size_t i = 1; i < threads_number_; ++i)
        {
            std::lock_guard<std::mutex> locker{ threads_synchronize_[i].mutex_ };
            if(threads_synchronize_[i].queue_.size() < smallest)
            {
                smallest = threads_synchronize_[i].queue_.size();
                min_elem = i;
            }
        }

        return threads_synchronize_[min_elem];
    }
};
//--------------------------------------------------------------
} // end of namespace BlackBox

#endif // THREAD_POOL_HPP_INCLUDED
