#include "PluginEditor.h"
#include "BinaryData.h"
#include "Layout.h"

using namespace juce;

namespace
{
    // repere 1240 x 928 (fond 2480 x 1856), positions generees par tools/make_art.py dans Layout.h
    const Rectangle<float> statusRect { 420.0f, 648.0f, 400.0f, 56.0f };
    const Rectangle<float> presetRect { 440.0f, 722.0f, 360.0f, 44.0f };

    const float rotStart = MathConstants<float>::pi * (7.0f / 6.0f);    // -150 degres
    const float rotEnd   = MathConstants<float>::pi * (17.0f / 6.0f);   // +150 degres

    const Colour redCore (0xffffd8cc), redMid (0xffff2a18), redDeep (0xff8c0504);
    const Colour inkWhite (0xffeeeae6);
    const Colour accent (0xffd8262c);

    // valeurs suivies pour la ligne d'etat : 9 potards, octave, preampOn, fuzzOn, gate
    const char* const valueIds[] = { "master", "gain", "volume", "distortion", "bass", "mid", "treble", "tone", "presence",
                                     "octave", "preampOn", "fuzzOn", "gate" };
    constexpr int numValueIds = 13;

    Point<float> knobCentre (int i) { return i < 8 ? layout::knobPos[i] : layout::presPos; }
    float knobRadius (int i)        { return i < 8 ? layout::knobR : layout::presR; }
}

//==============================================================================
// POTARDS
//==============================================================================
bool PeachyKnob::hitTest (int x, int y)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    return c.getDistanceFrom ({ (float) x, (float) y }) <= getWidth() * 0.5f;
}

void PeachyKnobLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h,
                                              float pos, float startAngle, float endAngle, Slider& s)
{
    const auto* k = dynamic_cast<PeachyKnob*> (&s);
    const auto c = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float R = (float) jmin (w, h) * 0.5f * (k != nullptr ? k->topRatio : 0.83f);   // disque noir du dessus
    const float a = startAngle + pos * (endAngle - startAngle);
    const Point<float> dir (std::sin (a), -std::cos (a));
    const Point<float> p0 = c + dir * (0.13f * R), p1 = c + dir * (0.86f * R);
    const float lw = jmax (1.4f, 0.095f * R);
    // trait de peinture blanche sur le dessus mat ; un peu plus clair tourne vers la lumiere (haut gauche)
    const float lightFacing = 0.5f + 0.5f * std::cos (a + MathConstants<float>::pi * 0.25f);
    Path line; line.startNewSubPath (p0); line.lineTo (p1);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.strokePath (line, PathStrokeType (lw * 1.25f, PathStrokeType::curved, PathStrokeType::rounded), AffineTransform::translation (R * 0.012f, R * 0.016f));
    g.setColour (Colour::greyLevel (0.86f + 0.10f * lightFacing));
    g.strokePath (line, PathStrokeType (lw, PathStrokeType::curved, PathStrokeType::rounded));
}

//==============================================================================
// INVERSEUR
//==============================================================================
LeverSwitch::LeverSwitch (RangedAudioParameter& p)
    : attachment (p, [this] (float v)
                  {
                      const bool nv = v > 0.5f;
                      if (nv != on)
                      {
                          on = nv;
                          targetFrame = on ? 0 : 2;
                          shownFrame = 1;                                    // position intermediaire ~55 ms
                          transitionUntil = Time::getMillisecondCounterHiRes() + 55.0;
                          repaint();
                      }
                  })
{
    attachment.sendInitialUpdate();
    shownFrame = targetFrame = on ? 0 : 2;
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void LeverSwitch::tick (double now)
{
    if (shownFrame != targetFrame && now >= transitionUntil) { shownFrame = targetFrame; repaint(); }
}

void LeverSwitch::paint (Graphics& g)
{
    if (! strip.isValid()) return;
    const int fs = strip.getWidth();
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (strip, 0, 0, getWidth(), getHeight(), 0, shownFrame * fs, fs, fs);
}

void LeverSwitch::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
    attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
    if (onUserChange) onUserChange();
}

void LeverSwitch::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    if (std::abs (w.deltaY) <= 0.0f) return;
    const bool want = w.deltaY > 0;                       // molette vers le haut = levier en haut = ON
    if (want != on) { attachment.setValueAsCompleteGesture (want ? 1.0f : 0.0f); if (onUserChange) onUserChange(); }
}

