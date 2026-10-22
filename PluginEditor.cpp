#include "PluginEditor.h"
#include <BinaryData.h>

using namespace juce;

namespace
{
    // Repère du dessin = taille de background.png (pédale + marges latérales pour les jacks). Toutes les coordonnées sont dans ces pixels.
    constexpr int kW = 1240, kH = 928;
    constexpr int kRefW = 993;                                   // tailles de fenêtre exprimées par rapport à cette largeur (comme Ring Think)
    const float kScales[3] = { 661.0f / 993.0f, 0.75f, 1.0f };
    constexpr float kDefaultScale = 661.0f / 993.0f;             // fenêtre par défaut = 661 x 495 px (= taille minimale)

    // ---- emplacements des commandes (à recaler sur le design définitif) ----
    constexpr float kKnobX = 232.0f, kKnobY = 172.0f, kKnobR = 70.0f;           // sélecteur 21 crans
    constexpr float kKnobArc = 150.0f;                                          // ± degrés du premier au dernier cran
    inline Point<float> progLed (int i)                                          // LED de la liste des programmes
    {
        const int col = i < 12 ? 0 : 1, row = i < 12 ? i : i - 12;
        return { (col == 0 ? 410.0f : 600.0f) + 6.0f, 60.0f + (float) row * 19.0f };
    }
    inline Rectangle<float> progLabel (int i) { auto p = progLed (i); return { p.x - 10.0f, p.y - 9.0f, 175.0f, 18.0f }; }
    const Rectangle<float> kSwitch (920.0f, 60.0f, 60.0f, 26.0f);                // inverseur CLASSIC / CHORDS
    const Rectangle<float> kSwitchHit (830.0f, 52.0f, 230.0f, 42.0f);
    const Rectangle<float> kPresetBox (832.0f, 100.0f, 258.0f, 30.0f);           // menu déroulant des presets
    constexpr float kFsLX = 200.0f, kFsRX = 1040.0f, kFsY = 760.0f, kFsScale = 1.15f;
    constexpr float kLedLX = 200.0f, kLedRX = 1040.0f, kLedY = 668.0f;
    const Rectangle<float> kStatus (300.0f, 874.0f, 640.0f, 24.0f);              // ligne d'état
    const Colour kOrange (0xffff9a1a), kRed (0xffff2a2a), kText (0xfff2e6e6);
    constexpr int kFrames = 25;                                                  // positions précalculées du plateau

    Image loadPng (const void* d, int n) { return ImageFileFormat::loadFrom (d, (size_t) n).convertedToFormat (Image::ARGB); }

    // redimensionne une fois, puis garde en cache tant que la taille ne change pas
    struct ScaledCache
    {
        Image src, img; float scale = -1.0f;
        const Image& get (float s)
        {
            if (std::abs (s - scale) > 1.0e-4f || ! img.isValid())
            {
                scale = s;
                const int w = jmax (1, roundToInt ((float) src.getWidth() * s)), h = jmax (1, roundToInt ((float) src.getHeight() * s));
                img = src.rescaled (w, h, Graphics::highResamplingQuality);
            }
            return img;
        }
    };

    //==========================================================================
    // Plateau de la pédale : morphing entre le calque « orteil levé » et le calque « orteil enfoncé ».
    // Chaque ligne de pixels du contour est interpolée (perspective d'un plateau qui pivote), puis les deux images sont fondues.
    class TreadleMorph
    {
    public:
        void setSources (Image upImg, Image downImg)
        {
            up = std::move (upImg); down = std::move (downImg);
            box = (opaqueBounds (up).getUnion (opaqueBounds (down))).expanded (4).getIntersection ({ 0, 0, kW, kH });
            scale = -1.0f;
        }
        Rectangle<int> designBounds() const { return box; }

        void prepare (float s)
        {
            if (std::abs (s - scale) < 1.0e-4f) return;
            scale = s;
            const int bw = jmax (1, roundToInt ((float) box.getWidth() * s)), bh = jmax (1, roundToInt ((float) box.getHeight() * s));
            su = up.getClippedImage (box).rescaled (bw, bh, Graphics::highResamplingQuality).convertedToFormat (Image::ARGB);
            sd = down.getClippedImage (box).rescaled (bw, bh, Graphics::highResamplingQuality).convertedToFormat (Image::ARGB);
            ou = outline (su); od = outline (sd);
            for (auto& f : frames) f = {};
        }

