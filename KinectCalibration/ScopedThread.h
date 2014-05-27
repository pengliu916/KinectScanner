#include <thread>
#include <stdexcept>

class ScopedThread
{
    std::thread t;
public:
    explicit ScopedThread(std::thread _t):t(std::move(_t)){
        if(!t.joinable())
            throw std::logic_error("No Thread");
    }
    ~ScopedThread(){
        t.join();
    }
    ScopedThread(ScopedThread const&)=delete;
    ScopedThread& operator=(ScopedThread const&)=delete;
};