//==============================================================================
// FOOTSWITCH
//==============================================================================
FootSwitchArea::FootSwitchArea (RangedAudioParameter& p, std::atomic<float>* holdMomentary)
    : hold (holdMomentary), attachment (p, [this] (float v) { value = v > 0.5f; repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (MouseCursor::PointingHandCursor);
}

bool FootSwitchArea::hitTest (int x, int y)
{
    // zone d'appui : le bouton et sa bague (le sprite, plus grand, contient aussi l'ecrou et l'ombre)
    const auto c = getLocalBounds().toFloat().getCentre();
    return c.getDistanceFrom ({ (float) x, (float) y }) <= (float) getWidth() * (layout::footR / layout::fsS);
}

void FootSwitchArea::paint (Graphics& g)
{
    // footswitch noir rendu en 3D : bouton enfonce tant qu'on appuie (il descend, son ombre portee raccourcit)
    const auto& im = pressed ? imgDown : imgUp;
    if (im.isValid())
    {
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (im, getLocalBounds().toFloat());
    }
}

void FootSwitchArea::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
    pressed = true;
    downTime = Time::getMillisecondCounterHiRes();
    turnedOnByPress = ! value;
    repaint();
    attachment.setValueAsCompleteGesture (value ? 0.0f : 1.0f);
    if (onUserChange) onUserChange();
}

void FootSwitchArea::mouseUp (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    const bool momentary = hold != nullptr && hold->load() > 0.5f;
    if (pressed && momentary && turnedOnByPress && Time::getMillisecondCounterHiRes() - downTime > 450.0 && value)
    {
        attachment.setValueAsCompleteGesture (0.0f);
        if (onUserChange) onUserChange();
    }
    pressed = false;
    repaint();
}

//==============================================================================
// EDITEUR
//==============================================================================
PeachyAudioProcessorEditor::PeachyAudioProcessorEditor (PeachyAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      octSwitch (*p.apvts.getParameter ("octave")),
      preFs  (*p.apvts.getParameter ("preampOn"), p.apvts.getRawParameterValue ("holdMomentary")),
      fuzzFs (*p.apvts.getParameter ("fuzzOn"),   p.apvts.getRawParameterValue ("holdMomentary"))
{
    background = ImageCache::getFromMemory (BinaryData::background_jpg, BinaryData::background_jpgSize);
    glow[0]    = ImageCache::getFromMemory (BinaryData::glow_l_png, BinaryData::glow_l_pngSize);
    glow[1]    = ImageCache::getFromMemory (BinaryData::glow_r_png, BinaryData::glow_r_pngSize);
    ledOn      = ImageCache::getFromMemory (BinaryData::led_on_png, BinaryData::led_on_pngSize);
    for (auto* f : { &preFs, &fuzzFs })
    {
        f->imgUp   = ImageCache::getFromMemory (BinaryData::fs_up_png,   BinaryData::fs_up_pngSize);
        f->imgDown = ImageCache::getFromMemory (BinaryData::fs_down_png, BinaryData::fs_down_pngSize);
    }
    octSwitch.strip = ImageCache::getFromMemory (BinaryData::toggle_strip_png, BinaryData::toggle_strip_pngSize);
    // police des inscriptions de la pedale (Shadows Into Light, ecriture a la main) pour la ligne d'etat et la barre de presets
    labelFace = Typeface::createSystemTypefaceFor (BinaryData::ShadowsIntoLight_ttf, BinaryData::ShadowsIntoLight_ttfSize);

    rightClickFwd.callback = [this] { showContextMenu(); };

    for (int i = 0; i < PeachyAudioProcessor::numKnobs; ++i)
    {
        auto& s = knobs[i];
        s.topRatio = i < 8 ? layout::knobTopR / layout::knobR : layout::presTopR / layout::presR;
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (rotStart, rotEnd, true);
        s.setMouseDragSensitivity (220);
        s.setDoubleClickReturnValue (true, 5.0);
        s.setLookAndFeel (&lnf);
        s.setMouseCursor (MouseCursor::PointingHandCursor);
        s.addMouseListener (&rightClickFwd, false);
        s.onDragStart = [this] { proc.currentPreset = -1; };
        addAndMakeVisible (s);
        knobAtt[i] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, PeachyAudioProcessor::knobId (i), s);
    }

    octSwitch.onRightClick = [this] { showContextMenu(); };
    octSwitch.onUserChange = [this] { proc.currentPreset = -1; };
    addAndMakeVisible (octSwitch);
    for (auto* f : { &preFs, &fuzzFs })
    {
        f->onRightClick = [this] { showContextMenu(); };
        f->onUserChange = [this] { proc.currentPreset = -1; };
        addAndMakeVisible (*f);
    }

    for (int i = 0; i < numValueIds; ++i) lastValues[i] = proc.raw (valueIds[i]);
    lastPresetCounter = proc.presetCounter.load();
    lastMidiCount = proc.midiCounter.load();
    ledLevel[0] = proc.raw ("preampOn") > 0.5f ? 1.0f : 0.0f;
    ledLevel[1] = proc.raw ("fuzzOn") > 0.5f ? 1.0f : 0.0f;

    // taille par defaut = taille minimale (comme Ring Think), proportions bloquees
    const int defaultW = roundToInt ((float) defaultH * (float) baseW / (float) baseH);
    setResizable (true, true);
    setResizeLimits (defaultW, defaultH, roundToInt ((float) maxH * baseW / baseH), maxH);
    if (auto* cons = getConstrainer()) cons->setFixedAspectRatio ((double) baseW / (double) baseH);
    setOpaque (true);
    setSize (defaultW, defaultH);
    rebuildCaches();
    startTimerHz (30);
}