        const Image& frame (int i)
        {
            i = jlimit (0, kFrames - 1, i);
            if (! frames[(size_t) i].isValid()) frames[(size_t) i] = build ((float) i / (float) (kFrames - 1));
            return frames[(size_t) i];
        }

    private:
        struct Outline { int top = 0, bottom = 0; std::vector<float> l, r; };

        static Rectangle<int> opaqueBounds (const Image& im)
        {
            const Image::BitmapData bd (im, Image::BitmapData::readOnly);
            int x0 = im.getWidth(), y0 = im.getHeight(), x1 = -1, y1 = -1;
            for (int y = 0; y < im.getHeight(); ++y)
                for (int x = 0; x < im.getWidth(); ++x)
                    if (bd.getPixelColour (x, y).getAlpha() > 6) { x0 = jmin (x0, x); x1 = jmax (x1, x); y0 = jmin (y0, y); y1 = jmax (y1, y); }
            return x1 < 0 ? Rectangle<int> (0, 0, kW, kH) : Rectangle<int> (x0, y0, x1 - x0 + 1, y1 - y0 + 1);
        }

        static Outline outline (const Image& im)
        {
            Outline o; const int w = im.getWidth(), h = im.getHeight();
            o.l.assign ((size_t) h, -1.0f); o.r.assign ((size_t) h, -1.0f);
            const Image::BitmapData bd (im, Image::BitmapData::readOnly);
            o.top = h; o.bottom = -1;
            for (int y = 0; y < h; ++y)
            {
                int a = -1, b = -1;
                for (int x = 0; x < w; ++x) if (bd.getPixelColour (x, y).getAlpha() > 6) { if (a < 0) a = x; b = x; }
                if (a >= 0) { o.l[(size_t) y] = (float) a; o.r[(size_t) y] = (float) b; o.top = jmin (o.top, y); o.bottom = jmax (o.bottom, y); }
            }
            if (o.bottom < 0) { o.top = 0; o.bottom = h - 1; for (int y = 0; y < h; ++y) { o.l[(size_t) y] = 0; o.r[(size_t) y] = (float) (w - 1); } }
            // lignes vides au milieu (trous) : reprend la ligne précédente
            for (int y = o.top + 1; y <= o.bottom; ++y)
                if (o.l[(size_t) y] < 0) { o.l[(size_t) y] = o.l[(size_t) y - 1]; o.r[(size_t) y] = o.r[(size_t) y - 1]; }
            return o;
        }

        static PixelARGB sample (const Image::BitmapData& bd, int w, int h, float x, float y)
        {
            x = jlimit (0.0f, (float) w - 1.001f, x); y = jlimit (0.0f, (float) h - 1.001f, y);
            const int x0 = (int) x, y0 = (int) y;
            const float fx = x - (float) x0, fy = y - (float) y0;
            auto px = [&] (int xx, int yy) { return *(const PixelARGB*) bd.getPixelPointer (xx, yy); };
            const PixelARGB a = px (x0, y0), b = px (x0 + 1, y0), c = px (x0, y0 + 1), d = px (x0 + 1, y0 + 1);
            auto mix = [&] (int ca, int cb, int cc, int cd)
            {
                const float t = (float) ca + ((float) cb - (float) ca) * fx, u = (float) cc + ((float) cd - (float) cc) * fx;
                return (uint8) jlimit (0, 255, roundToInt (t + (u - t) * fy));
            };
            PixelARGB o;
            o.setARGB (mix (a.getAlpha(), b.getAlpha(), c.getAlpha(), d.getAlpha()), mix (a.getRed(), b.getRed(), c.getRed(), d.getRed()),
                       mix (a.getGreen(), b.getGreen(), c.getGreen(), d.getGreen()), mix (a.getBlue(), b.getBlue(), c.getBlue(), d.getBlue()));
            return o;
        }

