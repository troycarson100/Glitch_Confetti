#pragma once
#include <juce_core/juce_core.h>
#include "MacroTypes.h"

class MacroRouter
{
public:
    void setRoute (const MacroRoute& r)
    {
        const juce::ScopedWriteLock wl(lock);
        routesByParam.set(r.paramID, r); // one route per param
    }

    void removeRoute (const juce::String& pid)
    {
        const juce::ScopedWriteLock wl(lock);
        routesByParam.remove(pid);
    }

    bool hasRoute (const juce::String& pid) const
    {
        const juce::ScopedReadLock rl(lock);
        return routesByParam.contains(pid);
    }

    bool getRoute (const juce::String& pid, MacroRoute& out) const
    {
        const juce::ScopedReadLock rl(lock);
        if (!routesByParam.contains(pid)) return false;
        out = routesByParam[pid];
        return true;
    }

    juce::Array<MacroRoute> snapshot() const
    {
        const juce::ScopedReadLock rl(lock);
        juce::Array<MacroRoute> arr;
        for (auto it = routesByParam.begin(); it != routesByParam.end(); ++it)
            arr.add(it.getValue());
        return arr;
    }

    // Persistence
    juce::ValueTree toValueTree() const
    {
        const juce::ScopedReadLock rl(lock);
        juce::ValueTree t("macroRoutes");
        for (auto it = routesByParam.begin(); it != routesByParam.end(); ++it)
        {
            const auto& r = it.getValue();
            juce::ValueTree n("route");
            n.setProperty("paramID", r.paramID, nullptr);
            n.setProperty("macroIndex", r.macroIndex, nullptr);
            n.setProperty("mode", (int) r.mode, nullptr);
            n.setProperty("direction", (int) r.direction, nullptr);
            n.setProperty("depth", r.depth, nullptr);
            t.addChild(n, -1, nullptr);
        }
        return t;
    }

    void fromValueTree (const juce::ValueTree& t)
    {
        const juce::ScopedWriteLock wl(lock);
        routesByParam.clear();
        if (!t.isValid() || !t.hasType("macroRoutes")) return;

        for (int i = 0; i < t.getNumChildren(); ++i)
        {
            auto n = t.getChild(i);
            if (!n.hasType("route")) continue;
            MacroRoute r;
            r.paramID    = n.getProperty("paramID").toString();
            r.macroIndex = (int) n.getProperty("macroIndex");
            r.mode       = (MacroMode) (int) n.getProperty("mode");
            r.direction  = (MacroDirection) (int) n.getProperty("direction");
            r.depth      = (float) n.getProperty("depth");
            routesByParam.set(r.paramID, r);
        }
    }

private:
    mutable juce::ReadWriteLock lock;
    juce::HashMap<juce::String, MacroRoute> routesByParam;
};
