#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../macro/MacroTypes.h"

// Draws ring + dot overlay for an assigned knob.
// Handles dot drag (depth) and right-click menu for mode/direction/remove.
class MacroOverlay : public juce::Component
{
public:
    MacroOverlay(juce::AudioProcessorValueTreeState& apvtsRef,
                 std::function<float()> getEffNormFn,
                 std::function<float()> getBaseNormFn,
                 std::function<bool(MacroRoute&)> getRouteFn,
                 std::function<void (const MacroRoute&)> setRouteFn,
                 std::function<void ()> removeRouteFn)
        : apvts(apvtsRef),
          getEffNorm(std::move(getEffNormFn)),
          getBaseNorm(std::move(getBaseNormFn)),
          getRouteForThisParam(std::move(getRouteFn)),
          setRoute(std::move(setRouteFn)),
          removeRoute(std::move(removeRouteFn))
    {
        setInterceptsMouseClicks(true, true);
    }

    void setParamID(juce::String id) { pid = std::move(id); }

    void paint(juce::Graphics& g) override
    {
        MacroRoute r;
        if (!getRouteForThisParam(r)) return;

        auto area = getLocalBounds().toFloat();
        const float size = juce::jmin(area.getWidth(), area.getHeight());
        const float radius = size * 0.5f - 3.f;
        const juce::Point<float> c = area.getCentre();

        juce::Colour color = (r.macroIndex == 0
            ? juce::Colour::fromString("FFF6A22D")
            : juce::Colour::fromString("FFEC571B"));

        const float base = getBaseNorm();
        const float d    = juce::jlimit(0.f, 1.f, r.depth);

        // Calculate the effective value to show where the knob should be
        const float eff = getEffNorm();
        
        // For the ring, we want to show the modulation range starting from current knob position
        // The ring shows where the knob can be modulated to
        float v0, v1;
        if (r.mode == MacroMode::Unipolar)
        {
            if (r.direction == MacroDirection::Forward) { 
                v0 = base;  // Start at current knob position
                v1 = juce::jlimit(0.f, 1.f, base + d);  // End at current position + depth
            } else { 
                v0 = juce::jlimit(0.f, 1.f, base - d);  // Start at current position - depth
                v1 = base;  // End at current knob position
            }
        }
        else // Bipolar
        {
            v0 = juce::jlimit(0.f, 1.f, base - d);  // Start at current position - depth
            v1 = juce::jlimit(0.f, 1.f, base + d);  // End at current position + depth
        }
        
        // Ensure we don't go beyond 0-1 range
        v0 = juce::jlimit(0.f, 1.f, v0);
        v1 = juce::jlimit(0.f, 1.f, v1);

        auto angleFor = [](float norm)
        {
            // Map 0-1 to knob angle range (225° to -45° = 270° total range)
            const float start = juce::degreesToRadians(225.f);
            const float end   = juce::degreesToRadians(-45.f);
            return start + (end - start) * norm;
        };

        // Range arc
        juce::Path arc;
        arc.addCentredArc(c.x, c.y, radius, radius, 0.0f, angleFor(v0), angleFor(v1), true);
        g.setColour(color.withAlpha(0.95f));
        g.strokePath(arc, juce::PathStrokeType(juce::jmax(2.f, radius * 0.08f)));

        // Dot positioned at top-left (Macro 1) or top-right (Macro 2) corner
        const float dotSize = (r.macroIndex == 0 ? size * 0.12f : size * 0.10f);
        
        // Position dot at corner of knob area
        juce::Point<float> dotPos;
        if (r.macroIndex == 0) {
            // Macro 1: top-left corner
            dotPos = juce::Point<float>(c.x - radius * 0.7f, c.y - radius * 0.7f);
        } else {
            // Macro 2: top-right corner  
            dotPos = juce::Point<float>(c.x + radius * 0.7f, c.y - radius * 0.7f);
        }
        
        g.setColour(color);
        g.fillEllipse(juce::Rectangle<float>(dotSize, dotSize).withCentre(dotPos));
        g.setColour(color.darker(0.4f));
        g.drawEllipse(juce::Rectangle<float>(dotSize, dotSize).withCentre(dotPos), 1.2f);

        // Ghost needle at effective value (shows where knob should be)
        const float effAngle = angleFor(eff);
        const float r0  = radius * 0.65f, r1 = radius * 0.95f;
        juce::Point<float> p0 = c + juce::Point<float>(std::cos(effAngle), std::sin(effAngle)) * r0;
        juce::Point<float> p1 = c + juce::Point<float>(std::cos(effAngle), std::sin(effAngle)) * r1;
        g.setColour(color.withAlpha(0.85f));
        g.drawLine({p0, p1}, 2.0f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        MacroRoute r;
        if (!getRouteForThisParam(r)) return;
        
        // Check if click is near the dot (positioned at corner)
        const float radius = juce::jmin(getLocalBounds().getWidth(), getLocalBounds().getHeight()) * 0.5f - 3.f;
        const juce::Point<float> c = getLocalBounds().toFloat().getCentre();
        
        // Calculate dot position (same as in paint method)
        juce::Point<float> dotPos;
        if (r.macroIndex == 0) {
            // Macro 1: top-left corner
            dotPos = juce::Point<float>(c.x - radius * 0.7f, c.y - radius * 0.7f);
        } else {
            // Macro 2: top-right corner  
            dotPos = juce::Point<float>(c.x + radius * 0.7f, c.y - radius * 0.7f);
        }
        
        const float dotSize = (r.macroIndex == 0 ? getLocalBounds().getWidth() * 0.12f : getLocalBounds().getWidth() * 0.10f);
        const float distance = e.getPosition().toFloat().getDistanceFrom(dotPos);
        
        if (distance <= dotSize * 0.6f) { // Clicked on dot
            startY = e.getPosition().y;
            if (e.mods.isPopupMenu()) showMenu();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        MacroRoute r;
        if (!getRouteForThisParam(r)) return;
        const float dy = e.getPosition().y - startY;
        const float delta = -dy / 300.f; // sensitivity
        r.depth = juce::jlimit(0.f, 1.f, r.depth + delta);
        setRoute(r);
        startY = e.getPosition().y;
        repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) showMenu();
    }

private:
    void showMenu()
    {
        MacroRoute r;
        if (!getRouteForThisParam(r)) return;

        juce::PopupMenu dir;
        dir.addItem(101, "Forward", true, r.direction == MacroDirection::Forward);
        dir.addItem(102, "Reverse", true, r.direction == MacroDirection::Reverse);

        juce::PopupMenu m;
        m.addSubMenu("Macro Unipolar", dir, true);
        m.addItem(2, "Macro Bipolar", true, r.mode == MacroMode::Bipolar);
        m.addSeparator();
        m.addItem(3, "Remove macro");

        m.showMenuAsync(juce::PopupMenu::Options{}, [this, r](int res) mutable
        {
            if      (res == 101) { r.mode = MacroMode::Unipolar; r.direction = MacroDirection::Forward; setRoute(r); }
            else if (res == 102) { r.mode = MacroMode::Unipolar; r.direction = MacroDirection::Reverse; setRoute(r); }
            else if (res == 2)   { r.mode = MacroMode::Bipolar; setRoute(r); }
            else if (res == 3)   { removeRoute(); }
            repaint();
        });
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::function<float()> getEffNorm, getBaseNorm;
    std::function<bool(MacroRoute&)> getRouteForThisParam;
    std::function<void (const MacroRoute&)> setRoute;
    std::function<void ()> removeRoute;

    juce::String pid;
    float startY = 0.f;
};