        Image build (float t) const
        {
            const int w = su.getWidth(), h = su.getHeight();
            Image out (Image::ARGB, w, h, true);
            const Image::BitmapData bu (su, Image::BitmapData::readOnly), bd (sd, Image::BitmapData::readOnly);
            Image::BitmapData bo (out, Image::BitmapData::writeOnly);
            const float topT = (float) ou.top + (float) (od.top - ou.top) * t;
            const float botT = (float) ou.bottom + (float) (od.bottom - ou.bottom) * t;
            const float spanT = jmax (1.0f, botT - topT);
            for (int y = jmax (0, (int) std::floor (topT)); y <= jmin (h - 1, (int) std::ceil (botT)); ++y)
            {
                const float v = jlimit (0.0f, 1.0f, ((float) y - topT) / spanT);
                const float yu = (float) ou.top + v * (float) (ou.bottom - ou.top);
                const float yd = (float) od.top + v * (float) (od.bottom - od.top);
                const int ru = jlimit (0, h - 1, roundToInt (yu)), rd = jlimit (0, h - 1, roundToInt (yd));
                const float lu = ou.l[(size_t) ru], rru = ou.r[(size_t) ru], ld = od.l[(size_t) rd], rrd = od.r[(size_t) rd];
                if (lu < 0 && ld < 0) continue;
                const float lt = lu + (ld - lu) * t, rt = rru + (rrd - rru) * t, wt = jmax (1.0f, rt - lt);
                for (int x = jmax (0, (int) std::floor (lt) - 1); x <= jmin (w - 1, (int) std::ceil (rt) + 1); ++x)
                {
                    const float uh = ((float) x - lt) / wt;
                    const PixelARGB cu = sample (bu, w, h, lu + uh * (rru - lu), yu);
                    const PixelARGB cd = sample (bd, w, h, ld + uh * (rrd - ld), yd);
                    auto m = [t] (uint8 a, uint8 b) { return (uint8) jlimit (0, 255, roundToInt ((float) a + ((float) b - (float) a) * t)); };
                    PixelARGB o;
                    o.setARGB (m (cu.getAlpha(), cd.getAlpha()), m (cu.getRed(), cd.getRed()), m (cu.getGreen(), cd.getGreen()), m (cu.getBlue(), cd.getBlue()));
                    *(PixelARGB*) bo.getPixelPointer (x, y) = o;
                }
            }
            return out;
        }

        Image up, down, su, sd;
        Outline ou, od;
        Rectangle<int> box { 0, 0, kW, kH };
        float scale = -1.0f;
        std::array<Image, kFrames> frames;
    };

    //==========================================================================
    // menu de taille de fenêtre (identique à Ring Think)
    bool isDefaultSize (const WharryEditor* ed) { return ed != nullptr && std::abs ((float) ed->getWidth() / (float) kRefW - kDefaultScale) < 0.02f; }

    void addSizeItems (PopupMenu& m, const WharryEditor* ed)
    {
        const int cur = ed != nullptr ? ed->getScaleIndex() : -1;
        m.addItem (904, "67 % (default)", true, isDefaultSize (ed));
        m.addItem (902, "75 %", true, cur == 1);
        m.addItem (903, "100 %", true, cur == 2);
    }

    void handleSizeChoice (int r, WharryEditor* ed)
    {
        if (ed == nullptr || r < 902 || r > 904) return;
        ed->setScaleIndex (r == 904 ? -1 : r - 901);
    }

    struct PresetLook : public LookAndFeel_V4
    {
        PresetLook()
        {
            setColour (ComboBox::backgroundColourId, Colour (0xff141214));
            setColour (ComboBox::textColourId, kText);
            setColour (ComboBox::outlineColourId, Colour (0xff5a1a1c));
            setColour (ComboBox::arrowColourId, kOrange);
            setColour (PopupMenu::backgroundColourId, Colour (0xff1a1718));
            setColour (PopupMenu::textColourId, kText);
            setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff7a1218));
            setColour (PopupMenu::headerTextColourId, kOrange);
        }
        Font getComboBoxFont (ComboBox& b) override { return Font (FontOptions (jmax (10.0f, (float) b.getHeight() * 0.52f), Font::bold)); }
    };
}

