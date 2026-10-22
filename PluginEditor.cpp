#include "PluginEditor.h"
#include <BinaryData.h>

using namespace juce;

namespace
{
    // Repère du dessin = taille de background.png (pédale + marges latérales pour les jacks). Toutes les coordonnées sont dans ces pixels.
    // fond recadré au plus près de la pédale ; mêmes hauteurs de fenêtre que Ring Think (495 / 557 / 743 px),
    // largeur selon les proportions de la pédale -> par défaut 544 x 495, jamais plus grand que Ring Think (661 x 495)
    constexpr int kW = 1020, kH = 928;
    constexpr float kRefH = 743.0f;                              // hauteur « 100 % » (= Ring Think à 100 %)
    const float kScales[3] = { 495.0f / 743.0f, 0.75f, 1.0f };
    constexpr float kDefaultScale = 495.0f / 743.0f;             // fenêtre par défaut (= taille minimale)

    // ---- emplacements des commandes dans le repère 1240 x 928 (générés par tools/design_art.py -> tools/layout.txt) ----
    constexpr float kLedR = 6.6f;
    const Point<float> kKnob (616.7f, 315.5f);  constexpr float kKnobR = 61.5f, kCapR = 45.7f;   // sélecteur 21 crans
    const Point<float> kSmall[3] = { { 620.3f, 530.8f }, { 737.1f, 530.8f }, { 854.8f, 528.2f } };   // GLIDE, TONE, MIX
    constexpr float kSmallR = 42.2f, kSmallCapR = 32.5f;
    const char* const kSmallId[3] = { "smooth", "tone", "mix" };
    const Rectangle<float> kScreen (709.0f, 198.7f, 223.2f, 247.7f);
    const Rectangle<float> kFs (668.6f, 679.3f, 133.5f, 133.5f);          // footswitch ON / OFF
    const Point<float> kFsLed (839.9f, 757.5f);
    const Rectangle<float> kToggle (856.6f, 113.5f, 54.5f, 38.6f);       // inverseur CLASSIC / CHORDS
    const Rectangle<float> kToggleHit = kToggle.expanded (14.0f, 22.0f);
    const Rectangle<float> kPlate (84.3f, 35.3f, 443.7f, 844.3f);   // plateau
    const Colour kInk (0xffeee6e4), kWarm (0xffffc47a), kCyan (0xff5fe3ff), kScreenBg (0xff081018);
    constexpr int kFrames = 33;                                                                // positions précalculées du plateau

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
    // Plateau de la pédale : il pivote autour de la charnière (côté talon, en bas). Chaque position est une
    // perspective exacte du calque « orteil enfoncé » (pas de fondu entre deux images -> aucun dédoublement).
    constexpr float kPlateL = 84.3f, kPlateR = 528.0f, kPlateTop = 35.3f, kHingeY = 879.6f;   // plateau à plat (orteil enfoncé)
    constexpr float kRise = 26.0f, kWiden = 1.05f;                                            // orteil levé : pointe plus haute et plus large

    struct Homography
    {
        double h[9] {};
        // calcule H tel que H(dst[i]) = src[i] (4 couples de points)
        bool solve (const Point<double> dst[4], const Point<double> src[4])
        {
            double A[8][9] {};
            for (int i = 0; i < 4; ++i)
            {
                const double x = dst[i].x, y = dst[i].y, u = src[i].x, v = src[i].y;
                double r1[9] = { x, y, 1, 0, 0, 0, -u * x, -u * y, u };
                double r2[9] = { 0, 0, 0, x, y, 1, -v * x, -v * y, v };
                std::copy (r1, r1 + 9, A[2 * i]); std::copy (r2, r2 + 9, A[2 * i + 1]);
            }
            for (int c = 0; c < 8; ++c)
            {
                int piv = c;
                for (int r = c + 1; r < 8; ++r) if (std::abs (A[r][c]) > std::abs (A[piv][c])) piv = r;
                if (std::abs (A[piv][c]) < 1e-12) return false;
                for (int k = 0; k < 9; ++k) std::swap (A[c][k], A[piv][k]);
                for (int r = 0; r < 8; ++r)
                    if (r != c)
                    {
                        const double f = A[r][c] / A[c][c];
                        for (int k = c; k < 9; ++k) A[r][k] -= f * A[c][k];
                    }
            }
            for (int i = 0; i < 8; ++i) h[i] = A[i][8] / A[i][i];
            h[8] = 1.0;
            return true;
        }
        Point<double> map (double x, double y) const
        {
            const double w = h[6] * x + h[7] * y + h[8];
            return { (h[0] * x + h[1] * y + h[2]) / w, (h[3] * x + h[4] * y + h[5]) / w };
        }
    };

