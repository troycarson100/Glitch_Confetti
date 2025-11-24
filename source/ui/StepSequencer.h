#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class StepCell : public juce::Button
{
public:
    StepCell (int index, juce::RangedAudioParameter& param) :
        juce::Button ("step" + juce::String (index)), idx (index), p (param) { setClickingTogglesState (false); }

    void paintButton (juce::Graphics& g, bool, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (2);
        
        // Get mode from AudioParameterChoice properly
        auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&p);
        int mode = choice ? choice->getIndex() : 0;
        
        juce::Colour bg = juce::Colours::black.withAlpha (0.6f);
        if (mode == 1) bg = juce::Colours::orange.withAlpha (0.35f);
        if (mode == 2) bg = juce::Colours::teal.withAlpha (0.35f);
        if (down)      bg = bg.brighter (0.2f);
        g.setColour (bg); g.fillRoundedRectangle (r, 6.f);
        
        // Draw border - bright if current step, dim otherwise
        juce::Colour borderColour = isCurrentStep ? juce::Colours::yellow : juce::Colours::white.withAlpha (0.25f);
        float borderWidth = isCurrentStep ? 3.0f : 1.0f;
        g.setColour (borderColour); 
        g.drawRoundedRectangle (r, 6.f, borderWidth);

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (juce::FontOptions (12.f, juce::Font::bold));

        juce::String sym = "-";
        if (mode == 1) sym = juce::CharPointer_UTF8 ("\xE2\x86\xBB"); // ↻
        if (mode == 2) sym = "/";

        g.drawFittedText (juce::String (idx+1), getLocalBounds().withTrimmedTop (getHeight()/2), juce::Justification::centred, 1);
        g.drawFittedText (sym, getLocalBounds().withTrimmedBottom (getHeight()/2), juce::Justification::centred, 1);
    }
    
    void setCurrentStep (bool current)
    {
        if (isCurrentStep != current)
        {
            isCurrentStep = current;
            repaint();
        }
    }

    void clicked() override
    {
        auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&p);
        if (! choice) return;
        const int next = (choice->getIndex() + 1) % choice->choices.size();
        choice->beginChangeGesture();
        choice->setValueNotifyingHost (choice->choices.size() > 1 ? (float) next / (float)(choice->choices.size() - 1) : 0.0f);
        choice->endChangeGesture();
        repaint();
    }

private:
    int idx;
    juce::RangedAudioParameter& p;
    bool isCurrentStep = false;
};

