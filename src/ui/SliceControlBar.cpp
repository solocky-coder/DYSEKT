#include "SliceControlBar.h"
#include <cmath>
#include "UIHelpers.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../audio/GrainEngine.h"

// ── Fixed ADSR knob colours — match LCD node colours, theme-independent ───────
static const juce::Colour kAdsrAttack  { 0xFF00FF87 };   // Toxic Lime
static const juce::Colour kAdsrDecay   { 0xFFFFE800 };   // Radioactive Yellow
static const juce::Colour kAdsrSustain { 0xFF00C8FF };   // Ice Blue
static const juce::Colour kAdsrRelease { 0xFFFF6B00 };   // Molten Orange

static juce::Colour adsrTintForField (int fieldId)
{
    using F = DysektProcessor;
    if (fieldId == F::FieldAttack)  return kAdsrAttack;
    if (fieldId == F::FieldDecay)   return kAdsrDecay;
    if (fieldId == F::FieldSustain) return kAdsrSustain;
    if (fieldId == F::FieldRelease) return kAdsrRelease;
    return {};  // invalid = use theme default
}

namespace
{
constexpr int kParamCellTextX    = 14;
constexpr int kParamCellTextWidth = 60;
constexpr int kParamCellWidth    = kParamCellTextX + kParamCellTextWidth;

constexpr float kKnobStart = juce::MathConstants<float>::pi * 1.25f;
constexpr float kKn*