    class TreadleWarp
    {
    public:
        void setSource (Image downImg)
        {
            down = std::move (downImg);
            const float cx = 0.5f * (kPlateL + kPlateR);
            box = Rectangle<float> (cx - (cx - kPlateL) * kWiden - 4.0f, kPlateTop - kRise - 4.0f,
                                    (kPlateR - kPlateL) * kWiden + 40.0f, kHingeY - kPlateTop + kRise + 40.0f)
                      .getSmallestIntegerContainer().getIntersection ({ 0, 0, kW, kH });
            scale = -1.0f;
        }
        Rectangle<int> designBounds() const { return box; }

        void prepare (float s)
        {
            if (std::abs (s - scale) < 1.0e-4f) return;
            scale = s;
            const int bw = jmax (1, roundToInt ((float) box.getWidth() * s)), bh = jmax (1, roundToInt ((float) box.getHeight() * s));
            src = down.getClippedImage (box).rescaled (bw, bh, Graphics::highResamplingQuality).convertedToFormat (Image::ARGB);
            for (auto& f : frames) f = {};
            }

        // t = 0 : orteil levé, t = 1 : orteil enfoncé
        const Image& frame (int i)
        {
            i = jlimit (0, kFrames - 1, i);
            if (! frames[(size_t) i].isValid()) frames[(size_t) i] = build ((float) i / (float) (kFrames - 1));
            return frames[(size_t) i];
        }