//==============================================================================
class PedalView : public Component, private Timer
{
public:
    explicit PedalView (WharryProcessor& p) : proc (p)
    {
        bg.src     = loadPng (BinaryData::background_png, BinaryData::background_pngSize);
        cap.src    = loadPng (BinaryData::cap_big_png, BinaryData::cap_big_pngSize);
        fsL.src    = loadPng (BinaryData::cap_fsL_png, BinaryData::cap_fsL_pngSize);
        fsR.src    = loadPng (BinaryData::cap_fsR_png, BinaryData::cap_fsR_pngSize);
        ledOnR.src = loadPng (BinaryData::led_r_on_png, BinaryData::led_r_on_pngSize);
        ledOffR.src= loadPng (BinaryData::led_r_off_png, BinaryData::led_r_off_pngSize);
        ledOnO.src = loadPng (BinaryData::led_p_on_png, BinaryData::led_p_on_pngSize);
        ledOffO.src= loadPng (BinaryData::led_p_off_png, BinaryData::led_p_off_pngSize);
        ledOnS.src = loadPng (BinaryData::led_s_on_png, BinaryData::led_s_on_pngSize);
        ledOffS.src= loadPng (BinaryData::led_s_off_png, BinaryData::led_s_off_pngSize);
        treadle.setSources (loadPng (BinaryData::treadle_up_png, BinaryData::treadle_up_pngSize),
                            loadPng (BinaryData::treadle_down_png, BinaryData::treadle_down_pngSize));

        presetBox.setLookAndFeel (&look);
        presetBox.addSectionHeading ("Whammy programs");
        for (int i = 0; i < wharry::kNumPrograms; ++i) presetBox.addItem (wharry::program (i).name, 1 + i);
        presetBox.addSectionHeading ("Studio Wiga factory presets");
        for (int i = 0; i < WharryProcessor::kNumFactory; ++i) presetBox.addItem (WharryProcessor::factory (i).name, 101 + i);
        presetBox.onChange = [this]
        {
            const int id = presetBox.getSelectedId();
            if (id >= 101) { shownFactory = id - 101; proc.loadFactory (id - 101); }
            else if (id >= 1) { shownFactory = -1; proc.selectWhammyProgram (id - 1); }
        };
        addAndMakeVisible (presetBox);
        syncPresetBox (true);

        setOpaque (false);
        startTimerHz (30);
    }
    ~PedalView() override { presetBox.setLookAndFeel (nullptr); }

    void resized() override
    {
        s = (float) getWidth() / (float) kW;
        presetBox.setBounds (r (kPresetBox).toNearestInt());
        repaint();
    }

    void paint (Graphics& g) override
    {
        const float ps = s * physScale();
        g.drawImage (bg.get (ps), getLocalBounds().toFloat());

        // ---- plateau (deux positions voisines fondues -> mouvement continu) ----
        treadle.prepare (ps);
        const float fpos = drawnPedal * (float) (kFrames - 1);
        const int f0 = jlimit (0, kFrames - 1, (int) std::floor (fpos)), f1 = jmin (kFrames - 1, f0 + 1);
        const float fr = fpos - (float) f0;
        const auto tb = r (treadle.designBounds().toFloat());
        g.setOpacity (1.0f);
        g.drawImage (treadle.frame (f0), tb);
        if (fr > 0.02f && f1 != f0) { g.setOpacity (fr); g.drawImage (treadle.frame (f1), tb); g.setOpacity (1.0f); }

        // ---- sélecteur : chapeau fixe (lumière fixe), seul le repère tourne ----
        const auto& capImg = cap.get (ps * (2.0f * kKnobR) / (float) cap.src.getWidth());
        g.drawImage (capImg, r ({ kKnobX - kKnobR, kKnobY - kKnobR, 2.0f * kKnobR, 2.0f * kKnobR }));
        const float ang = degreesToRadians (-kKnobArc + 2.0f * kKnobArc * (float) drawnProgram / (float) (wharry::kNumPrograms - 1));
        const Point<float> c = r (Point<float> (kKnobX, kKnobY));
        const Point<float> a = c + Point<float> (std::sin (ang), -std::cos (ang)) * (kKnobR * 0.30f * s);
        const Point<float> b = c + Point<float> (std::sin (ang), -std::cos (ang)) * (kKnobR * 0.82f * s);
        g.setColour (Colours::black.withAlpha (0.45f)); g.drawLine ({ a.translated (1.2f * s, 1.6f * s), b.translated (1.2f * s, 1.6f * s) }, 6.0f * s);
        g.setColour (Colour (0xfff7f4ef)); g.drawLine ({ a, b }, 4.5f * s);

        // ---- LED des programmes ----
        for (int i = 0; i < wharry::kNumPrograms; ++i)
            drawLed (g, i == drawnProgram && drawnActive ? ledOnS : ledOffS, ledOffS, progLed (i), 0.42f, i == drawnProgram ? (drawnActive ? 1.0f : 0.35f) : 0.0f, kOrange);

        // ---- inverseur CLASSIC / CHORDS ----
        {
            const auto sw = r (kSwitch);
            const float lx = drawnChords ? sw.getRight() - sw.getHeight() * 0.5f : sw.getX() + sw.getHeight() * 0.5f;
            const float rad = sw.getHeight() * 0.42f;
            g.setColour (Colours::black.withAlpha (0.5f)); g.fillEllipse (lx - rad + 1.5f * s, sw.getCentreY() - rad + 2.0f * s, 2 * rad, 2 * rad);
            ColourGradient cg (Colour (0xfff4f4f4), lx - rad * 0.4f, sw.getCentreY() - rad * 0.6f, Colour (0xff6c6c70), lx + rad, sw.getCentreY() + rad, true);
            g.setGradientFill (cg); g.fillEllipse (lx - rad, sw.getCentreY() - rad, 2 * rad, 2 * rad);
        }

        // ---- footswitchs et leurs LED ----
        drawFootswitch (g, fsL, kFsLX, pressedL);
        drawFootswitch (g, fsR, kFsRX, pressedR);
        drawLed (g, ledOnO, ledOffO, { kLedLX, kLedY }, 0.8f, drawnChords ? 1.0f : 0.0f, kOrange);
        drawLed (g, ledOnR, ledOffR, { kLedRX, kLedY }, 0.8f, drawnActive ? 1.0f : 0.0f, kRed);

        // ---- ligne d'état ----
        g.setColour (kText);
        g.setFont (Font (FontOptions (15.0f * s, Font::bold)));
        g.drawFittedText (drawnStatus, r (kStatus).toNearestInt(), Justification::centred, 1);
    }

