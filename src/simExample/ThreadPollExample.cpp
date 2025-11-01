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
                while(true){
                    int conn_fd;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock,[this]{
                            return this->stop || !this->connections.empty();
                        });
                        if(this->stop && this->connections.empty()) return;
                        conn_fd = this->connections.front();
                        this->connections.pop();
                    }
                    handle_client(conn_fd);
                }
            });
        }
    }
    void enqueue(int conn_fd){
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            connections.push(conn_fd);
        }
        condition.notify_all();
    }
    ~ThreadPool(){
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for(std::thread& worker:workers){
            worker.join();
        }
    }

};