    private:
        Image build (float t) const
        {
            const int w = src.getWidth(), h = src.getHeight();
            const double sx = (double) w / box.getWidth(), sy = (double) h / box.getHeight();
            auto loc = [&] (double x, double y) { return Point<double> ((x - box.getX()) * sx, (y - box.getY()) * sy); };
            const double cx = 0.5 * (kPlateL + kPlateR), lift = 1.0 - t;
            const double wid = 1.0 + (kWiden - 1.0) * lift, rise = kRise * lift;
            const Point<double> srcQ[4] = { loc (kPlateL, kPlateTop), loc (kPlateR, kPlateTop), loc (kPlateR, kHingeY), loc (kPlateL, kHingeY) };
            const Point<double> dstQ[4] = { loc (cx - (cx - kPlateL) * wid, kPlateTop - rise), loc (cx + (kPlateR - cx) * wid, kPlateTop - rise),
                                            loc (kPlateR, kHingeY), loc (kPlateL, kHingeY) };
            Image plate (Image::ARGB, w, h, true);
            Homography H;
            if (lift < 1.0e-4 || ! H.solve (dstQ, srcQ)) plate = src.createCopy();
            else
            {
                const Image::BitmapData bs (src, Image::BitmapData::readOnly);
                Image::BitmapData bo (plate, Image::BitmapData::writeOnly);
                const double topY = dstQ[0].y, botY = dstQ[3].y;
                for (int y = 0; y < h; ++y)
                {
                    // la partie avant, inclinée vers la lumière (en haut à gauche), s'éclaircit un peu
                    const double gy = jlimit (0.0, 1.0, (botY - (double) y) / jmax (1.0, botY - topY));
                    const float gain = (float) (1.0 + 0.10 * lift * std::pow (gy, 1.5));
                    for (int x = 0; x < w; ++x)
                    {
                        const auto q = H.map (x + 0.5, y + 0.5);
                        const double u = q.x - 0.5, v = q.y - 0.5;
                        if (u < -1.0 || v < -1.0 || u > w || v > h) continue;
                        *(PixelARGB*) bo.getPixelPointer (x, y) = sample (bs, w, h, (float) u, (float) v, gain);
                    }
                }
            }
            if (lift < 1.0e-4) return plate;
            // tranche droite du plateau, visible quand la pointe se lève (épaisse côté pointe, nulle à la charnière)
            {
                Image::BitmapData bo (plate, Image::BitmapData::readWrite);
                const double maxFace = 9.0 * sx * lift;
                const double ty = dstQ[1].y, by = dstQ[2].y;
                for (int y = jmax (0, (int) std::ceil (ty)); y <= jmin (h - 1, (int) std::floor (by)); ++y)
                {
                    const double v = ((double) y - ty) / jmax (1.0, by - ty);          // 0 = pointe, 1 = charnière
                    const double xr = dstQ[1].x + (dstQ[2].x - dstQ[1].x) * v;
                    const double fw = maxFace * (1.0 - v);
                    if (fw < 0.3) continue;
                    for (int x = (int) std::floor (xr); x <= jmin (w - 1, (int) std::ceil (xr + fw)); ++x)
                    {
                        if (x < 0) continue;
                        const double u = ((double) x - xr) / fw;                     // 0 = arête, 1 = bas de la tranche
                        if (u < 0.0 || u > 1.0) continue;
                        auto* px = (PixelARGB*) bo.getPixelPointer (x, y);
                        const float k = (float) jlimit (0.0, 1.0, std::min (u * fw, (1.0 - u) * fw) + 0.5);   // bords adoucis
                        const int g0 = (int) (34 + 46 * (1.0 - u) + 25 * (1.0 - v));   // métal sombre, plus clair près de l'arête
                        // tranche opaque posée par-dessus (pixels prémultipliés : out = tranche * k + existant * (1 - k))
                        auto mixc = [k] (int a, int b) { return (uint8) jlimit (0, 255, roundToInt ((float) a * k + (float) b * (1.0f - k))); };
                        PixelARGB o;
                        o.setARGB (mixc (255, px->getAlpha()), mixc (jmin (255, g0 + 6), px->getRed()), mixc (g0, px->getGreen()), mixc (jmin (255, g0 + 4), px->getBlue()));
                        *px = o;
                    }
                }
            }
            // ombre portée sous la pointe levée (lumière en haut à gauche -> ombre vers la droite / le bas)
            const int ox = roundToInt (8.0 * sx * lift), oy = roundToInt (11.0 * sy * lift), rad = jmax (1, roundToInt (6.0 * sx));
            std::vector<float> sh ((size_t) (w * h), 0.0f), tmp ((size_t) (w * h), 0.0f);
            {
                const Image::BitmapData bp (plate, Image::BitmapData::readOnly);
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x)
                    {
                        const int xs = x - ox, ys = y - oy;
                        if (xs >= 0 && ys >= 0 && xs < w && ys < h)
                            sh[(size_t) (y * w + x)] = (float) ((const PixelARGB*) bp.getPixelPointer (xs, ys))->getAlpha() / 255.0f;
                    }
            }
            const float norm = 1.0f / (float) (2 * rad + 1);
            for (int pass = 0; pass < 2; ++pass)      // flou boîte séparable, 2 passes (~ gaussien)
            {
                for (int y = 0; y < h; ++y)
                {
                    float acc = 0.0f;
                    for (int x = -rad; x < w + rad; ++x)
                    {
                        if (x + rad < w) acc += sh[(size_t) (y * w + x + rad)];
                        if (x - rad - 1 >= 0) acc -= sh[(size_t) (y * w + x - rad - 1)];
                        if (x >= 0 && x < w) tmp[(size_t) (y * w + x)] = acc * norm;
                    }
                }
                for (int x = 0; x < w; ++x)
                {
                    float acc = 0.0f;
                    for (int y = -rad; y < h + rad; ++y)
                    {
                        if (y + rad < h) acc += tmp[(size_t) ((y + rad) * w + x)];
                        if (y - rad - 1 >= 0) acc -= tmp[(size_t) ((y - rad - 1) * w + x)];
                        if (y >= 0 && y < h) sh[(size_t) (y * w + x)] = acc * norm;
                    }
                }
            }
            Image out (Image::ARGB, w, h, true);
            {
                const Image::BitmapData bp (plate, Image::BitmapData::readOnly);
                Image::BitmapData bo (out, Image::BitmapData::writeOnly);
                const float strength = (float) (0.55 * lift);
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x)
                    {
                        const PixelARGB pp = *(const PixelARGB*) bp.getPixelPointer (x, y);
                        const float pa = (float) pp.getAlpha() / 255.0f;
                        const float sa = sh[(size_t) (y * w + x)] * strength * (1.0f - pa);
                        PixelARGB o;   // prémultiplié : l'ombre noire n'ajoute que de l'alpha
                        o.setARGB ((uint8) jlimit (0, 255, roundToInt ((pa + sa) * 255.0f)), pp.getRed(), pp.getGreen(), pp.getBlue());
                        *(PixelARGB*) bo.getPixelPointer (x, y) = o;
                    }
            }
            return out;
        }

        static PixelARGB sample (const Image::BitmapData& bd, int w, int h, float x, float y, float gain)
        {
            const int x0 = (int) std::floor (x), y0 = (int) std::floor (y);
            const float fx = x - (float) x0, fy = y - (float) y0;
            auto px = [&] (int xx, int yy) -> PixelARGB
            {
                if (xx < 0 || yy < 0 || xx >= w || yy >= h) { PixelARGB z; z.setARGB (0, 0, 0, 0); return z; }
                return *(const PixelARGB*) bd.getPixelPointer (xx, yy);
            };
            const PixelARGB a = px (x0, y0), b = px (x0 + 1, y0), c = px (x0, y0 + 1), d = px (x0 + 1, y0 + 1);
            auto mix = [&] (int ca, int cb, int cc, int cd, float k)
            {
                const float t = (float) ca + ((float) cb - (float) ca) * fx, u = (float) cc + ((float) cd - (float) cc) * fx;
                return (uint8) jlimit (0, 255, roundToInt ((t + (u - t) * fy) * k));
            };
            PixelARGB o;   // pixels prémultipliés : la couleur ne peut pas dépasser l'alpha
            const uint8 al = mix (a.getAlpha(), b.getAlpha(), c.getAlpha(), d.getAlpha(), 1.0f);
            o.setARGB (al, jmin (al, mix (a.getRed(), b.getRed(), c.getRed(), d.getRed(), gain)),
                       jmin (al, mix (a.getGreen(), b.getGreen(), c.getGreen(), d.getGreen(), gain)),
                       jmin (al, mix (a.getBlue(), b.getBlue(), c.getBlue(), d.getBlue(), gain)));
            return o;
        }

        Image down, src;
        Rectangle<int> box { 0, 0, kW, kH };
        float scale = -1.0f;
        std::array<Image, kFrames> frames;
    };

    //==========================================================================
    // menu de taille de fenêtre (identique à Ring Think)
    bool isDefaultSize (const WharryEditor* ed) { return ed != nullptr && std::abs ((float) ed->getHeight() / kRefH - kDefaultScale) < 0.02f; }

    void addSizeItems (PopupMenu& m, const WharryEditor* ed)
    {
        const int cur = ed != nullptr ? ed->getScaleIndex() : -1;
        m.addItem (904, "Default (same height as Ring Think)", true, isDefaultSize (ed));
        m.addItem (902, "75 %", true, cur == 1);
        m.addItem (903, "100 %", true, cur == 2);
    }

    void handleSizeChoice (int r, WharryEditor* ed)
    {
        if (ed == nullptr || r < 902 || r > 904) return;
        ed->setScaleIndex (r == 904 ? -1 : r - 901);
    }

}

