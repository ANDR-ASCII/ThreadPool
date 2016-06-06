#ifndef THREADPOOL_HPP_INCLUDED
#define THREADPOOL_HPP_INCLUDED

#include <condition_variable>
#include <iostream>
#include <vector>
#include <future>
#include <thread>
#include <mutex>
#include <queue>
#include <map>
#include "StaticQueue.hpp"

namespace BlackBox
{

using namespace std::literals::chrono_literals;

//=========================================================================================
template <typename ReturnT>
class ThreadPool
{
    //###########################################################################

    size_t concurrency_;
    std::vector<std::thread> threads_;
    StaticQueue<std::packaged_task<ReturnT()>, 32> queue_;

    // synchronization
    // {
            std::condition_variable condition_;
            std::condition_variable threadReady_;
            std::condition_variable conveyerCondition_;
            std::future<void> asyncAdderWaiter_;

            std::mutex conveyerMutex_;
            std::mutex internalMutex_;              // for queue_
            std::mutex modifyReadyFlag_;            // for readyFlags_

            std::map<std::thread::id, bool> readyFlags_;
            std::queue<std::packaged_task<ReturnT()>> conveyer_;

            bool exitflag_;
            bool doAdd_;
    // }

    //###########################################################################

    void Runnable()
    {
        while(true)
        {
            std::unique_lock<std::mutex> locker{internalMutex_};
            condition_.wait(locker, [this]{ return !queue_.Empty() || exitflag_; });

            if(exitflag_)
            {
                break;
            }

            auto task = queue_.Pop();
            locker.unlock();

            {
                // this thread is busy

                std::lock_guard<std::mutex> flagLocker{modifyReadyFlag_};
                readyFlags_[std::this_thread::get_id()] = false;
            }

            (*task)();

            {
                // this thread is ready to execute tasks

                std::lock_guard<std::mutex> flagLocker{modifyReadyFlag_};
                readyFlags_[std::this_thread::get_id()] = true;
                threadReady_.notify_all();
            }
        }
    }

    void AsyncAdder()
    {
        while(true)
        {
            std::unique_lock<std::mutex> conveyerLocker{conveyerMutex_};
            conveyerCondition_.wait(conveyerLocker,
                                    [this]{ return !conveyer_.empty() || !doAdd_; });

            if(doAdd_ == false)
            {
                break;
            }

            auto task = std::move(conveyer_.front());
            conveyer_.pop();
            conveyerLocker.unlock();

            // true if someone thread ready and queue not filled
            auto compare =
            [this]
            {
                return std::find_if(std::begin(readyFlags_), std::end(readyFlags_),
                        [](std::pair<std::thread::id, bool> const& p)
                        {
                            return p.second;
                        }) != std::end(readyFlags_) && !queue_.Filled();
            };

            std::unique_lock<std::mutex> flagLocker{modifyReadyFlag_};
            threadReady_.wait(flagLocker, compare);
            flagLocker.unlock();

            std::unique_lock<std::mutex> locker{internalMutex_};
            queue_.Push(std::move(task));
            locker.unlock();
            condition_.notify_one();
        }
    }

public:
    ThreadPool(size_t concurrency) :
        concurrency_{concurrency},
        threads_{concurrency_},
        exitflag_{false},
        doAdd_{true}
    {
        asyncAdderWaiter_
            = std::async(std::launch::async, &ThreadPool::AsyncAdder, this);

        // Run threads and suspend their
        for(auto& th : threads_)
        {
            th = std::thread{&ThreadPool::Runnable, this};
            readyFlags_[th.get_id()] = true;
            th.detach();
        }
    }

    /********************************************//**
     * \brief Add someone task to queue for execute
     *
     * \param func Some callable object (static function or function-member)
     * \param pack Parameter pack for callable object
     *
     * \return Returns the shared future object
     *           associated with the return-type of passed callable object
     *
     ***********************************************/

    template <typename F, typename... Args>
    std::shared_future<ReturnT> AddTask(F func, Args&&... pack)
    {
        // TODO:
        // 1) To add the ability of transfer a function-member

        auto callable = std::bind(func, std::forward<Args>(pack)...);
        std::packaged_task<ReturnT()> newTask{callable};
        std::shared_future<ReturnT> futureObject = newTask.get_future();

        std::unique_lock<std::mutex> locker{conveyerMutex_};
        conveyer_.push(std::move(newTask));

        conveyerCondition_.notify_one();
        locker.unlock();
        return futureObject;
    }

    ~ThreadPool()
    {
        exitflag_ = true;
        doAdd_ = false;
        condition_.notify_all();
        conveyerCondition_.notify_one();

        /*
        // waits all threads for correct exit
        for(auto& thread : threads_)
        {
            if(thread.joinable())
            {
                thread.join();
            }
        }*/
    }
};
//=========================================================================================
} // end of namespace GeneticAlgorithm

#endif // THREADPOOL_HPP_INCLUDED
