<!-- c8febc51-1014-4a8f-821e-696510ed8bb7 eb7d4b7f-dd1f-4ef2-8256-b6f47729ebb2 -->
# Fix Comb- Prominence and Comb+ Clipping

## Problem Analysis

- **Comb-**: Still not prominent enough (currently 1.3x boost, -0.90 max feedback)
- **Comb+**: Still has some clipping despite 0.5x reduction and aggressive limiting

## Solution Strategy

### 1. Increase Comb- Prominence

**File**: `source/dsp/FilterProcessor.h`

- Increase `COMB_MINUS_OUTPUT_BOOST` from 1.3f to **1.6f** (60% boost instead of 30%)
- Increase `MAX_FEEDBACK_MINUS` from -0.90f to **-0.95f** (stronger negative feedback for deeper notches)
- Reduce `COMB_MINUS_DAMPING` from 0.18f to **0.15f** (less damping = sharper notches)
- This will make Comb- much more audible and prominent

### 2. Eliminate Comb+ Clipping

**File**: `source/dsp/FilterProcessor.h`

- Reduce `COMB_PLUS_OUTPUT_REDUCTION` from 0.5f to **0.45f** (55% reduction instead of 50%)
- Reduce `MAX_FEEDBACK_PLUS` from 0.85f to **0.80f** (lower max feedback = less accumulation)
- Add pre-output soft clipping stage before the existing output processing
- Tighten `OUTPUT_HARD_LIMIT` from 0.75f to **0.70f** (tighter final limit)
- Add additional soft limit stage specifically for Comb+ output path

### 3. Enhanced Protection Stages

**File**: `source/dsp/FilterProcessor.h`

- In `process()` method, add pre-output soft clipping for Comb+:
  - Apply `softClip()` with threshold 0.6f before the output reduction multiplier
  - This catches peaks before they get amplified by the feedback
- Add Comb+ specific output limiting:
  - After output reduction, apply additional `softLimit()` with 0.65f threshold
  - Then apply existing soft clip and soft limit stages

## Implementation Details

### Constants to Update (lines ~260-274)

```cpp
MAX_FEEDBACK_PLUS = 0.80f;        // Reduced from 0.85f
MAX_FEEDBACK_MINUS = -0.95f;      // Increased from -0.90f
COMB_MINUS_DAMPING = 0.15f;       // Reduced from 0.18f
COMB_PLUS_OUTPUT_REDUCTION = 0.45f; // Reduced from 0.5f
COMB_MINUS_OUTPUT_BOOST = 1.6f;   // Increased from 1.3f
OUTPUT_HARD_LIMIT = 0.70f;        // Reduced from 0.75f
```

### Process Method Updates (lines ~437-456 for left, ~492-511 for right)

- For Comb+ output path: Add pre-output soft clipping before reduction multiplier
- For Comb- output path: Keep existing boost, but with higher multiplier
- Add Comb+ specific additional soft limit stage after reduction

## Expected Results

- **Comb-**: Much more prominent and audible (60% boost, stronger feedback, sharper notches)
- **Comb+**: No clipping even at high resonance (more aggressive reduction, lower max feedback, additional protection stages)