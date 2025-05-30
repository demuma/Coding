#pragma once

#include "../include/SharedBuffer.hpp"

template <typename T>
SharedBuffer<T>::SharedBuffer() :  writeBufferIndex(0), readBufferIndex(1), currentReadFrameIndex(0), currentWriteFrameIndex(0) {
    
        // Initialize the current read and write buffers
        currentReadBuffer.store(&buffers[readBufferIndex]);
        currentWriteBuffer.store(&buffers[writeBufferIndex]);
        finished.store(false);

        DEBUG_MSG("Shared buffer: write buffer: " << writeBufferIndex);
        DEBUG_MSG("Shared buffer: read buffer: " << readBufferIndex);
}

// Write a frame to the current write buffer
template <typename T>
void SharedBuffer<T>::write(const T& frame) {

    // Lock the queue
    std::lock_guard<std::mutex> lock(queueMutex);
    DEBUG_MSG("Simulation: writing frame: " << currentWriteFrameIndex << " on buffer " << writeBufferIndex);
    
    // Write to the current write buffer
    currentWriteBuffer.load()->push(frame);

    // Increment the current write frame index
    ++currentWriteFrameIndex;
}

// Read a frame from the current read buffer
template <typename T>
T SharedBuffer<T>::read() {

    // If read buffer empty, wait for a buffer swap to get the next frame
    std::unique_lock<std::mutex> lock(queueMutex);
    if(currentReadBuffer.load()->empty()) {
        if(!finished.load()){

            DEBUG_MSG("Renderer: waiting for frame: " << currentReadFrameIndex << " on buffer " << readBufferIndex);
            queueCond.wait(lock, [this] { return !currentReadBuffer.load()->empty(); });
            readBufferIndex = currentReadBuffer.load() - buffers;
            DEBUG_MSG("Renderer: read buffer swapped to " << readBufferIndex);
        } else {

            // Swap the read and write buffers
            std::queue<T>* currentWriteBuffer = &buffers[1 - readBufferIndex];
            currentReadBuffer.store(currentWriteBuffer);
        }
    }
    
    // Read current frame from the current read buffer 
    readBufferIndex = currentReadBuffer.load() - buffers; // Get the read index
    DEBUG_MSG("Renderer: rendering frame: " << currentReadFrameIndex << " in buffer " << readBufferIndex);
    
    // Read and delete the frame from the current read buffer
    currentFrame = currentReadBuffer.load()->front();
    currentReadBuffer.load()->pop();

    // Increment the current read frame index
    ++currentReadFrameIndex;

    return currentFrame;
}

// Swap the read and write buffers
template <typename T>
void SharedBuffer<T>::swap() {

    // Lock the queue
    std::lock_guard<std::mutex> lock(queueMutex);

    // Swap buffers if the read buffer is empty
    if (currentReadBuffer.load()->empty()) {

        DEBUG_MSG("Simulation: read buffer " << readBufferIndex.load() << " is empty");

        // Swap the read and write buffers
        currentReadBuffer.store(currentWriteBuffer);
        currentWriteBuffer.store(&buffers[readBufferIndex]);

        // Update the buffer indices
        writeBufferIndex.store(currentWriteBuffer - buffers);
        readBufferIndex.store(currentReadBuffer.load() - buffers);
        DEBUG_MSG("Simulation: swapping write buffer to " << writeBufferIndex.load());

        // Notify the renderer to continue reading from the new buffer
        queueCond.notify_one();
    }
}

// Signalize the simulation has finished so that the renderer can swap buffers if needed
template <typename T>
void SharedBuffer<T>::end() {
        finished.store(true);
}