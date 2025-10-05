#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// Forward declaration
enum class PageID { SpaceDelay, Panner };

class FxPage : public juce::Component
{
public:
    struct Assets {
        std::unique_ptr<juce::Drawable> backgroundSVG; // full background
        std::unique_ptr<juce::Drawable> tabSVG;        // just the tab art
    };

    FxPage(PageID id, Assets pageAssets, std::function<void(PageID)> onTabClicked)
        : pageID(id), assets(std::move(pageAssets)), onTabClicked(std::move(onTabClicked))
    {
        // Tab button sits visually "behind" the active page except the tab tip.
        addAndMakeVisible(tabButton);
        tabButton.setTriggeredOnMouseDown(true);
        tabButton.onClick = [this]{ if (this->onTabClicked) this->onTabClicked(this->pageID); };
    }

    void paint(juce::Graphics& g) override
    {
        // full background
        if (assets.backgroundSVG != nullptr)
            assets.backgroundSVG->drawWithin(g, getLocalBounds().toFloat(),
                                             juce::RectanglePlacement::centred, 1.0f);
    }

    void resized() override
    {
        // Full page bounds are the same as the effect-left+master container's page area
        // Position the tab so it "sticks up" above the page by a few px
        auto r = getLocalBounds();

        const int tabW = 160;   // tune to your art
        const int tabH = 42;    // tab height visible above page
        const int tabX = (pageID == PageID::SpaceDelay ? 18 : 210); // x where that tab should appear
        const int tabY = -(tabH - 8); // negative Y to peek above top edge (8px overlap)

        tabButton.setBounds(tabX, tabY, tabW, tabH);
    }

    // Visual state (active/inactive) so you can dim an inactive tab if desired
    void setActive(bool isActive)
    {
        active = isActive;
        // Left controls only clickable when active
        for (auto* c : getChildren())
            if (c != &tabButton)
                c->setInterceptsMouseClicks(active, active);
        repaint();
    }

    PageID getPageID() const { return pageID; }
    
    // Method to add UI components to this page
    template<typename T>
    void addPageComponent(std::unique_ptr<T>& component)
    {
        if (component)
        {
            addAndMakeVisible(component.get());
        }
    }

private:
    PageID pageID;
    Assets assets;
    bool active = false;

    class TabButton : public juce::Button {
    public:
        TabButton(std::unique_ptr<juce::Drawable>& tabArt, bool* activePtr)
            : juce::Button("FxTab"), tabDrawableRef(tabArt), activeRef(activePtr) {}

        void paintButton(juce::Graphics& g, bool over, bool down) override
        {
            // draw the tab SVG at full tab bounds
            if (tabDrawableRef != nullptr)
                tabDrawableRef->drawWithin(g, getLocalBounds().toFloat(),
                                           juce::RectanglePlacement::stretchToFit, 1.0f);

            // Optional: overlay a subtle hover/active tint
            if (over)   g.fillAll(juce::Colours::white.withAlpha(0.06f));
            if (!*activeRef) g.fillAll(juce::Colours::black.withAlpha(0.10f)); // dim inactive
        }

    private:
        std::unique_ptr<juce::Drawable>& tabDrawableRef;
        bool* activeRef;
    } tabButton { assets.tabSVG, &active };

    std::function<void(PageID)> onTabClicked;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxPage)
};