PeachyAudioProcessorEditor::~PeachyAudioProcessorEditor()
{
    stopTimer();
    for (auto& s : knobs) { s.removeMouseListener (&rightClickFwd); s.setLookAndFeel (nullptr); }
}

//==============================================================================
void PeachyAudioProcessorEditor::setScaledHeight (int h)
{
    h = jlimit ((int) defaultH, (int) maxH, h);
    setSize (roundToInt ((float) h * (float) baseW / (float) baseH), h);
}

Rectangle<int> PeachyAudioProcessorEditor::scaled (Rectangle<float> r) const
{
    const float s = (float) getHeight() / (float) baseH;
    return r.transformedBy (AffineTransform::scale (s)).getSmallestIntegerContainer();
}

float PeachyAudioProcessorEditor::physicalScale() const
{
    return jlimit (1.0f, 4.0f, (float) Component::getApproximateScaleFactorForComponent (this));
}

void PeachyAudioProcessorEditor::showStatus() { statusUntil = Time::getMillisecondCounterHiRes() + 2500.0; }

// caches : fond et calques lumineux a la taille de la fenetre (reconstruits par le timer seulement)
void PeachyAudioProcessorEditor::rebuildCaches (float sc)
{
    if (sc <= 0.0f) sc = cacheScale > 0.0f ? cacheScale : physicalScale();
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;
    if (w == cacheW && h == cacheH && std::abs (sc - cacheScale) < 0.01f && bgScaled.isValid()) return;
    cacheW = w; cacheH = h; cacheScale = sc;
    const int pw = roundToInt ((float) w * sc), ph = roundToInt ((float) h * sc);
    bgScaled = (pw >= background.getWidth()) ? background : background.rescaled (pw, ph, Graphics::highResamplingQuality);
    const float s = (float) h / (float) baseH;
    const Rectangle<float> gr[2] = { layout::glowL, layout::glowR };
    for (int i = 0; i < 2; ++i)
    {
        const int gpx = roundToInt (gr[i].getWidth() * s * sc);
        glowS[i] = (gpx >= glow[i].getWidth()) ? glow[i] : glow[i].rescaled (gpx, gpx, Graphics::highResamplingQuality);
    }
    repaint();
}

void PeachyAudioProcessorEditor::resized()
{
    // anti-gel : jamais de setSize ici ; les limites du constrainer suffisent
    for (int i = 0; i < PeachyAudioProcessor::numKnobs; ++i)
    {
        const auto c = knobCentre (i); const float r = knobRadius (i);
        knobs[i].setBounds (scaled ({ c.x - r, c.y - r, r * 2.0f, r * 2.0f }));
    }
    const float ts = layout::toggleS * 0.5f;
    octSwitch.setBounds (scaled ({ layout::octPos.x - ts, layout::octPos.y - ts, ts * 2.0f, ts * 2.0f }));
    const float fr = layout::fsS * 0.5f;
    preFs.setBounds  (scaled ({ layout::fsL.x - fr, layout::fsL.y - fr, fr * 2.0f, fr * 2.0f }));
    fuzzFs.setBounds (scaled ({ layout::fsR.x - fr, layout::fsR.y - fr, fr * 2.0f, fr * 2.0f }));
    resizeSettle = 4;
}