//==============================================================================
class PedalView : public Component, private Timer
{
public:
    explicit PedalView (WharryProcessor& p) : proc (p)
    {
        bg.src      = loadPng (BinaryData::background_png, BinaryData::background_pngSize);
        fsOn.src    = loadPng (BinaryData::fs_on_png, BinaryData::fs_on_pngSize);
        fsOff.src   = loadPng (BinaryData::fs_off_png, BinaryData::fs_off_pngSize);
        tgL.src     = loadPng (BinaryData::toggle_l_png, BinaryData::toggle_l_pngSize);
        tgR.src     = loadPng (BinaryData::toggle_r_png, BinaryData::toggle_r_pngSize);
        treadle.setSource (loadPng (BinaryData::treadle_down_png, BinaryData::treadle_down_pngSize));
        typeface = Typeface::createSystemTypefaceFor (BinaryData::PoppinsMedium_ttf, BinaryData::PoppinsMedium_ttfSize);
        typefaceBold = Typeface::createSystemTypefaceFor (BinaryData::PoppinsBold_ttf, BinaryData::PoppinsBold_ttfSize);
        setOpaque (true);
        startTimerHz (30);
    }

    void resized() override { s = (float) getWidth() / (float) kW; repaint(); }

    void paint (Graphics& g) override
    {
        const float ps = s * physScale();
        g.drawImage (bg.get (ps), getLocalBounds().toFloat());

        // ---- plateau (deux positions voisines fondues -> mouvement continu) ----
        treadle.prepare (ps);
        const float fpos = jlimit (0.0f, 1.0f, drawnPedal) * (float) (kFrames - 1);
        const int f0 = jlimit (0, kFrames - 1, (int) std::floor (fpos)), f1 = jmin (kFrames - 1, f0 + 1);
        const float fr = fpos - (float) f0;
        const auto tb = r (treadle.designBounds().toFloat());
        g.setOpacity (1.0f);
        g.drawImage (treadle.frame (f0), tb);
        if (fr > 0.02f && f1 != f0) { g.setOpacity (fr); g.drawImage (treadle.frame (f1), tb); g.setOpacity (1.0f); }

        // ---- sélecteur : le chapeau laiton reste fixe (reflets), seul le repère gravé tourne vers la LED choisie ----
        {
            const float ang = knobAngle (drawnProgram);
            const Point<float> dir (std::sin (ang), -std::cos (ang));
            const auto c = r (kKnob);
            const auto a = c + dir * (kCapR * 0.30f * s), b = c + dir * (kCapR * 0.86f * s);
            g.setColour (Colours::white.withAlpha (0.5f)); g.drawLine ({ a.translated (-0.8f * s, -0.8f * s), b.translated (-0.8f * s, -0.8f * s) }, 2.4f * s);
            g.setColour (Colour (0xe0201a1a)); g.drawLine ({ a, b }, 3.2f * s);
        }

        // ---- GLIDE / TONE / MIX ----
        for (int i = 0; i < 3; ++i)
        {
            const float ang = degreesToRadians (-135.0f + 270.0f * drawnSmall[i]);
            const Point<float> dir (std::sin (ang), -std::cos (ang));
            const auto c = r (kSmall[i]);
            // repère gravé sur le chapeau alu (reflets fixes)
            g.setColour (Colours::white.withAlpha (0.45f));
            g.drawLine ({ c + dir * (kSmallCapR * 0.30f * s) + Point<float> (-0.7f, -0.7f) * s, c + dir * (kSmallCapR * 0.90f * s) + Point<float> (-0.7f, -0.7f) * s }, 2.0f * s);
            g.setColour (Colour (0xe0201a1a));
            g.drawLine ({ c + dir * (kSmallCapR * 0.30f * s), c + dir * (kSmallCapR * 0.90f * s) }, 2.6f * s);
        }

        // ---- inverseur CLASSIC / CHORDS ----
        g.drawImage ((drawnChords ? tgR : tgL).get (ps), r (kToggle));

        // ---- footswitch + LED ----
        {
            auto fr2 = r (kFs);
            if (pressed) fr2 = fr2.withSizeKeepingCentre (fr2.getWidth() * 0.965f, fr2.getHeight() * 0.965f).translated (0, 1.5f * s);
            g.drawImage ((drawnActive ? fsOn : fsOff).get (ps), fr2);
            drawLed (g, kFsLed, drawnActive ? 1.0f : 0.0f);
        }

        drawScreen (g);
    }