    // ------------------------------------------------------------ souris
    void mouseDown (const MouseEvent& e) override
    {
        const auto p = e.position / s;
        if (e.mods.isPopupMenu()) { showMenu (p); return; }
        drag = Drag::None;
        if (p.getDistanceFrom ({ kKnobX, kKnobY }) < kKnobR * 1.1f) { drag = Drag::Knob; dragStartY = p.y; dragStartProg = currentProgram(); return; }
        for (int i = 0; i < wharry::kNumPrograms; ++i) if (progLabel (i).contains (p)) { proc.selectWhammyProgram (i); return; }
        if (kSwitchHit.contains (p)) { proc.togglePedalSwitch ("chords"); return; }
        if (p.getDistanceFrom ({ kFsLX, kFsY }) < 52.0f) { pressedL = true; proc.togglePedalSwitch ("chords"); repaint (fsArea (kFsLX)); return; }
        if (p.getDistanceFrom ({ kFsRX, kFsY }) < 52.0f) { pressedR = true; proc.togglePedalSwitch ("active"); repaint (fsArea (kFsRX)); return; }
        if (treadle.designBounds().toFloat().contains (p))
        {
            drag = Drag::Pedal; dragStartY = p.y;
            dragStartPedal = proc.apvts.getRawParameterValue ("pedal")->load();
            if (auto* par = proc.apvts.getParameter ("pedal")) par->beginChangeGesture();
        }
    }

    void mouseDrag (const MouseEvent& e) override
    {
        const auto p = e.position / s;
        if (drag == Drag::Knob)
        {
            const int step = (int) std::lround ((dragStartY - p.y) / 16.0f);
            const int np = jlimit (0, wharry::kNumPrograms - 1, dragStartProg + step);
            if (np != currentProgram()) proc.selectWhammyProgram (np);
        }
        else if (drag == Drag::Pedal)
        {
            // tirer vers le bas = appuyer sur la pointe (orteil enfoncé)
            const float span = (float) treadle.designBounds().getHeight() * 0.8f;
            const float v = jlimit (0.0f, 1.0f, dragStartPedal + (p.y - dragStartY) / span);
            if (auto* par = proc.apvts.getParameter ("pedal")) par->setValueNotifyingHost (v);
        }
    }

    void mouseUp (const MouseEvent&) override
    {
        if (drag == Drag::Pedal) if (auto* par = proc.apvts.getParameter ("pedal")) par->endChangeGesture();
        drag = Drag::None;
        if (pressedL || pressedR) { pressedL = pressedR = false; repaint (fsArea (kFsLX)); repaint (fsArea (kFsRX)); }
    }

