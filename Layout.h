// genere par tools/make_background.py - ne pas modifier a la main (repere 1240 x 928, fond 2x)
#pragma once
#include <juce_graphics/juce_graphics.h>
namespace layout
{
    // ordre : master, gain, volume, distortion, bass, mid, treble, tone
    const juce::Point<float> knobPos[8] = { { 292.63f, 202.88f }, { 499.68f, 202.88f }, { 693.71f, 202.88f }, { 988.56f, 202.88f }, { 187.48f, 396.27f }, { 388.02f, 396.27f }, { 598.32f, 396.27f }, { 872.57f, 396.27f } };
    const float knobR = 50.00f, knobTopR = 42.00f;
    const juce::Point<float> presPos { 509.43f, 545.60f }; const float presR = 21.00f, presTopR = 16.50f;
    const juce::Point<float> octPos { 1014.58f, 396.27f }; const float toggleS = 124.00f;
    const juce::Point<float> ledL { 314.31f, 584.77f }, ledR { 914.85f, 584.77f }; const float ledS = 40.00f;
    const juce::Rectangle<float> glowL { 194.50f, 465.00f, 240.00f, 240.00f }, glowR { 795.00f, 465.00f, 240.00f, 240.00f };
    const juce::Point<float> fsL { 261.20f, 759.39f }, fsR { 988.56f, 759.39f }; const float footR = 44.00f, fsS = 170.00f;   // zone d'appui / sprite
}