    // ------------------------------------------------------------ souris
    void mouseDown (const MouseEvent& e) override
    {
        const auto p = e.position / s;
        if (e.mods.isPopupMenu()) { showMenu (p); return; }
        drag = Drag::None;
        if (kScreen.contains (p)) { showPresetMenu(); return; }
        if (p.getDistanceFrom (kKnob) < kKnobR) { drag = Drag::Knob; dragStartY = p.y; dragStartProg = currentProgram(); return; }
        for (int i = 0; i < 3; ++i)
            if (p.getDistanceFrom (kSmall[i]) < kSmallR * 1.15f)
            {
                drag = (Drag) ((int) Drag::Small0 + i); dragStartY = p.y;
                if (auto* par = proc.apvts.getParameter (kSmallId[i])) { dragStartVal = par->getValue(); par->beginChangeGesture(); }
                return;
            }
        if (kToggleHit.contains (p)) { proc.togglePedalSwitch ("chords"); return; }
        if (kFs.getCentre().getDistanceFrom (p) < kFs.getWidth() * 0.45f) { pressed = true; proc.togglePedalSwitch ("active"); repaint (r (kFs).toNearestInt().expanded (4)); return; }
        if (kPlate.contains (p))
        {
            drag = Drag::Pedal; dragStartY = p.y;
            dragStartVal = proc.apvts.getRawParameterValue ("pedal")->load();
            if (auto* par = proc.apvts.getParameter ("pedal")) par->beginChangeGesture();
        }
    }

    void mouseDrag (const MouseEvent& e) override
    {
        const auto p = e.position / s;
        if (drag == Drag::Knob)
        {
            const int np = jlimit (0, wharry::kNumPrograms - 1, dragStartProg + (int) std::lround ((dragStartY - p.y) / 14.0f));
            if (np != currentProgram()) proc.selectWhammyProgram (np);
        }
        else if (drag == Drag::Pedal)
        {
            // tirer vers le bas = appuyer sur la pointe (orteil enfoncé)
            const float v = jlimit (0.0f, 1.0f, dragStartVal + (p.y - dragStartY) / (kPlate.getHeight() * 0.7f));
            if (auto* par = proc.apvts.getParameter ("pedal")) par->setValueNotifyingHost (v);
        }
        else if (drag >= Drag::Small0)
        {
            const float v = jlimit (0.0f, 1.0f, dragStartVal + (dragStartY - p.y) / 220.0f);
            if (auto* par = proc.apvts.getParameter (kSmallId[(int) drag - (int) Drag::Small0])) par->setValueNotifyingHost (v);
        }
    }

    void mouseUp (const MouseEvent&) override
    {
        if (drag == Drag::Pedal) if (auto* par = proc.apvts.getParameter ("pedal")) par->endChangeGesture();
        if (drag >= Drag::Small0) if (auto* par = proc.apvts.getParameter (kSmallId[(int) drag - (int) Drag::Small0])) par->endChangeGesture();
        drag = Drag::None;
        if (pressed) { pressed = false; repaint (r (kFs).toNearestInt().expanded (4)); }
    }

    void mouseDoubleClick (const MouseEvent& e) override
    {
        const auto p = e.position / s;
        if (kPlate.contains (p))              // double-clic sur le plateau : tout en haut / tout en bas
            proc.setParamValue ("pedal", proc.apvts.getRawParameterValue ("pedal")->load() > 0.5f ? 0.0f : 1.0f);
        for (int i = 0; i < 3; ++i)           // double-clic sur un bouton : valeur par défaut
            if (p.getDistanceFrom (kSmall[i]) < kSmallR * 1.15f)
                if (auto* par = proc.apvts.getParameter (kSmallId[i])) { par->beginChangeGesture(); par->setValueNotifyingHost (par->getDefaultValue()); par->endChangeGesture(); }
    }

    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        const auto p = e.position / s;
        if (p.getDistanceFrom (kKnob) < kKnobR)
            proc.selectWhammyProgram (jlimit (0, wharry::kNumPrograms - 1, currentProgram() + (w.deltaY > 0 ? 1 : -1)));
        else if (kPlate.contains (p))
            proc.setParamValue ("pedal", jlimit (0.0f, 1.0f, proc.apvts.getRawParameterValue ("pedal")->load() - w.deltaY * 0.25f));
        else
            for (int i = 0; i < 3; ++i)
                if (p.getDistanceFrom (kSmall[i]) < kSmallR * 1.15f)
                    if (auto* par = proc.apvts.getParameter (kSmallId[i]))
                    { par->beginChangeGesture(); par->setValueNotifyingHost (jlimit (0.0f, 1.0f, par->getValue() + w.deltaY * 0.1f)); par->endChangeGesture(); }
    }