//==============================================================================
// Anti-gel v2 (Studio Wiga) : rendu logiciel force sous Windows, aucun calcul lourd dans paint(),
// caches reconstruits par le timer, chien de garde de redessin ; cote audio, aucun appel a l'hote.
//==============================================================================
void PeachyAudioProcessorEditor::useSoftwareRenderer()
{
    auto* peer = getPeer();
    if (peer != lastPeer) { lastPeer = peer; repaint(); }
   #if JUCE_WINDOWS
    if (peer != nullptr)
    {
        int sw = peer->getAvailableRenderingEngines().indexOf ("Software Renderer");
        if (sw < 0) sw = 0;
        if (peer->getCurrentRenderingEngine() != sw) peer->setCurrentRenderingEngine (sw);
    }
   #endif
}

void PeachyAudioProcessorEditor::parentHierarchyChanged() { useSoftwareRenderer(); }
void PeachyAudioProcessorEditor::visibilityChanged()      { useSoftwareRenderer(); if (isShowing()) repaint(); }

void PeachyAudioProcessorEditor::askRepaint (Rectangle<int> r)
{
    if (repaintAskedMs <= 0.0) repaintAskedMs = Time::getMillisecondCounterHiRes();
    repaint (r);
}

//==============================================================================
String PeachyAudioProcessorEditor::buildStatusLine() const
{
    const int i = lastChanged;
    if (i < 0) return {};
    const float v = proc.raw (valueIds[i]);
    if (i < 9) return String (valueIds[i]) + "  " + String (roundToInt (v * 10.0f)) + " %";
    if (i == 9)  return v > 0.5f ? "octave on" : "octave off";
    if (i == 10) return v > 0.5f ? "preamp on" : "preamp off";
    if (i == 11) return v > 0.5f ? "fuzz on" : "fuzz off";
    return "noise gate " + PeachyAudioProcessor::gateNames()[jlimit (0, 3, roundToInt (v))].toLowerCase();
}

String PeachyAudioProcessorEditor::buildPresetLine() const
{
    const int cp = proc.currentPreset.load();
    String s;
    if (cp >= 0 && cp < PeachyAudioProcessor::getNumFactoryPresets())
        s << "P" << (cp + 1) << "  " << PeachyAudioProcessor::getFactoryPreset (cp).name;
    else if (cp > 100) s << "User " << (cp - 100);
    else               s << "Edited";
    return s;
}

//==============================================================================
void PeachyAudioProcessorEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    if (getPeer() != lastPeer || now - lastRendererCheckMs > 1000.0) { lastRendererCheckMs = now; useSoftwareRenderer(); }

    if (resizeSettle > 0 && --resizeSettle == 0) rebuildCaches();
    else if (resizeSettle == 0 && now - lastScaleCheckMs > 250.0)
    {
        lastScaleCheckMs = now;
        const float sc = physicalScale();
        if (std::abs (sc - cacheScale) > 0.01f) rebuildCaches (sc);
    }

    if (repaintAskedMs > 0.0 && isShowing() && now - repaintAskedMs > 1500.0 && lastPaintMs < repaintAskedMs)
    {
        repaintAskedMs = 0.0; lastPeer = nullptr;
        useSoftwareRenderer(); repaint();
    }

    octSwitch.tick (now);

    int changed = -1, numChanged = 0;
    for (int i = 0; i < numValueIds; ++i)
    {
        const float v = proc.raw (valueIds[i]);
        if (std::abs (v - lastValues[i]) > 1.0e-4f) { lastValues[i] = v; changed = i; ++numChanged; }
    }
    const int pcount = proc.presetCounter.load();
    if (pcount != lastPresetCounter) { lastPresetCounter = pcount; numChanged = 0; }
    if (numChanged == 1) { lastChanged = changed; showStatus(); }
    for (int i = 0; i < PeachyAudioProcessor::numKnobs; ++i)
        if (knobs[i].isMouseButtonDown()) { lastChanged = i; showStatus(); }
    const int mc = proc.midiCounter.load();
    if (mc != lastMidiCount) { lastMidiCount = mc; midiFlash = 8; }
    if (midiFlash > 0) --midiFlash;

    const float target = now < statusUntil ? 1.0f : 0.0f;
    statusAlpha += (target - statusAlpha) * (target > statusAlpha ? 0.45f : 0.12f);
    if (statusAlpha < 0.005f) statusAlpha = 0.0f;
    if (statusAlpha > 0.995f) statusAlpha = 1.0f;

    // LED blanches : PREAMP (gauche), FUZZ (droite) ; fondu 0,45 a l'allumage / 0,28 a l'extinction
    const bool on[2] = { proc.raw ("preampOn") > 0.5f, proc.raw ("fuzzOn") > 0.5f };
    const Rectangle<float> gr[2] = { layout::glowL, layout::glowR };
    for (int i = 0; i < 2; ++i)
    {
        auto& l = ledLevel[i];
        l += ((on[i] ? 1.0f : 0.0f) - l) * (on[i] ? 0.45f : 0.28f);
        if (l < 0.003f) l = 0.0f;
        if (l > 0.997f) l = 1.0f;
        if (std::abs (l - drawnLed[i]) > 0.004f) { drawnLed[i] = l; askRepaint (scaled (gr[i])); }
    }

    const auto l1 = buildStatusLine(), pl = buildPresetLine();
    const bool textChanged = l1 != lastLine1;
    lastLine1 = l1;
    if (std::abs (statusAlpha - drawnStatus) > 0.003f || (statusAlpha > 0.0f && textChanged))
    {
        drawnStatus = statusAlpha;
        askRepaint (scaled (statusRect.expanded (4.0f)));
    }
    if (pl != lastPreset || hoverZone != drawnHover || midiFlash > 0)
    {
        lastPreset = pl; drawnHover = hoverZone;
        askRepaint (scaled (presetRect.expanded (3.0f)));
    }
}

