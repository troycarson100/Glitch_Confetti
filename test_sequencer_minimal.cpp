#include <iostream>
#include <chrono>
#include <thread>

int main() {
    std::cout << "Testing minimal sequencer logic..." << std::endl;
    
    int currentStep = -1;
    int testStep = 0;
    
    for (int i = 0; i < 20; ++i) {
        // Simulate the fallback sequencer logic
        if (i % 30 == 0) { // Every second at 30Hz
            testStep = (testStep + 1) % 16;
            std::cout << "Fallback sequencer: testStep=" << testStep << std::endl;
            
            if (testStep != currentStep) {
                std::cout << "applyStepHighlight: newStep=" << testStep << ", currentStep=" << currentStep << std::endl;
                currentStep = testStep;
                std::cout << "Set step " << testStep << " as sequencer active" << std::endl;
            }
        }
        
        // Simulate 30Hz timer
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
    
    std::cout << "Sequencer test completed successfully!" << std::endl;
    return 0;
}