private:
    enum class Drag { None, Knob, Pedal, Small0, Small1, Small2 };

    Rectangle<float> r (Rectangle<float> d) const { return d * s; }
    Point<float> r (Point<float> d) const { return d * s; }
    float physScale() const
    {
        if (auto* d = Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds())) return jmax (1.0f, (float) d->scale);
        return 1.0f;
    }
    int currentProgram() const { return jlimit (0, wharry::kNumPrograms - 1, (int) std::lround (proc.apvts.getRawParameterValue ("program")->load())); }
    bool param (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    static float knobAngle (int prog)      // 21 crans répartis sur ± 150°
    {
        prog = jlimit (0, wharry::kNumPrograms - 1, prog);
        return degreesToRadians (-150.0f + 300.0f * (float) prog / (float) (wharry::kNumPrograms - 1));
    }

    // LED : bille grise éteinte, cœur blanc chaud + halo ambré allumée
    void drawLed (Graphics& g, Point<float> c0, float level)
    {
        const auto c = r (c0);
        const float rr = kLedR * s;
        g.setGradientFill (ColourGradient (Colour (0xff6b6466), c.x - rr * 0.35f, c.y - rr * 0.4f, Colour (0xff141011), c.x + rr, c.y + rr, true));
        g.fillEllipse (c.x - rr, c.y - rr, 2 * rr, 2 * rr);
        if (level > 0.01f)
        {
            g.setGradientFill (ColourGradient (kWarm.withAlpha (0.55f * level), c.x, c.y, kWarm.withAlpha (0.0f), c.x + rr * 3.6f, c.y, true));
            g.fillEllipse (c.x - rr * 3.6f, c.y - rr * 3.6f, rr * 7.2f, rr * 7.2f);
            g.setGradientFill (ColourGradient (Colour (0xfffff8ea).withAlpha (level), c.x, c.y, kWarm.withAlpha (level), c.x + rr, c.y, true));
            g.fillEllipse (c.x - rr * 0.95f, c.y - rr * 0.95f, rr * 1.9f, rr * 1.9f);
        }
    }

    // écran : mode / état, famille + numéro, programmes voisins et programme choisi, décalage en direct, barres de course
    void drawScreen (Graphics& g)
    {
        const auto sc = r (kScreen);
        g.setColour (kScreenBg); g.fillRoundedRectangle (sc, 4.0f * s);
        const int cur = jlimit (0, wharry::kNumPrograms - 1, drawnProgram);
        const auto& pr = wharry::program (cur);
        const float alpha = drawnActive ? 1.0f : 0.4f;
        auto area = sc.reduced (7.0f * s);
        const float fh = sc.getHeight();

        // ligne du haut : moteur + état
        g.setFont (Font (FontOptions (typefaceBold).withHeight (fh * 0.068f)));
        g.setColour (kCyan.withAlpha (drawnActive ? 0.85f : 0.4f));
        auto top = area.removeFromTop (fh * 0.08f);
        g.drawText (drawnChords ? (param ("studio") ? "CHORDS HQ" : "CHORDS") : "CLASSIC", top, Justification::centredLeft);
        g.drawText (drawnActive ? "ON" : "BYPASS", top, Justification::centredRight);
        area.removeFromTop (fh * 0.02f);
        g.setColour (kCyan.withAlpha (0.2f)); g.fillRect (area.getX(), area.getY(), area.getWidth(), jmax (1.0f, 0.8f * s));
        area.removeFromTop (fh * 0.025f);

        // famille + position sur le sélecteur
        const char* fam = pr.type == wharry::Whammy ? "WHAMMY" : (pr.type == wharry::Detune ? "DETUNE" : "HARMONY");
        g.setFont (Font (FontOptions (typeface).withHeight (fh * 0.068f)));
        g.setColour (kCyan.withAlpha (0.75f * alpha));
        auto famRow = area.removeFromTop (fh * 0.08f);
        g.drawText (fam, famRow, Justification::centredLeft);
        g.drawText (String (cur + 1) + " / " + String (wharry::kNumPrograms), famRow, Justification::centredRight);

        // liste défilante : programme précédent, programme choisi (en grand), programme suivant
        auto row = [&] (int idx, float h, float size, float a, bool bold)
        {
            auto rr = area.removeFromTop (h);
            if (idx < 0 || idx >= wharry::kNumPrograms) return;
            g.setFont (Font (FontOptions (bold ? typefaceBold : typeface).withHeight (size)));
            g.setColour (Colour (0xffe8fbff).withAlpha (a * alpha));
            g.drawFittedText (wharry::program (idx).label, rr.toNearestInt(), Justification::centred, 1, 0.55f);
        };
        row (cur - 1, fh * 0.09f, fh * 0.065f, 0.35f, false);
        row (cur,     fh * 0.16f, fh * 0.125f, 1.0f,  true);
        row (cur + 1, fh * 0.09f, fh * 0.065f, 0.35f, false);

        // décalage courant (ou plage du programme)
        g.setFont (Font (FontOptions (typeface).withHeight (fh * 0.08f)));
        String val;
        if (pr.type == wharry::Detune) val = String (roundToInt (pr.up)) + " cents  ·  " + String (roundToInt (jlimit (0.0f, 1.0f, drawnPedal) * 100.0f)) + " %";
        else { const float sm = drawnSemis; val = (sm >= 0.0f ? "+" : "") + String (sm, 1) + " st"; }
        g.setColour (kCyan.withAlpha (0.95f * alpha));
        g.drawText (val, area.removeFromTop (fh * 0.1f), Justification::centred);

        // ligne d'état (valeurs des réglages en direct, ou consigne de MIDI learn)
        auto stRow = area.removeFromBottom (fh * 0.075f);
        g.setFont (Font (FontOptions (typeface).withHeight (fh * 0.056f)));
        g.setColour (kCyan.withAlpha (proc.getLearnTarget() != MidiTarget::None ? 1.0f : 0.7f * alpha));
        g.drawFittedText (drawnStatus, stRow.toNearestInt(), Justification::centred, 1, 0.5f);
        area.removeFromBottom (fh * 0.02f);

        // barres : position de la pédale
        auto bars = area.reduced (0, fh * 0.015f);
        const float tp = jlimit (0.0f, 1.0f, drawnPedal);
        const int nb = 14;
        const float bw = bars.getWidth() / (float) nb;
        for (int i = 0; i < nb; ++i)
        {
            const float t = ((float) i + 0.5f) / (float) nb;
            const float hgt = bars.getHeight() * (0.25f + 0.75f * std::sin (kPiF * t));
            const bool lit = t <= tp + 0.001f;
            g.setColour (kCyan.withAlpha ((lit ? 0.9f : 0.18f) * alpha));
            g.fillRect (bars.getX() + (float) i * bw + bw * 0.15f, bars.getBottom() - hgt, bw * 0.7f, hgt);
        }
    }
    static constexpr float kPiF = 3.14159265f;

    String statusText() const
    {
        const auto learn = proc.getLearnTarget();
        if (learn == MidiTarget::Pedal)   return "MIDI LEARN : move your expression pedal";
        if (learn == MidiTarget::LeftFsw) return "MIDI LEARN : CLASSIC / CHORDS switch";
        if (learn == MidiTarget::RightFsw)return "MIDI LEARN : ON / OFF footswitch";
        auto pc = [this] (const char* id) { return String (roundToInt (proc.apvts.getParameter (id)->getValue() * 100.0f)) + " %"; };
        String st = "PEDAL " + String (roundToInt (proc.getPedalShown() * 100.0f)) + " %"
                  + "  ·  GLIDE " + String (roundToInt (proc.apvts.getRawParameterValue ("smooth")->load())) + " ms"
                  + "  ·  TONE " + pc ("tone") + "  ·  MIX " + pc ("mix");
        if (param ("chords") && param ("studio") && proc.getLatency() > 0)
            st << "  ·  " << String (roundToInt (1000.0 * proc.getLatency() / jmax (1.0, proc.getSampleRate()))) << " ms";
        return st;
    }

    void timerCallback() override
    {
        if (auto* ed = findParentComponentOfClass<WharryEditor>()) ed->useSoftwareRenderer();

        const int prog = currentProgram();
        const bool chords = param ("chords"), active = param ("active");
        const float ped = proc.getPedalShown(), sm = proc.getSemitones();
        const String st = statusText();
        bool screen = false;

        if (std::abs (ped - drawnPedal) > 0.002f)
        {
            drawnPedal = ped; screen = true;
            repaint (r (treadle.designBounds().toFloat()).toNearestInt().expanded (2));
        }
        if (std::abs (sm - drawnSemis) > 0.04f) { drawnSemis = sm; screen = true; }
        if (prog != drawnProgram)
        {
            repaint (r (Rectangle<float> (kKnob.x - kKnobR, kKnob.y - kKnobR, 2 * kKnobR, 2 * kKnobR)).toNearestInt().expanded (2));
            drawnProgram = prog; screen = true;
        }
        if (chords != drawnChords || active != drawnActive)
        {
            drawnChords = chords; drawnActive = active; screen = true;
            repaint (r (kToggle).toNearestInt().expanded (3));
            repaint (r (kFs).toNearestInt().expanded (4));
            repaint (ledArea (kFsLed));
        }
        for (int i = 0; i < 3; ++i)
        {
            const float v = proc.apvts.getParameter (kSmallId[i])->getValue();
            if (std::abs (v - drawnSmall[i]) > 0.002f)
            {
                drawnSmall[i] = v;
                repaint (r (Rectangle<float> (kSmall[i].x - kSmallR, kSmall[i].y - kSmallR, 2 * kSmallR, 2 * kSmallR)).toNearestInt().expanded (2));
            }
        }
        if (st != drawnStatus) { drawnStatus = st; screen = true; }
        if (screen) repaint (r (kScreen).toNearestInt().expanded (2));
    }
    Rectangle<int> ledArea (Point<float> c) const { const float h = kLedR * 3.8f; return r (Rectangle<float> (c.x - h, c.y - h, 2 * h, 2 * h)).toNearestInt().expanded (1); }

    void showPresetMenu()
    {
        PopupMenu m, progs, fac;
        const int cur = currentProgram();
        for (int i = 0; i < wharry::kNumPrograms; ++i) progs.addItem (1 + i, wharry::program (i).name, true, i == cur);
        for (int i = 0; i < WharryProcessor::kNumFactory; ++i) fac.addItem (101 + i, WharryProcessor::factory (i).name);
        m.addSectionHeader ("Presets");
        m.addSubMenu ("Whammy programs (original)", progs);
        m.addSubMenu ("Studio Wiga factory presets", fac);
        Component::SafePointer<PedalView> sp (this);
        m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (r (kScreen).toNearestInt())), [sp] (int res)
        {
            if (sp == nullptr || res == 0) return;
            if (res >= 101) sp->proc.loadFactory (res - 101);
            else sp->proc.selectWhammyProgram (res - 1);
        });
    }

    void showMenu (Point<float> p)
    {
        auto* ed = findParentComponentOfClass<WharryEditor>();
        PopupMenu m;
        const bool onFs = kFs.getCentre().getDistanceFrom (p) < kFs.getWidth() * 0.6f;
        const bool onToggle = kToggleHit.contains (p);
        if (onFs || onToggle)
        {
            const auto t = onToggle ? MidiTarget::LeftFsw : MidiTarget::RightFsw;
            const auto d = proc.getMidiDescription (t);
            m.addSectionHeader (onToggle ? "CLASSIC / CHORDS switch" : "ON / OFF footswitch");
            m.addItem (onToggle ? 11 : 21, "MIDI learn" + (d.isNotEmpty() ? " (now: " + d + ")" : String()));
            m.addItem (onToggle ? 12 : 22, "Clear MIDI assignment", d.isNotEmpty());
            m.addSeparator();
        }
        else
        {
            m.addSectionHeader ("Expression pedal (" + proc.getMidiDescription (MidiTarget::Pedal) + ")");
            m.addItem (31, "MIDI learn pedal controller");
            m.addItem (32, "Reset to CC 11 (Whammy standard)");
            m.addItem (33, "Invert pedal direction", true, param ("invert"));
            m.addItem (34, "Reset heel / toe calibration");
            m.addSeparator();
            m.addSectionHeader ("Chords engine");
            m.addItem (51, "Live (no latency)", true, ! param ("studio"));
            m.addItem (52, "Studio HQ (latency, for re-amping / mixing)", true, param ("studio"));
            m.addItem (53, "Respond to MIDI Program Change", true, param ("pc"));
            m.addSeparator();
        }
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
                default: handleSizeChoice (res, sp->findParentComponentOfClass<WharryEditor>()); break;
            }
        });
    }

    WharryProcessor& proc;
    ScaledCache bg, fsOn, fsOff, tgL, tgR;
    TreadleWarp treadle;
    Typeface::Ptr typeface, typefaceBold;
    float s = kDefaultScale * kRefH / (float) kH;

    Drag drag = Drag::None;
    float dragStartY = 0, dragStartVal = 0;
    int dragStartProg = 0;
    bool pressed = false;

    float drawnPedal = -1.0f, drawnSemis = 0.0f, drawnSmall[3] = { -1.0f, -1.0f, -1.0f };
    int drawnProgram = -1;
    bool drawnChords = false, drawnActive = true;
    String drawnStatus;
};

//==============================================================================
WharryEditor::WharryEditor (WharryProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    view = std::make_unique<PedalView> (proc);
    addAndMakeVisible (*view);
    setResizable (true, true);
    const int minH = (int) std::lround (kRefH * kDefaultScale);          // jamais plus petit que la taille par défaut
    const int maxH = 1120;
    setResizeLimits ((int) std::lround ((double) minH * kW / kH), minH, (int) std::lround ((double) maxH * kW / kH), maxH);
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
    const float sc = (float) getHeight() / kRefH;
    for (int i = 1; i < 3; ++i) if (std::abs (sc - kScales[i]) < 0.02f) return i;
    return -1;
}

void WharryEditor::setScaleIndex (int idx)
{
    idx = jlimit (-1, 2, idx);
    if (idx == 0) idx = -1;
    proc.uiScaleIdx = idx;
    const float sc = idx < 0 ? kDefaultScale : kScales[idx];
    const int h = (int) std::lround (kRefH * sc);
    setSize ((int) std::lround ((double) h * kW / kH), h);
}