//==============================================================================
void PeachyAudioProcessorEditor::drawLed (Graphics& g, int which)
{
    const float level = ledLevel[which];
    if (level <= 0.003f) return;
    const auto gr = which == 0 ? layout::glowL : layout::glowR;
    if (glowS[which].isValid()) { g.setOpacity (level); g.drawImage (glowS[which], gr); }
    g.setOpacity (1.0f);

    // LED allumee : sprite rendu avec la meme lumiere que la pedale (lentille en dome, coeur chaud, reflet)
    const auto c = which == 0 ? layout::ledL : layout::ledR;
    const float hs = layout::ledS * 0.5f;
    if (ledOn.isValid())
    {
        g.setOpacity (level);
        g.drawImage (ledOn, { c.x - hs, c.y - hs, hs * 2.0f, hs * 2.0f });
        g.setOpacity (1.0f);
    }
    // tres leger halo optique autour de la lentille
    ColourGradient halo (Colour (0xfff4f6ff).withAlpha (0.42f * level), c.x, c.y, Colour (0xfff4f6ff).withAlpha (0.0f), c.x + 30.0f, c.y, true);
    halo.addColour (0.35, Colour (0xfff4f6ff).withAlpha (0.14f * level));
    g.setGradientFill (halo);
    g.fillEllipse (c.x - 30.0f, c.y - 30.0f, 60.0f, 60.0f);
}

