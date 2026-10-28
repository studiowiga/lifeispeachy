// genere par tools/make_art.py - ne pas modifier a la main (repere 1240 x 928, fond 2x)
#pragma once
#include <juce_graphics/juce_graphics.h>
namespace layout
{
    // ordre : master, gain, volume, distortion, bass, mid, treble, tone
    const juce::Point<float> knobPos[8] = { { 292.63f, 202.88f }, { 499.68f, 202.88f }, { 693.71f, 202.88f }, { 988.56f, 202.88f }, { 187.48f, 396.27f }, { 388.02f, 396.27f }, { 598.32f, 396.27f }, { 872.57f, 396.27f } };
    const float knobR = 52.00f, knobTopR = 43.16f;      // rayon du potard / du disque noir du dessus (repere)
    const juce::Point<float> presPos { 509.43f, 545.60f }; const float presR = 22.00f, presTopR = 18.26f;
    const juce::Point<float> octPos { 1014.58f, 343.23f }; const float toggleS = 92.00f;
    const juce::Point<float> ledL { 314.31f, 584.77f }, ledR { 914.85f, 584.77f }; const float ledLensR = 5.58f;
    const juce::Rectangle<float> glowL { 194.50f, 465.00f, 240.00f, 240.00f }, glowR { 795.00f, 465.00f, 240.00f, 240.00f };
    const juce::Point<float> fsL { 261.20f, 759.39f }, fsR { 988.56f, 759.39f }; const float footR = 34.39f;   // bouton du footswitch
    const juce::Point<float> peachC { 612.00f, 716.00f }; const float peachR = 116.00f;
}
