#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <atomic>

/**
 * Lock-free ring buffer for audio visualization
 * Stores downsampled amplitude values for efficient UI rendering
 */
class AudioRingBuffer
{
public:
    AudioRingBuffer() = default;
    
    void prepare(int numSamplesToStore)
    {
        buffer.resize(numSamplesToStore);
        buffer.assign(numSamplesToStore, 0.0f);
        writeIndex.store(0);
    }
    
    // Called from audio thread - push downsampled amplitude
    void push(float value) noexcept
    {
        const int idx = writeIndex.load(std::memory_order_relaxed);
        buffer[idx] = value;
        writeIndex.store((idx + 1) % buffer.size(), std::memory_order_release);
    }
    
    // Called from UI thread - read samples for visualization
    void readLatest(std::vector<float>& output, int numSamples) const
    {
        const int currentWrite = writeIndex.load(std::memory_order_acquire);
        const int bufferSize = (int)buffer.size();
        
        output.resize(numSamples);
        
        for (int i = 0; i < numSamples; ++i)
        {
            // Read backwards from current write position
            int readPos = currentWrite - numSamples + i;
            if (readPos < 0) readPos += bufferSize;
            readPos = readPos % bufferSize;
            
            output[i] = buffer[readPos];
        }
    }
    
    int size() const { return (int)buffer.size(); }
    
private:
    std::vector<float> buffer;
    std::atomic<int> writeIndex { 0 };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioRingBuffer)
};