class StepSequencer : public juce::Component, private juce::Timer
{
public:
    StepSequencer (juce::AudioProcessorValueTreeState& s, std::function<int()> currentStepFn, std::function<void()> resetFn = nullptr)
    : apvts (s), getCurrentStep (std::move (currentStepFn)), onReset (std::move (resetFn))
    {
        DBG("StepSequencer constructor called");
        for (int i = 0; i < 16; ++i)
        {
            auto* rp = apvts.getParameter ("seq_step" + juce::String (i+1));
            jassert (rp != nullptr);
            auto cell = std::make_unique<StepCell> (i, *rp);
            addAndMakeVisible (*cell);
            cells.push_back (std::move (cell));
        }
        addAndMakeVisible (division);
        division.addItemList (juce::StringArray{ "1/4","1/8","1/16","1/32","Free" }, 1);
        division.onChange = [this]{
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("seq_division")))
            {
                const int idx = division.getSelectedItemIndex();              // 0..4
                const int numChoices = p->choices.size();
                const float norm = numChoices > 1 ? (float) idx / (float)(numChoices - 1) : 0.0f;
                p->beginChangeGesture();
                p->setValueNotifyingHost(norm);
                p->endChangeGesture();
            }
            updateControlStates();
        };

        addAndMakeVisible (rateHz);
        rateHz.setRange (0.5, 32.0, 0.001);
        rateHz.onValueChange = [this]{
            if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("seq_rate_hz")))
            {
                const float value = (float) rateHz.getValue(); // 0.5-32.0
                const float norm = p->getNormalisableRange().convertTo0to1(value);
                p->beginChangeGesture();
                p->setValueNotifyingHost(norm);
                p->endChangeGesture();
            }
        };

        addAndMakeVisible (stepsUsed);
        stepsUsed.setRange (1, 16, 1);
        stepsUsed.onValueChange = [this]{
            if (auto* p = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter("seq_steps")))
            {
                const int value = (int) stepsUsed.getValue(); // 1-16
                const float norm = (float)(value - 1) / (float)(16 - 1); // normalize to 0-1
                p->beginChangeGesture();
                p->setValueNotifyingHost(norm);
                p->endChangeGesture();
            }
        };

        addAndMakeVisible (followHostToggle);
        followHostToggle.setButtonText ("Follow Host");
        followHostToggle.onStateChange = [this]{
            apvts.getParameter ("seq_follow_host")->setValueNotifyingHost (followHostToggle.getToggleState() ? 1.0f : 0.0f);
            updateControlStates();
        };

        addAndMakeVisible (runButton);
        runButton.setButtonText ("Run");
        runButton.onStateChange = [this]{
            apvts.getParameter ("seq_run")->setValueNotifyingHost (runButton.getToggleState() ? 1.0f : 0.0f);
        };

        addAndMakeVisible (resetButton);
        resetButton.setButtonText ("🔁");
        resetButton.onClick = [this]{
            if (onReset) {
                // Fix: Use SafePointer to prevent accessing destroyed component during shutdown
                auto safeThis = juce::Component::SafePointer<StepSequencer>(this);
                auto resetFn = onReset; // Capture the function pointer
                auto* mm = juce::MessageManager::getInstanceWithoutCreating();
                if (mm != nullptr && safeThis != nullptr) {
                    mm->callAsync([safeThis, resetFn]() {
                        if (auto* self = safeThis.getComponent()) {
                            if (resetFn) {
                                resetFn();
                            }
                        }
                    });
                }
            }
        };

        // init from APVTS
        if (auto* divP = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("seq_division")))
            division.setSelectedItemIndex (divP->getIndex(), juce::dontSendNotification);
        rateHz.setValue (*apvts.getRawParameterValue ("seq_rate_hz"), juce::dontSendNotification);
        stepsUsed.setValue ((int) *apvts.getRawParameterValue ("seq_steps"), juce::dontSendNotification);
        followHostToggle.setToggleState (*apvts.getRawParameterValue ("seq_follow_host") > 0.5f, juce::dontSendNotification);
        runButton.setToggleState (*apvts.getRawParameterValue ("seq_run") > 0.5f, juce::dontSendNotification);
        updateControlStates();

        startTimerHz (30); // blink current step
    }

    ~StepSequencer() override
    {
        stopTimer(); // Fix: stop animation timer before controls are destroyed
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::darkblue.withAlpha (0.8f));
        g.setColour (juce::Colours::lightblue);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1), 8.f, 2.0f);

        g.setColour (juce::Colours::white);
        g.setFont (14.0f);
        
        // Show "Host" in header when following host
        bool isFollowingHost = followHostToggle.getToggleState();
        juce::String title = isFollowingHost ? "Step Sequencer (Host)" : "Step Sequencer";
        g.drawText (title, 10, 6, 200, 18, juce::Justification::left);
        
        // Debug info display
        g.setFont (10.0f);
        g.setColour (juce::Colours::yellow);
        auto debugText = "Step: " + juce::String(getCurrentStep ? getCurrentStep() : -1);
        g.drawText (debugText, getWidth() - 100, 6, 90, 18, juce::Justification::right);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        auto top = r.removeFromTop (28);
        resetButton.setBounds (top.removeFromRight (40).reduced (4, 2));
        runButton.setBounds (top.removeFromRight (80).reduced (4, 2));
        followHostToggle.setBounds (top.removeFromRight (100).reduced (4, 2));
        division.setBounds (top.removeFromRight (100).reduced (4, 2));
        rateHz.setBounds (top.removeFromRight (100).reduced (4, 2));
        stepsUsed.setBounds (top.removeFromRight (80).reduced (4, 2));

        auto grid = r.reduced (6);
        const int cols = 8, rows = 2;
        const int w = grid.getWidth() / cols;
        const int h = grid.getHeight() / rows;

        for (size_t i = 0; i < 16; ++i)
        {
            int row = static_cast<int>(i) / cols;
            int col = static_cast<int>(i) % cols;
            cells[i]->setBounds (grid.getX() + col * w + 4, grid.getY() + row * h + 4, w - 8, h - 8);
        }
    }

private:
    void timerCallback() override
    {
        // Fix: Check if component and MessageManager are still valid before accessing cells
        // This prevents crashes when timer callback fires during component destruction
        // Note: We can't access PluginEditor here due to forward declaration, but the
        // MessageManager check should catch most shutdown cases
        auto* mm = juce::MessageManager::getInstanceWithoutCreating();
        if (mm == nullptr || getParentComponent() == nullptr || !isVisible())
            return;
        
        const int cs = getCurrentStep ? getCurrentStep() : 0;
        for (size_t i = 0; i < cells.size(); ++i)
            cells[i]->setCurrentStep (static_cast<int>(i) == cs);
    }

    void updateControlStates()
    {
        bool followingHost = followHostToggle.getToggleState();
        
        // When following host, disable Run button
        runButton.setEnabled (!followingHost);
        
        // Update rate control based on division
        rateHz.setEnabled (division.getSelectedItemIndex() == 4); // Free mode only
        
        repaint(); // Update header text
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::function<int()> getCurrentStep;
    std::function<void()> onReset;

    std::vector<std::unique_ptr<StepCell>> cells;

    juce::Slider       rateHz;         // enabled only when "Free"
    juce::Slider       stepsUsed;      // 1..16
    juce::ComboBox     division;       // 1/4..Free
    juce::ToggleButton followHostToggle; // Follow host transport
    juce::ToggleButton runButton;      // Run (enabled only when not following host)
    juce::TextButton   resetButton;    // Reset sequencer phase

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepSequencer)
};