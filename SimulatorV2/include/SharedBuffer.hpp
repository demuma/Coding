#pragma once

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream>

#include "Agent.hpp"
#include "Logging.hpp"

/*****************************************/
/********** SHARED BUFFER CLASS **********/
/*****************************************/

class SharedBuffer {
public:
    SharedBuffer();
    void write(const std::vector<Agent>& frame);
    std::vector<Agent> read();
    void swap();
    void end();
    std::atomic<std::queue<std::vector<Agent>>*> currentReadBuffer;
    std::atomic<std::queue<std::vector<Agent>>*> currentWriteBuffer;
    std::atomic<int> writeBufferIndex;
    std::atomic<int> readBufferIndex;
    size_t currentReadFrameIndex;
    size_t currentWriteFrameIndex;

private:
    std::queue<std::vector<Agent>> buffers[2];
    std::mutex queueMutex;
    std::condition_variable queueCond;
    std::vector<Agent> currentFrame;
    std::atomic<bool> stopped;
};