void PeachyAudioProcessorEditor::drawStatus (Graphics& g)
{
    const float a = statusAlpha;
    if (a <= 0.0f) return;
    g.setColour (Colour (0xff0b0909).withAlpha (0.92f * a));
    g.fillRoundedRectangle (statusRect, 9.0f);
    g.setColour (accent.withAlpha (0.55f * a));
    g.drawRoundedRectangle (statusRect.reduced (0.75f), 9.0f, 1.6f);
    g.setFont (Font (FontOptions (labelFace).withHeight (64.0f)));
    g.setColour (inkWhite.withAlpha (a));
    g.drawFittedText (lastLine1, statusRect.reduced (12.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.75f);
}

void PeachyAudioProcessorEditor::drawPresetBar (Graphics& g)
{
    const auto r = presetRect;
    g.setColour (Colour (0xff0b0909).withAlpha (0.86f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (accent.withAlpha (hoverZone != 0 ? 0.80f : 0.45f));
    g.drawRoundedRectangle (r.reduced (0.6f), r.getHeight() * 0.5f, 1.3f);
    const float aw = 38.0f;
    auto arrow = [&] (Rectangle<float> ar, bool left, bool hot)
    {
        Path p; const auto c = ar.getCentre(); const float s = 7.0f;
        if (left) p.addTriangle (c.x + s * 0.6f, c.y - s, c.x + s * 0.6f, c.y + s, c.x - s * 0.8f, c.y);
        else      p.addTriangle (c.x - s * 0.6f, c.y - s, c.x - s * 0.6f, c.y + s, c.x + s * 0.8f, c.y);
        g.setColour (inkWhite.withAlpha (hot ? 1.0f : 0.62f));
        g.fillPath (p);
    };
    arrow (r.withWidth (aw), true, hoverZone == 1);
    arrow (r.withTrimmedLeft (r.getWidth() - aw), false, hoverZone == 3);
    g.setFont (Font (FontOptions (labelFace).withHeight (46.0f)));
    g.setColour ((midiFlash > 0 ? Colour (0xffff9a80) : inkWhite).withAlpha (hoverZone == 2 ? 1.0f : 0.90f));
    g.drawFittedText (lastPreset, r.reduced (aw, 0.0f).toNearestInt(), Justification::centred, 1, 0.7f);
}

void PeachyAudioProcessorEditor::paint (Graphics& g)
{
    lastPaintMs = Time::getMillisecondCounterHiRes();
    repaintAskedMs = 0.0;
    g.fillAll (Colours::black);
    const Image& bg = bgScaled.isValid() ? bgScaled : background;
    g.setImageResamplingQuality (&bg == &background ? Graphics::highResamplingQuality : Graphics::lowResamplingQuality);
    if (bg.isValid()) g.drawImage (bg, getLocalBounds().toFloat());

    const float s = (float) getHeight() / (float) baseH;
    g.addTransform (AffineTransform::scale (s));
    drawLed (g, 0);
    drawLed (g, 1);
    drawStatus (g);
    drawPresetBar (g);
}

//==============================================================================
static int presetZoneAt (Point<float> p)
{
    if (! presetRect.contains (p)) return 0;
    if (p.x < presetRect.getX() + 38.0f) return 1;
    if (p.x > presetRect.getRight() - 38.0f) return 3;
    return 2;
}

void PeachyAudioProcessorEditor::mouseMove (const MouseEvent& e)
{
    const float s = (float) getHeight() / (float) baseH;
    hoverZone = presetZoneAt (e.position / s);
    setMouseCursor (hoverZone != 0 ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
}

void PeachyAudioProcessorEditor::mouseExit (const MouseEvent&) { hoverZone = 0; }

void PeachyAudioProcessorEditor::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showContextMenu(); return; }
    const float s = (float) getHeight() / (float) baseH;
    switch (presetZoneAt (e.position / s))
    {
        case 1: stepPreset (-1); break;
        case 2: showPresetMenu(); break;
        case 3: stepPreset (1); break;
        default: break;
    }
}

void PeachyAudioProcessorEditor::stepPreset (int delta)
{
    const int n = PeachyAudioProcessor::getNumFactoryPresets();
    int cp = proc.currentPreset.load();
    if (cp < 0 || cp >= n) cp = delta > 0 ? -1 : 0;
    proc.applyFactoryPreset ((cp + delta + n) % n);
}

void PeachyAudioProcessorEditor::showPresetMenu()
{
    PopupMenu m;
    m.addSectionHeader ("Factory presets");
    for (int i = 0; i < PeachyAudioProcessor::getNumFactoryPresets(); ++i)
        m.addItem (100 + i, String (i + 1) + ". " + PeachyAudioProcessor::getFactoryPreset (i).name, true, proc.currentPreset.load() == i);
    m.addSeparator();
    PopupMenu load, save;
    for (int slot = 1; slot <= 9; ++slot)
    {
        const bool exists = proc.hasUserPreset (slot);
        load.addItem (200 + slot, "User " + String (slot) + (exists ? "" : "  (empty)"), exists, proc.currentPreset.load() == 100 + slot);
        save.addItem (300 + slot, "User " + String (slot) + (exists ? "  (overwrite)" : ""));
    }
    m.addSubMenu ("Load user preset", load);
    m.addSubMenu ("Save user preset", save);
    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (scaled (presetRect))),
        [safe = Component::SafePointer<PeachyAudioProcessorEditor> (this)] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& pr = safe->proc;
            if      (r >= 100 && r < 200) pr.applyFactoryPreset (r - 100);
            else if (r > 200 && r < 300)  pr.loadUserPreset (r - 200);
            else if (r > 300 && r < 400)  pr.saveUserPreset (r - 300);
            safe->showStatus();
        });
}

