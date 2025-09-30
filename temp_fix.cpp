            // Set all knobs to use 0-1 range internally (consistent behavior)
            masterKnobs[i]->setRange(0.0, 1.0, 0.001);
            
            // Connect to APVTS parameter - use normalized values directly
            if (auto* param = dynamic_cast<juce::AudioParameterFloat*>(processorRef.getParameters()[8 + i])) {
                // Get normalized parameter value directly (no double conversion)
                float paramValue = param->get(); // This is already normalized 0-1
                masterKnobs[i]->setValue(paramValue, juce::dontSendNotification);
            } else {
                // Set default values: Input and Output at 0.5 (0.0 dB), Dry/Wet at 1.0 (100%)
                float defaultValue = (i == 1) ? 1.0f : 0.5f; // Dry/Wet = 100% (1.0), Input/Output = 0.0 dB (0.5 normalized)
                masterKnobs[i]->setValue(defaultValue, juce::dontSendNotification);
            }
            
            // Set up value change callback - send normalized values directly
            masterKnobs[i]->onValueChange = [this, i]() {
                if (auto* param = dynamic_cast<juce::AudioParameterFloat*>(processorRef.getParameters()[8 + i])) {
                    float knobValue = masterKnobs[i]->getValue(); // This is 0-1
                    // Send normalized value directly to parameter (JUCE handles dB conversion)
                    param->setValueNotifyingHost(knobValue);
                }
            };

