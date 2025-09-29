#include "source/FontManager.h"
#include <iostream>

int main() {
    auto& fontManager = FontManager::getInstance();
    
    std::cout << "Testing font loading..." << std::endl;
    
    // Test AlteHaasGroteskBold
    auto font1 = fontManager.getFont("AlteHaasGroteskBold", 12.0f, juce::Font::bold);
    std::cout << "AlteHaasGroteskBold typeface: " << font1.getTypefaceName().toStdString() << std::endl;
    
    // Test Akira Expanded
    auto font2 = fontManager.getFont("Akira Expanded", 12.0f, juce::Font::bold);
    std::cout << "Akira Expanded typeface: " << font2.getTypefaceName().toStdString() << std::endl;
    
    return 0;
}

