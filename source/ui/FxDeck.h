#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "FxPageLayer.h"
#include "FxTabBar.h"

class FxDeck : public juce::Component
{
public:
    FxDeck(std::unique_ptr<juce::Drawable> spaceBg,
           std::unique_ptr<juce::Drawable> pannerBg,
           std::unique_ptr<juce::Drawable> spaceTabSvg,
           std::unique_ptr<juce::Drawable> pannerTabSvg,
           std::function<void(FxPageID)> onPageChanged)
        : onPageChanged(std::move(onPageChanged))
    {
        // Pages
        spacePage  = std::make_unique<FxPageLayer>(std::move(spaceBg));
        pannerPage = std::make_unique<FxPageLayer>(std::move(pannerBg));

        // Add back page first so it sits underneath
        addAndMakeVisible(*pannerPage);
        addAndMakeVisible(*spacePage);

        // Tabs sit above pages (they don't clip)
        tabBar = std::make_unique<FxTabBar>(std::move(spaceTabSvg), std::move(pannerTabSvg),
                    [this](FxPageID id){ setPage(id); });
        addAndMakeVisible(*tabBar);
        setPage(FxPageID::SpaceDelay, false);
    }

    FxPageLayer& space()  { return *spacePage;  }
    FxPageLayer& panner() { return *pannerPage; }

    void setPage(FxPageID id, bool notify=true)
    {
        if (current == id) return;
        current = id;

        // Show/Hide – safest, no layout churn, no reparenting at runtime
        const bool toSpace = (id == FxPageID::SpaceDelay);
        spacePage ->setVisible(toSpace);
        pannerPage->setVisible(!toSpace);

        // Make sure the active page is on top (for tab overlap aesthetics)
        if (toSpace) spacePage->toFront(false); else pannerPage->toFront(false);

        if (tabBar) tabBar->setActive(id);
        if (notify && onPageChanged) onPageChanged(id);
    }

    FxPageID getPage() const { return current; }

    void resized() override
    {
        auto r = getLocalBounds();
        // Tabs peek up a bit; leave room at the top
        const int tabHeight = 44;
        tabBar->setBounds(r.removeFromTop(tabHeight).withTrimmedLeft(6).withTrimmedRight(6));

        // Pages fill below tabs
        spacePage ->setBounds(r);
        pannerPage->setBounds(r);
    }

private:
    std::unique_ptr<FxPageLayer> spacePage, pannerPage;
    std::unique_ptr<FxTabBar> tabBar;
    FxPageID current { FxPageID::SpaceDelay };
    std::function<void(FxPageID)> onPageChanged;
};
