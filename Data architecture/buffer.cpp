#include <iostream>
#include <queue>
#include <thread>
#include <chrono>

std::queue<int> buffer;
std::mutex mtx;
std::condition_variable cv;

void producer()
{   
    std::chrono::steady_clock clock;
    auto start = clock.now();
    int i = 0;
    while(true) {
        // std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Simulate production time
        
        std::lock_guard<std::mutex> lock(mtx);
        buffer.push(i);
        cv.notify_one(); // Notify the consumer that an item is available
        i++;
    }
}

void consumer()
{
    std::chrono::steady_clock clock;
    float frameRate = 60.0;
    float timeStep = 1.0 / frameRate;
    auto start = clock.now();
    int i = 0;
    while (true) {
        auto start = clock.now();
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, []{ return !buffer.empty(); }); // Wait until an item is available in the buffer
        
        int item = buffer.front();
        buffer.pop();
        // std::cout << "Consumed: " << item << std::endl;
        
        lock.unlock();
        auto end = clock.now();
        auto diff = end - start;
        // long delay = (1000000000 * timeStep) - std::chrono::duration_cast<std::chrono::nanoseconds>(diff).count();
        long delay = 1000000000 * timeStep;
        i++;
        std::this_thread::sleep_for(std::chrono::nanoseconds(delay)); // Simulate consumption time
        auto end2 = clock.now();
        auto diff2 = end2 - start;
        long delay2 = std::chrono::duration_cast<std::chrono::nanoseconds>(diff2).count();
        std::cout << "Frame rate: " << (double)delay2 / 1000000000.0f  << " s" << std::endl;
    }
}

int main()
{
    std::thread producerThread(producer);
    std::thread consumerThread(consumer);
    
    producerThread.join();
    consumerThread.join();
    
    return 0;
}