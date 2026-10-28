// genere par tools/make_background.py - ne pas modifier a la main (repere 1240 x 928, fond 2x)
#pragma once
#include <juce_graphics/juce_graphics.h>
namespace layout
{
    // ordre : master, gain, volume, distortion, bass, mid, treble, tone
    const juce::Point<float> knobPos[8] = { { 266.44f, 182.39f }, { 490.05f, 182.39f }, { 699.61f, 182.39f }, { 1018.04f, 182.39f }, { 152.88f, 391.25f }, { 369.46f, 391.25f }, { 596.59f, 391.25f }, { 892.78f, 391.25f } };
    const float knobR = 54.00f, knobTopR = 45.36f;
    const juce::Point<float> presPos { 500.58f, 552.53f }; const float presR = 22.68f, presTopR = 17.82f;
    const juce::Point<float> octPos { 1046.15f, 391.25f }; const float toggleS = 118.80f;
    const juce::Point<float> ledL { 289.85f, 594.83f }, ledR { 938.44f, 594.83f }; const float ledS = 43.20f;
    const juce::Rectangle<float> glowL { 170.00f, 475.00f, 240.00f, 240.00f }, glowR { 818.50f, 475.00f, 240.00f, 240.00f };
    const juce::Point<float> fsL { 232.50f, 783.42f }, fsR { 1018.04f, 783.42f }; const float footR = 47.52f, fsS = 183.60f;   // zone d'appui / sprite
    const juce::Rectangle<float> statusRect { 404.00f, 663.12f, 432.00f, 60.48f }, presetRect { 425.60f, 743.04f, 388.80f, 47.52f };
    const float statusFont = 69.12f, presetFont = 49.68f, uiScale = 1.08f;
    const juce::Rectangle<float> boxRect { 36.80f, 21.60f, 1165.32f, 883.44f };   // boitier (notice)
    const float jackOutX = 282.54f, jackInX = 991.01f, jackY = 6.48f;
}
