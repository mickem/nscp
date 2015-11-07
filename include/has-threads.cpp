#include <has-threads.hpp>
#include <boost/foreach.hpp>

//#include <iostream>

has_threads::has_threads()
    : grp(NULL), count(0)
{
    grp = new boost::thread_group();
}

has_threads::~has_threads()
{
    waitForThreads();
    delete grp;
}

void has_threads::interruptThreads()
{
    grp->interrupt_all();
}

void has_threads::waitForThreads()
{
    // Let any thread initialisation finish, otherwise
    // we might see deadlock on `thread_create_lock`.
    thread_start_lock.lock();
    thread_start_lock.unlock();

    {
        // Protects from threads being created whilst we join everything we already have.
        boost::mutex::scoped_lock l1(thread_create_lock);

        // Protects from threads in the process of destroying themselves
        // from conflicting with the following logic.
        boost::mutex::scoped_lock l2(thread_removal_lock);

        grp->join_all();

        // Now reset the group, so that any thread objects whose threads ended
        // during that `join_all` are destroyed properly.
        delete grp;
        grp = new boost::thread_group();
    }
}

size_t has_threads::threadCount() const
{
    boost::mutex::scoped_lock l(count_lock);
    return count;
}