    void mouseDoubleClick (const MouseEvent& e) override
    {
        const auto p = e.position / s;
        if (treadle.designBounds().toFloat().contains (p))          // double-clic : pédale tout en haut / tout en bas
            proc.setParamValue ("pedal", proc.apvts.getRawParameterValue ("pedal")->load() > 0.5f ? 0.0f : 1.0f);
    }

    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        const auto p = e.position / s;
        if (p.getDistanceFrom ({ kKnobX, kKnobY }) < kKnobR * 1.2f)
            proc.selectWhammyProgram (jlimit (0, wharry::kNumPrograms - 1, currentProgram() + (w.deltaY > 0 ? 1 : -1)));
        else if (treadle.designBounds().toFloat().contains (p))
            proc.setParamValue ("pedal", jlimit (0.0f, 1.0f, proc.apvts.getRawParameterValue ("pedal")->load() - w.deltaY * 0.25f));
    }

private:
    enum class Drag { None, Knob, Pedal };

    Rectangle<float> r (Rectangle<float> d) const { return d * s; }
    Point<float> r (Point<float> d) const { return d * s; }
    Rectangle<int> fsArea (float x) const { return r (Rectangle<float> (x - 60.0f, kFsY - 60.0f, 120.0f, 120.0f)).toNearestInt().expanded (2); }
    float physScale() const
    {
        if (auto* d = Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds())) return jmax (1.0f, (float) d->scale);
        return 1.0f;
    }
    int currentProgram() const { return jlimit (0, wharry::kNumPrograms - 1, (int) std::lround (proc.apvts.getRawParameterValue ("program")->load())); }
    bool param (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }

    void drawLed (Graphics& g, ScaledCache& on, ScaledCache& off, Point<float> c, float sc, float level, Colour glow)
    {
        const float ps = s * physScale();
        const auto& o = off.get (ps * sc);
        const float w = (float) off.src.getWidth() * sc * s, h = (float) off.src.getHeight() * sc * s;
        const Rectangle<float> rr (c.x * s - w * 0.5f, c.y * s - h * 0.5f, w, h);
        if (level > 0.01f)
        {
            g.setGradientFill (ColourGradient (glow.withAlpha (0.35f * level), c * s, glow.withAlpha (0.0f), c * s + Point<float> (w * 1.2f, 0), true));
            g.fillEllipse (rr.expanded (w * 0.7f));
        }
        g.drawImage (o, rr);
        if (level > 0.01f) { g.setOpacity (level); g.drawImage (on.get (ps * sc), rr); g.setOpacity (1.0f); }
    }

    void drawFootswitch (Graphics& g, ScaledCache& img, float x, bool pressed)
    {
        const float sc = kFsScale * (pressed ? 0.95f : 1.0f);
        const float w = (float) img.src.getWidth() * sc, h = (float) img.src.getHeight() * sc;
        g.drawImage (img.get (s * physScale() * sc), r ({ x - w * 0.5f, kFsY - h * 0.5f + (pressed ? 2.0f : 0.0f), w, h }));
    }

    String statusText() const
    {
        const auto learn = proc.getLearnTarget();
        if (learn == MidiTarget::Pedal)   return "MIDI LEARN: move your expression pedal";
        if (learn == MidiTarget::LeftFsw) return "MIDI LEARN: press the footswitch for CLASSIC / CHORDS";
        if (learn == MidiTarget::RightFsw)return "MIDI LEARN: press the footswitch for ON / OFF";
        const auto& pr = wharry::program (currentProgram());
        String st = String (pr.label) + "   |   " + (param ("chords") ? (param ("studio") ? "CHORDS HQ" : "CHORDS") : "CLASSIC")
                  + "   |   PEDAL " + String (roundToInt (proc.getPedalShown() * 100.0f)) + " %";
        if (pr.type == wharry::Detune) st << "   |   DETUNE " << String (roundToInt (pr.up)) << " CENTS";
        else
        {
            const float sm = proc.getSemitones();
            st << "   |   " << (sm >= 0.0f ? "+" : "") << String (sm, 1) << " ST";
        }
        if (param ("chords") && param ("studio") && proc.getLatency() > 0)
            st << "   |   LATENCY " << String (roundToInt (1000.0 * proc.getLatency() / jmax (1.0, proc.getSampleRate()))) << " MS";
        st << "   |   " << (param ("active") ? "ON" : "BYPASS");
        return st;
    }

    void syncPresetBox (bool force)
    {
        const int prog = currentProgram();
        if (! force && prog == shownProgram) return;
        shownProgram = prog;
        if (shownFactory >= 0 && WharryProcessor::factory (shownFactory).program == prog)
            presetBox.setSelectedId (101 + shownFactory, dontSendNotification);
        else { shownFactory = -1; presetBox.setSelectedId (1 + prog, dontSendNotification); }
    }

    void timerCallback() override
    {
        if (auto* ed = findParentComponentOfClass<WharryEditor>()) ed->useSoftwareRenderer();

        const int prog = currentProgram();
        const bool chords = param ("chords"), active = param ("active");
        const float ped = proc.getPedalShown();
        const String st = statusText();
        syncPresetBox (false);

        if (std::abs (ped - drawnPedal) > 0.002f)
        {
            drawnPedal = ped;
            repaint (r (treadle.designBounds().toFloat()).toNearestInt().expanded (2));
        }
        if (prog != drawnProgram)
        {
            repaint (r ({ kKnobX - kKnobR - 6, kKnobY - kKnobR - 6, 2 * kKnobR + 12, 2 * kKnobR + 12 }).toNearestInt());
            for (int i : { prog, drawnProgram }) if (i >= 0) repaint (r (Rectangle<float> (progLed (i).x - 14, progLed (i).y - 14, 28, 28)).toNearestInt());
            drawnProgram = prog;
        }
        if (chords != drawnChords || active != drawnActive)
        {
            drawnChords = chords; drawnActive = active;
            repaint (r (kSwitch.expanded (8)).toNearestInt());
            repaint (r (Rectangle<float> (kLedLX - 50, kLedY - 50, 100, 100)).toNearestInt());
            repaint (r (Rectangle<float> (kLedRX - 50, kLedY - 50, 100, 100)).toNearestInt());
            repaint (r (Rectangle<float> (progLed (prog).x - 14, progLed (prog).y - 14, 28, 28)).toNearestInt());
        }
        if (st != drawnStatus) { drawnStatus = st; repaint (r (kStatus).toNearestInt().expanded (2)); }
    }

    void showMenu (Point<float> p)
    {
        auto* ed = findParentComponentOfClass<WharryEditor>();
        PopupMenu m;
        const bool onTreadle = treadle.designBounds().toFloat().contains (p);
        const bool onLeft = p.getDistanceFrom ({ kFsLX, kFsY }) < 60.0f, onRight = p.getDistanceFrom ({ kFsRX, kFsY }) < 60.0f;
        if (onLeft || onRight)
        {
            const auto t = onLeft ? MidiTarget::LeftFsw : MidiTarget::RightFsw;
            const auto d = proc.getMidiDescription (t);
            m.addSectionHeader (onLeft ? "CLASSIC / CHORDS footswitch" : "ON / OFF footswitch");
            m.addItem (onLeft ? 11 : 21, "MIDI learn" + (d.isNotEmpty() ? " (now: " + d + ")" : String()));
            m.addItem (onLeft ? 12 : 22, "Clear MIDI assignment", d.isNotEmpty());
            m.addSeparator();
        }
        else
        {
            m.addSectionHeader ("Expression pedal (" + proc.getMidiDescription (MidiTarget::Pedal) + ")");
            m.addItem (31, "MIDI learn pedal controller");
            m.addItem (32, "Reset to CC 11 (Whammy standard)");
            m.addItem (33, "Invert pedal direction", true, param ("invert"));
            m.addItem (34, "Reset heel / toe calibration");
            PopupMenu sm;
            const float cur = proc.apvts.getRawParameterValue ("smooth")->load();
            int id = 40;
            for (float v : { 0.0f, 15.0f, 30.0f, 60.0f, 120.0f })
                sm.addItem (id++, v == 0.0f ? String ("Off") : String (roundToInt (v)) + " ms", true, std::abs (cur - v) < 0.5f);
            m.addSubMenu ("Pedal smoothing", sm);
            m.addSeparator();
            m.addSectionHeader ("Chords engine");
            m.addItem (51, "Live (no latency)", true, ! param ("studio"));
            m.addItem (52, "Studio HQ (latency, for re-amping / mixing)", true, param ("studio"));
            m.addItem (53, "Respond to MIDI Program Change", true, param ("pc"));
            m.addSeparator();
        }
        ignoreUnused (onTreadle);
        m.addSectionHeader ("Plugin size");
        addSizeItems (m, ed);

        Component::SafePointer<PedalView> sp (this);
        m.showMenuAsync (PopupMenu::Options(), [sp] (int res)
        {
            if (sp == nullptr || res == 0) return;
            auto& pr = sp->proc;
            switch (res)
            {
                case 11: pr.startMidiLearn (MidiTarget::LeftFsw); break;
                case 12: pr.clearMidi (MidiTarget::LeftFsw); break;
                case 21: pr.startMidiLearn (MidiTarget::RightFsw); break;
                case 22: pr.clearMidi (MidiTarget::RightFsw); break;
                case 31: pr.startMidiLearn (MidiTarget::Pedal); break;
                case 32: pr.clearMidi (MidiTarget::Pedal); break;
                case 33: pr.togglePedalSwitch ("invert"); break;
                case 34: pr.setParamValue ("heel", 0.0f); pr.setParamValue ("toe", 1.0f); break;
                case 51: pr.setParamValue ("studio", 0.0f); break;
                case 52: pr.setParamValue ("studio", 1.0f); break;
                case 53: pr.togglePedalSwitch ("pc"); break;
                default:
                    if (res >= 40 && res <= 44) { const float v[5] = { 0, 15, 30, 60, 120 }; pr.setParamValue ("smooth", v[res - 40]); }
                    else handleSizeChoice (res, sp->findParentComponentOfClass<WharryEditor>());
                    break;
            }
        });
    }

    WharryProcessor& proc;
    PresetLook look;
    ComboBox presetBox;
    ScaledCache bg, cap, fsL, fsR, ledOnR, ledOffR, ledOnO, ledOffO, ledOnS, ledOffS;
    TreadleMorph treadle;
    float s = kDefaultScale * (float) kRefW / (float) kW;

    Drag drag = Drag::None;
    float dragStartY = 0, dragStartPedal = 0;
    int dragStartProg = 0;
    bool pressedL = false, pressedR = false;

    float drawnPedal = -1.0f;
    int drawnProgram = -1, shownProgram = -1, shownFactory = -1;
    bool drawnChords = false, drawnActive = true;
    String drawnStatus;
};

