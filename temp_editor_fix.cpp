    // Add parameter listener
    processorRef.getValueTreeState().addParameterListener("timeMs", this);
    processorRef.getValueTreeState().addParameterListener("feedback", this);
    processorRef.getValueTreeState().addParameterListener("wowDepth", this);
    processorRef.getValueTreeState().addParameterListener("wowRate", this);
    processorRef.getValueTreeState().addParameterListener("saturation", this);
    processorRef.getValueTreeState().addParameterListener("highCut", this);
    processorRef.getValueTreeState().addParameterListener("lowCut", this);
    processorRef.getValueTreeState().addParameterListener("mix", this);
