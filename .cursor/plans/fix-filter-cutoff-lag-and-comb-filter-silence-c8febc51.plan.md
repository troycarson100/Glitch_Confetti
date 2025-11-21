<!-- c8febc51-1014-4a8f-821e-696510ed8bb7 1cc25977-2d93-4700-b39a-928f400322e7 -->
# Replace Comb Filter DSP with Stable Implementation

## Problem Analysis

The current comb filter implementation is causing high-pitch ringing, indicating:

1. **Feedback loop instability**: The delay line contains feedback that's being read and fed back again, creating oscillation
2. **Incorrect topology**: The output calculation may be adding delayed signal that already contains feedback
3. **Missing DC blocking**: No high-pass filter to prevent DC buildup
4. **Insufficient loop attenuation**: Feedback gain may be too high relative to filtering

## Solution: Standard Comb Filter Implementation

Based on research and the working DubDelayProcessor pattern, implement a proper comb filter using:

### Standard Comb Filter Equations:

- **Feedback Comb**: `y[n] = x[n] + g * y[n-M]` where g is feedback gain
- **Feedforward Comb**: `y[n] = x[n] + a * x[n-M]` where a is feedforward gain
- **Combined**: `y[n] = x[n] + g * y[n-M] + a * x[n-M]`

### Key Implementation Details:

1. **Delay Line Structure**:

- Read delayed signal from delay line
- Store clean delayed signal BEFORE feedback processing (for output)
- Process feedback path separately with filtering
- Write `input + filtered_feedback` to delay (not the output)

2. **Feedback Path Processing** (like DubDelayProcessor):

- Apply HPF (40Hz) to remove DC buildup
- Apply LPF for stability
- Scale feedback gain appropriately (0.6x multiplier like DubDelay)
- Clamp feedback to prevent runaway (-0.9 to 0.9)

3. **Output Calculation**:

- For Comb+: `output = input + delayed * feedback + delayed * depth`
- For Comb-: `output = input - delayed * feedback + delayed * depth`
- The delayed signal used in output is the CLEAN delayed signal (before feedback processing)

4. **Stability Measures**:

- DC-blocking HPF in feedback path
- Aggressive LPF in feedback path (lower cutoff for stability)
- Feedback gain reduction (0.6x multiplier)
- Proper clamping at multiple stages
- NaN/infinity checks

## Implementation Steps

### File: `source/dsp/FilterProcessor.h`

**1. Add HPF to CombProc** (lines 222-250):

- Add `juce::dsp::StateVariableTPTFilter<float> hpfL, hpfR;` member variables
- Initialize HPF in `prepare()`: set to highpass, 40Hz cutoff
- Reset HPF in `prepare()`

**2. Rewrite `CombProc::process()` method** (lines 303-400):

- Read delayed signal
- Store clean delayed for output: `wetL = delayedL`
- Process feedback path:
- Apply HPF: `delayedL = hpfL.processSample(0, delayedL)`
- Apply LPF: `delayedL = lpL.processSample(0, delayedL)`
- Calculate feedback: `fbL = delayedL * feedback * polarity * 0.6f` (0.6x reduction)
- Clamp: `fbL = juce::jlimit(-0.9f, 0.9f, fbL)`
- Calculate output:
- Feedforward: `ffL = wetL * depth`
- Output: `output = input + wetL * feedback * polarity + ffL`
- Write to delay: `delay.write(input + fbL)`
- Apply soft limiting to output

**3. Adjust parameter scaling in `set()` method** (lines 252-301):

- Reduce maximum feedback from 0.5 to 0.4
- Ensure feedback is always positive (0.0 to 0.4 range)
- Adjust LP cutoff based on feedback level (lower for higher feedback)

## Expected Result

- No high-pitch ringing or oscillation
- Stable comb filter behavior at all resonance levels
- Comb- and Comb+ both work correctly
- Musical, usable comb filter effect

### To-dos

- [x] 