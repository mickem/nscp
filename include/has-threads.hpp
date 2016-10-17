#ifndef HASTHREADS_H
#define HASTHREADS_H

#include <set>
#include <iostream>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

/**
 * Concept to be derived by any class that should be capable of launching and tracking
 * worker threads, and waiting for workers to complete at object destruction.
 * 
 * Non-copyable.
 * 
 * 
 * ** NOTE REGARDING DESTRUCTION **
 * 
 * Threads are automatically waited for on destruction, but they are not interrupted. So:
 *  - Deriving classes should call interruptThreads() if any are persistent and would not terminate
 *    soon on their own.
 *  - They should also call waitForThreads() if any callback uses member data, to ensure that
 *    the thread finished execution before the member data is destroyed.
 * 
 * 
 * ** ANOTHER NOTE **
 * 
 * Be careful not to expose boost::thread_group::size(); if a thread uses it, this could cause
 * a deadlock with boost::thread_group::join_all(). Instead, we can track it ourselves, safely.
 */
struct has_threads
{
    public:

        /**
         * Asks all pending threads to terminate.
         * 
         * See http://www.boost.org/doc/libs/1_40_0/doc/html/thread/thread_management.html
         * for what this actually means.
         */
        void interruptThreads();

        /**
         * Wait for all pending threads to terminate.
         */
        void waitForThreads();

        has_threads();
        ~has_threads();

        /**
         * Create and run a tracked thread.
         */
        template <typename Callable>
        void createThread(Callable f);

        /**
         * Returns the number of threads currently managed by this object.
         */
        size_t threadCount() const;

    private:

        has_threads(has_threads const&);

        /**
         * Entrypoint function for a thread.
         * Wraps the user-provided worker function in signal and exception handling.
         */
        template <typename Callable>
        void runThread(Callable f, boost::thread* tp);


        boost::thread_group* grp;
        size_t count;

        mutable boost::mutex thread_create_lock;
        mutable boost::mutex thread_start_lock;
        mutable boost::mutex thread_removal_lock;
        mutable boost::mutex count_lock;
};


template <typename Callable>
void has_threads::createThread(Callable f)
{
    {
        // Although boost::thread_group does a good job of looking after threads for us,
        // it doesn't auto-remove objects wrapping terminated threads. So we must do that... carefully.

        thread_start_lock.lock(); // thread creation starts
        boost::mutex::scoped_lock l(thread_create_lock);

        // We need `tp` to be valid *before* the thread starts, so that its third argument is
        // always guaranteed to be valid. But we also need `tp` to be the pointer we put into the
        // thread_group. So we swap a running thread object into the original non-running one.
        boost::thread* tp = new boost::thread;

        boost::thread tmp(&has_threads::runThread<Callable>, this, f, tp);
        tp->swap(tmp);
        grp->add_thread(tp);

        {
            boost::mutex::scoped_lock l2(count_lock);
            count++;
        }
    }
}


template <typename Callable>
void has_threads::runThread(Callable f, boost::thread* tp)
{
    {
        // Wait until createThread's work with this thread object is definitely done
        boost::mutex::scoped_lock l(thread_create_lock);
    }

    thread_start_lock.unlock(); // thread creation is now deemed to be utterly complete

    try {
        f();
    }
    catch (boost::thread_interrupted& e) {

        // Yes, we should catch this exception! Letting it bubble over is _potentially_ dangerous:
        // http://stackoverflow.com/questions/6375121

        std::cout << "Thread " << boost::this_thread::get_id() << " interrupted (and ended)." << std::endl;
    }
    catch (std::exception& e) {
        std::cout << "Exception caught from thread " << boost::this_thread::get_id() << ": " << e.what() << std::endl;
    }
    catch (...) {
        std::cout << "Unknown exception caught from thread " << boost::this_thread::get_id() << std::endl;
    }

    // If the group is in the middle of a boost::thread_group::join_all, then boost::thread_group::remove_thread
    // would block as they sit on the same mutex. Silly >.< So we check a "removal" flag first.
    {
        boost::mutex::scoped_try_lock l(thread_removal_lock);
        if (!l) // can only occur when has_threads::waitForThreads() is in progress and we shouldn't be self-deleting
            return;

        grp->remove_thread(tp);
        {
            boost::mutex::scoped_lock l2(count_lock);
            count--;
        }

        delete tp;
    }
}


#endif