//==============================================================================
WharryEditor::WharryEditor (WharryProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    view = std::make_unique<PedalView> (proc);
    addAndMakeVisible (*view);
    setResizable (true, true);
    const int minW = (int) std::lround (kRefW * kDefaultScale);          // jamais plus petit que la taille par défaut
    const int maxW = 1500;
    setResizeLimits (minW, (int) std::lround ((double) minW * kH / kW), maxW, (int) std::lround ((double) maxW * kH / kW));
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setScaleIndex (jlimit (-1, 2, proc.uiScaleIdx.load()));
}

WharryEditor::~WharryEditor() = default;

// Anti-gel de la fenêtre (Windows) : JUCE 8 dessine par défaut avec Direct2D ; avec certains pilotes (Intel)
// la fenêtre cesse de se rafraîchir alors que le DAW continue. Le moteur logiciel est stable partout.
void WharryEditor::useSoftwareRenderer()
{
   #if JUCE_WINDOWS
    if (auto* peer = getPeer())
    {
        int sw = peer->getAvailableRenderingEngines().indexOf ("Software Renderer");
        if (sw < 0) sw = 0;
        if (peer->getCurrentRenderingEngine() != sw)
            peer->setCurrentRenderingEngine (sw);
    }
   #endif
}

void WharryEditor::parentHierarchyChanged() { useSoftwareRenderer(); }

void WharryEditor::resized()
{
    // pas de redimensionnement forcé ici (tolérance de l'hôte à 125 / 150 %) : les limites du constrainer suffisent
    view->setBounds (getLocalBounds());
}

int WharryEditor::getScaleIndex() const
{
    const float sc = (float) getWidth() / (float) kRefW;
    for (int i = 1; i < 3; ++i) if (std::abs (sc - kScales[i]) < 0.02f) return i;
    return -1;
}

void WharryEditor::setScaleIndex (int idx)
{
    idx = jlimit (-1, 2, idx);
    if (idx == 0) idx = -1;
    proc.uiScaleIdx = idx;
    const float sc = idx < 0 ? kDefaultScale : kScales[idx];
    const int w = (int) std::lround (kRefW * sc);
    setSize (w, (int) std::lround ((double) w * kH / kW));
}
