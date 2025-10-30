// ThreadPoll Example
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
class ThreadPool {
    std::vector<std::thread> workers;
    std::queue<int> connections;
    std::condition_variable condition;
    std::mutex queue_mutex;

    bool stop = false;
public:
    ThreadPool(size_t threads){
        for (int i = 0; i<threads; ++i) {
            workers.emplace_back([this]{
                //TODO
            });
        }
    }
    ~ThreadPool(){}

};