void PeachyAudioProcessorEditor::showContextMenu()
{
    PopupMenu menu, sizes;
    const int defaultW = roundToInt ((float) defaultH * (float) baseW / (float) baseH);
    sizes.addItem (1, "Default (" + String (defaultW) + " x " + String (defaultH) + ")", true, getHeight() == defaultH);
    sizes.addItem (2, "75 %",  true, getHeight() == roundToInt (refH * 0.75f));
    sizes.addItem (3, "100 %", true, getHeight() == refH);
    menu.addSubMenu ("Window size", sizes);
    menu.addSeparator();

    PopupMenu factory;
    for (int i = 0; i < PeachyAudioProcessor::getNumFactoryPresets(); ++i)
        factory.addItem (100 + i, String (i + 1) + ". " + PeachyAudioProcessor::getFactoryPreset (i).name, true, proc.currentPreset.load() == i);
    menu.addSubMenu ("Factory presets", factory);
    PopupMenu load, save;
    for (int slot = 1; slot <= 9; ++slot)
    {
        const bool exists = proc.hasUserPreset (slot);
        load.addItem (200 + slot, "User " + String (slot) + (exists ? "" : "  (empty)"), exists, proc.currentPreset.load() == 100 + slot);
        save.addItem (300 + slot, "User " + String (slot) + (exists ? "  (overwrite)" : ""));
    }
    menu.addSubMenu ("Load user preset", load);
    menu.addSubMenu ("Save user preset", save);
    menu.addSeparator();

    PopupMenu gate;
    const int gNow = jlimit (0, 3, roundToInt (proc.raw ("gate")));
    const char* gateHelp[4] = { "Off", "Light (default)", "Medium", "Strong (noisy pickups / hum)" };
    for (int i = 0; i < 4; ++i) gate.addItem (50 + i, gateHelp[i], true, gNow == i);
    menu.addSubMenu ("Noise gate", gate);

    PopupMenu settings;
    settings.addItem (40, "Hold footswitch = momentary", true, proc.raw ("holdMomentary") > 0.5f);
    settings.addSeparator();
    settings.addItem (41, "MIDI footswitches latching (127 = on / 0 = off)", true, proc.raw ("midiFsMode") < 0.5f);
    settings.addItem (42, "MIDI footswitches momentary (toggle each press)", true, proc.raw ("midiFsMode") > 0.5f);
    menu.addSubMenu ("Settings", settings);

    PopupMenu midi;
    const char* names[9] = { "Master", "Gain", "Volume (fuzz)", "Distortion", "Bass", "Mid", "Treble", "Tone (fuzz)", "Presence" };
    for (int i = 0; i < 9; ++i) midi.addItem (900 + i, "CC " + String (20 + i) + "  " + names[i], false);
    midi.addItem (910, "CC 29  Octave (64+ on)", false);
    midi.addItem (911, "CC 30  Preamp footswitch", false);
    midi.addItem (912, "CC 31  Fuzz footswitch", false);
    midi.addItem (913, "CC 52-60  fine (14-bit) for CC 20-28", false);
    midi.addItem (914, "Program Change 1-10  Factory presets", false);
    menu.addSubMenu ("MIDI map", midi);
    menu.addSeparator();
    menu.addItem (999, "Peachy preamp + fuzz v" + String (JucePlugin_VersionString) + " - STUDIO WIGA", false);

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this),
        [safe = Component::SafePointer<PeachyAudioProcessorEditor> (this)] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& ed = *safe; auto& pr = ed.proc;
            if      (r == 1) ed.setScaledHeight (defaultH);
            else if (r == 2) ed.setScaledHeight (roundToInt (refH * 0.75f));
            else if (r == 3) ed.setScaledHeight (refH);
            else if (r == 40) pr.setParamValue ("holdMomentary", pr.raw ("holdMomentary") > 0.5f ? 0.0f : 1.0f);
            else if (r == 41) pr.setParamValue ("midiFsMode", 0.0f);
            else if (r == 42) pr.setParamValue ("midiFsMode", 1.0f);
            else if (r >= 50 && r <= 53) pr.setParamValue ("gate", (float) (r - 50));
            else if (r >= 100 && r < 200) pr.applyFactoryPreset (r - 100);
            else if (r > 200 && r < 300)  pr.loadUserPreset (r - 200);
            else if (r > 300 && r < 400)  pr.saveUserPreset (r - 300);
            ed.showStatus();
        });
}
