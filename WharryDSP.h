// Wharry (Studio Wiga) - moteur audio, sans dépendance JUCE (testable hors DAW)
// Copie de fonctionnement de la DigiTech Whammy 5 :
//   - 10 programmes WHAMMY  : signal 100 % traité, la pédale glisse de 0 (orteil levé) à l'intervalle (orteil enfoncé)
//   -  2 programmes DETUNE  : sec + voix légèrement désaccordée, la pédale règle le niveau de la voix
//   -  9 programmes HARMONY : sec + 2e voix qui glisse entre deux intervalles
//   - CLASSIC : moteur monophonique (délai à double tête synchronisé sur la période détectée), réactif, caractère « Whammy »
//   - CHORDS  : vocodeur de phase polyphonique à verrouillage de phase (accords propres, latence annoncée au DAW)
#pragma once
#include <cmath>
#include <vector>
#include <complex>
#include <algorithm>
#include <cstring>

namespace wharry
{
static constexpr float kPi = 3.14159265358979323846f;
static constexpr double kTwoPi = 6.28318530717958647692;

inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

enum ProgType { Whammy = 0, Detune = 1, Harmony = 2 };

struct Program
{
    const char* name;      // nom complet (menus, ligne d'état)
    const char* label;     // étiquette courte (sérigraphie)
    int type;
    float up, down;        // demi-tons orteil levé / orteil enfoncé (Detune : cents de la voix)
};

static constexpr int kNumPrograms = 21;

// ordre du sélecteur et des Program Change MIDI (1 à 21)
inline const Program& program (int i)
{
    static const Program p[kNumPrograms] = {
        { "Whammy 2 Oct Up",        "2 OCT UP",      Whammy,   0.0f,  24.0f },
        { "Whammy Oct Up",          "OCT UP",        Whammy,   0.0f,  12.0f },
        { "Whammy 5th Up",          "5TH UP",        Whammy,   0.0f,   7.0f },
        { "Whammy 4th Up",          "4TH UP",        Whammy,   0.0f,   5.0f },
        { "Whammy 2nd Down",        "2ND DN",        Whammy,   0.0f,  -2.0f },
        { "Whammy 4th Down",        "4TH DN",        Whammy,   0.0f,  -5.0f },
        { "Whammy 5th Down",        "5TH DN",        Whammy,   0.0f,  -7.0f },
        { "Whammy Oct Down",        "OCT DN",        Whammy,   0.0f, -12.0f },
        { "Whammy 2 Oct Down",      "2 OCT DN",      Whammy,   0.0f, -24.0f },
        { "Whammy Dive Bomb",       "DIVE BOMB",     Whammy,   0.0f, -36.0f },
        { "Detune Shallow",         "SHALLOW",       Detune,  10.0f,  10.0f },
        { "Detune Deep",            "DEEP",          Detune,  25.0f,  25.0f },
        { "Harmony Oct Down/Oct Up","OCT DN/OCT UP", Harmony,-12.0f,  12.0f },
        { "Harmony 5th Dn/4th Dn",  "5TH DN/4TH DN", Harmony, -7.0f,  -5.0f },
        { "Harmony 4th Dn/3rd Dn",  "4TH DN/3RD DN", Harmony, -5.0f,  -4.0f },
        { "Harmony 5th Up/7th Up",  "5TH/7TH",       Harmony,  7.0f,  10.0f },
        { "Harmony 5th Up/6th Up",  "5TH/6TH",       Harmony,  7.0f,   9.0f },
        { "Harmony 4th Up/5th Up",  "4TH/5TH",       Harmony,  5.0f,   7.0f },
        { "Harmony 3rd Up/4th Up",  "3RD/4TH",       Harmony,  4.0f,   5.0f },
        { "Harmony b3rd Up/3rd Up", "b3RD/3RD",      Harmony,  3.0f,   4.0f },
        { "Harmony 2nd Up/3rd Up",  "2ND/3RD",       Harmony,  2.0f,   4.0f },
    };
    return p[i < 0 ? 0 : (i >= kNumPrograms ? kNumPrograms - 1 : i)];
}

//==============================================================================
// Biquad passe-bas (RBJ), utilisé pour la décimation du détecteur de hauteur
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void lowpass (double fs, double fc, double q)
    {
        const double w = kTwoPi * fc / fs, c = std::cos (w), s = std::sin (w), al = s / (2.0 * q), a0 = 1.0 + al;
        b0 = (float) ((1.0 - c) * 0.5 / a0); b1 = (float) ((1.0 - c) / a0); b2 = b0;
        a1 = (float) (-2.0 * c / a0); a2 = (float) ((1.0 - al) / a0);
    }
    void reset() { z1 = z2 = 0; }
    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

//==============================================================================
// Détecteur de période (YIN sur signal décimé ~12 kHz), de ~35 Hz (7 cordes accordée très bas) à ~1500 Hz.
class PitchTracker
{
public:
    void prepare (double fs)
    {
        sr = fs;
        dec = std::max (1, (int) std::lround (fs / 12000.0));
        dsr = fs / dec;
        lp1.lowpass (fs, dsr * 0.40, 0.54); lp2.lowpass (fs, dsr * 0.40, 1.31);
        tauMax = std::min (kW - 2, (int) std::ceil (dsr / 35.0));
        tauMin = std::max (2, (int) std::floor (dsr / 1500.0));
        std::fill (std::begin (ring), std::end (ring), 0.0f);
        holdMax = (int) (0.12 * fs);
        reset();
    }
    void reset()
    {
        lp1.reset(); lp2.reset(); phase = 0; wpos = 0; sinceHop = 0; period = 0; hold = 0; env = 0;
        std::fill (std::begin (ring), std::end (ring), 0.0f);
    }
    // à appeler à chaque échantillon ; renvoie la période courante en échantillons (0 = pas de note claire)
    float push (float x)
    {
        env = std::max (std::abs (x), env * 0.9995f);
        const float y = lp2.process (lp1.process (x));
        if (++phase >= dec)
        {
            phase = 0;
            ring[wpos] = y; wpos = (wpos + 1) & (kRing - 1);
            if (++sinceHop >= kHop) { sinceHop = 0; analyse(); }
        }
        if (hold > 0 && --hold == 0) period = 0;
        return period;
    }
    float getPeriod() const { return period; }

private:
    static constexpr int kRing = 1024, kW = 384, kHop = 48;
    void analyse()
    {
        if (env < 1.0e-4f) return;
        // fenêtre analysée : les (kW + tauMax) derniers échantillons décimés
        const int len = kW + tauMax;
        for (int i = 0; i < len; ++i) buf[i] = ring[(wpos - len + i + kRing) & (kRing - 1)];
        float running = 0.0f;
        d[0] = 1.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float s = 0.0f;
            for (int j = 0; j < kW; ++j) { const float e = buf[j] - buf[j + tau]; s += e * e; }
            running += s;
            d[tau] = running > 0.0f ? s * (float) tau / running : 1.0f;
        }
        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (d[tau] < 0.15f)
            {
                while (tau + 1 < tauMax && d[tau + 1] < d[tau]) ++tau;
                best = tau; break;
            }
        if (best < 0) return;   // garde l'ancienne période jusqu'à expiration
        float t = (float) best;
        if (best > 1 && best < tauMax)
        {
            const float a = d[best - 1], b = d[best], c = d[best + 1], den = a - 2.0f * b + c;
            if (std::abs (den) > 1.0e-9f) t += 0.5f * (a - c) / den;
        }
        period = t * (float) dec;
        hold = holdMax;
    }

    double sr = 48000, dsr = 12000;
    int dec = 4, phase = 0, wpos = 0, sinceHop = 0, tauMax = 300, tauMin = 8, hold = 0, holdMax = 5000;
    float period = 0, env = 0;
    Biquad lp1, lp2;
    float ring[kRing] {};
    float buf[kRing] {};
    float d[kRing] {};
};

//==============================================================================
// Moteur CLASSIC : délai à deux têtes en fondu (grains de Hann à 50 %),
// longueur de grain multiple de la période détectée et têtes recalées en phase -> glissements propres sur notes seules.
class GrainShifter
{
public:
    // poly = false : CLASSIC (grain calé sur la période détectée)
    // poly = true  : CHORDS live (raccord choisi par corrélation : accords, faible latence)
    void prepare (double fs, bool polyMode)
    {
        sr = fs; poly = polyMode;
        int n = 1; while (n < (int) (fs * 0.6)) n <<= 1;
        buf.assign ((size_t) n, 0.0f); mask = n - 1;
        dMin = std::max (8.0, 0.0015 * fs);
        dMax = (double) n - 8.0;
        tracker.prepare (fs);
        reset();
    }
    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        w = 0; tracker.reset();
        taps[0] = { dMin, (int) (0.03 * sr), 0, true };
        taps[1] = { dMin, (int) (0.03 * sr), (int) (0.015 * sr), true };
        countdown = taps[0].len / 2;
    }
    float process (float x, float ratio)
    {
        buf[(size_t) w] = x;
        const float T = poly ? 0.0f : tracker.push (x);

        if (--countdown <= 0)
        {
            // démarre un nouveau grain sur la tête la plus avancée (celle qui finit)
            auto progress = [] (const Tap& t) { return t.active ? (double) t.pos / (double) t.len : 2.0; };
            Tap& old = progress (taps[0]) >= progress (taps[1]) ? taps[0] : taps[1];
            Tap& other = (&old == &taps[0]) ? taps[1] : taps[0];
            startGrain (old, other, ratio, T);
            countdown = std::max (16, old.len / 2);
        }

        float acc = 0.0f, ws = 0.0f;
        for (auto& t : taps)
        {
            if (! t.active) continue;
            const float wv = 0.5f - 0.5f * std::cos (2.0f * kPi * (float) t.pos / (float) t.len);
            acc += wv * read (t.d);
            ws += wv;
            t.d = std::min (dMax, std::max (dMin, t.d + (1.0 - (double) ratio)));
            if (++t.pos >= t.len) t.active = false;
        }
        w = (w + 1) & mask;
        return ws > 0.2f ? acc / ws : acc / 0.2f;
    }
    float getPeriod() const { return tracker.getPeriod(); }

private:
    struct Tap { double d; int len; int pos; bool active; };

    void startGrain (Tap& t, const Tap& other, float ratio, float T)
    {
        const double r = ratio;
        const double span = std::max (r, 1.0 / r);
        if (poly) { startPolyGrain (t, other, r, span); return; }
        // grain visé : ~38 ms au repos, plus court pour les grands intervalles (moins de traînée)
        double g = sr * 0.038 / std::sqrt (std::min (span, 4.0));
        if (T > 0.0f)
        {
            const double k = std::max (1.0, std::round (g / T));
            g = k * T;
            if (g < 2.0 * T && 2.0 * T < sr * 0.09) g = 2.0 * T;
        }
        g = std::max (g, sr * 0.006);
        t.len = std::max (32, (int) std::lround (g));
        t.pos = 0; t.active = true;
        double d0 = r > 1.0 ? dMin + (r - 1.0) * (double) t.len : dMin;
        // recalage en phase sur la tête encore active : même point du cycle -> pas de « vibrato » de fondu
        if (T > 0.0f && other.active)
        {
            const double k = std::round ((d0 - other.d) / (double) T);
            double cand = other.d + k * (double) T;
            while (cand < dMin) cand += T;
            if (r > 1.0 && cand - (r - 1.0) * (double) t.len < dMin)
                cand += std::ceil ((dMin - (cand - (r - 1.0) * (double) t.len)) / (double) T) * (double) T;
            const double endD = r < 1.0 ? cand + (1.0 - r) * (double) t.len : cand;
            if (endD <= dMax) d0 = cand;
        }
        t.d = std::min (dMax, std::max (dMin, d0));
    }

    // CHORDS : cherche, autour du retard nominal, le décalage où le signal ressemble le plus
    // à ce que joue la tête active (corrélation normalisée, recherche grossière puis fine)
    void startPolyGrain (Tap& t, const Tap& other, double r, double span)
    {
        double g = sr * 0.070 / std::sqrt (std::min (span, 4.0));
        g = std::max (g, sr * 0.020);
        t.len = std::max (32, (int) std::lround (g));
        t.pos = 0; t.active = true;
        const double grow = r > 1.0 ? (r - 1.0) * (double) t.len : 0.0;
        const double shrink = r < 1.0 ? (1.0 - r) * (double) t.len : 0.0;
        const double lo = dMin + grow, hi = dMax - shrink - 8.0;
        double nominal = std::min (hi, std::max (lo, dMin + grow));
        if (! other.active || hi <= lo) { t.d = nominal; return; }

        const int S = (int) (sr * 0.022);       // ±22 ms : couvre la période commune d'un accord grave (7 cordes en G)
        const int L = (int) (sr * 0.030);       // fenêtre de comparaison 30 ms
        const int coarse = std::max (1, (int) std::lround (sr / 12000.0));
        const double base = other.d;
        auto score = [&] (double dc, int stride)
        {
            float xy = 0, xx = 0, yy = 0;
            for (int i = 0; i < L; i += stride)
            {
                const float a = at (base + (double) i), b = at (dc + (double) i);
                xy += a * b; xx += a * a; yy += b * b;
            }
            return xx * yy > 1.0e-12f ? xy / std::sqrt (xx * yy) : 0.0f;
        };
        double from = std::max (lo, nominal - S), to = std::min (hi, nominal + S);
        if (to - from < 2.0 * S) { if (from == lo) to = std::min (hi, lo + 2.0 * S); else from = std::max (lo, hi - 2.0 * S); }
        double best = nominal; float bs = -2.0f;
        for (double dc = from; dc <= to; dc += coarse)
        {
            const float sc = score (dc, coarse) - 0.02f * (float) (std::abs (dc - nominal) / (double) S);   // préfère rester près du nominal
            if (sc > bs) { bs = sc; best = dc; }
        }
        const double c0 = best;
        for (int k = -coarse; k <= coarse; ++k)
        {
            const double dc = c0 + k;
            if (dc < lo || dc > hi) continue;
            const float sc = score (dc, 1) - 0.02f * (float) (std::abs (dc - nominal) / (double) S);
            if (k == -coarse || sc > bs) { bs = sc; best = dc; }
        }
        t.d = best;
    }

    float at (double d) const { int i = w - (int) d; return buf[(size_t) (i & mask)]; }

    float read (double d) const
    {
        double rp = (double) w - d;
        while (rp < 0) rp += (double) (mask + 1);
        const int i1 = (int) rp;
        const float f = (float) (rp - (double) i1);
        const float y0 = buf[(size_t) ((i1 - 1) & mask)], y1 = buf[(size_t) (i1 & mask)],
                    y2 = buf[(size_t) ((i1 + 1) & mask)], y3 = buf[(size_t) ((i1 + 2) & mask)];
        // Hermite 4 points
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    double sr = 48000, dMin = 64, dMax = 30000;
    bool poly = false;
    std::vector<float> buf;
    int mask = 0, w = 0, countdown = 0;
    Tap taps[2] {};
    PitchTracker tracker;
};

//==============================================================================
// FFT complexe radix-2 (taille fixe, tables précalculées dans prepare)
class FFT
{
public:
    void prepare (int n)
    {
        N = n; logN = 0; while ((1 << logN) < N) ++logN;
        tw.resize ((size_t) N / 2);
        for (int k = 0; k < N / 2; ++k) tw[(size_t) k] = std::polar (1.0f, (float) (-kTwoPi * k / N));
        rev.resize ((size_t) N);
        for (int i = 0; i < N; ++i)
        {
            int r = 0; for (int b = 0; b < logN; ++b) if (i & (1 << b)) r |= 1 << (logN - 1 - b);
            rev[(size_t) i] = r;
        }
    }
    void run (std::complex<float>* x, bool inverse) const
    {
        for (int i = 0; i < N; ++i) if (i < rev[(size_t) i]) std::swap (x[i], x[rev[(size_t) i]]);
        for (int len = 2; len <= N; len <<= 1)
        {
            const int half = len >> 1, step = N / len;
            for (int i = 0; i < N; i += len)
                for (int j = 0; j < half; ++j)
                {
                    std::complex<float> wv = tw[(size_t) (j * step)];
                    if (inverse) wv = std::conj (wv);
                    const auto u = x[i + j], v = x[i + j + half] * wv;
                    x[i + j] = u + v; x[i + j + half] = u - v;
                }
        }
    }
private:
    int N = 0, logN = 0;
    std::vector<std::complex<float>> tw;
    std::vector<int> rev;
};

//==============================================================================
// Moteur CHORDS : vocodeur de phase, décalage des pics spectraux avec verrouillage de phase (Laroche & Dolson)
class ChordShifter
{
public:
    void prepare (int fftSize, int overlap)
    {
        N = fftSize; hop = N / overlap; nb = N / 2 + 1; olaScale = 1.0f / ((float) N * 0.375f * (float) overlap);
        fft.prepare (N);
        win.resize ((size_t) N);
        for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos ((float) (kTwoPi * i / N));
        inF.assign ((size_t) N, 0.0f); outF.assign ((size_t) N, 0.0f);
        spec.assign ((size_t) N, {}); outSpec.assign ((size_t) N, {});
        mag.assign ((size_t) nb, 0.0f); ph.assign ((size_t) nb, 0.0f); prevPh.assign ((size_t) nb, 0.0f);
        prevOut.assign ((size_t) nb, 0.0f); omega.assign ((size_t) nb, 0.0f);
        peaks.assign ((size_t) nb, 0);
        reset();
    }
    void reset()
    {
        std::fill (inF.begin(), inF.end(), 0.0f); std::fill (outF.begin(), outF.end(), 0.0f);
        std::fill (prevPh.begin(), prevPh.end(), 0.0f); std::fill (prevOut.begin(), prevOut.end(), 0.0f);
        idx = 0; cnt = 0;
    }
    int latency() const { return N; }

    float process (float x, float ratio)
    {
        inF[(size_t) idx] = x;
        const float y = outF[(size_t) idx];
        outF[(size_t) idx] = 0.0f;
        idx = (idx + 1) % N;
        if (++cnt >= hop) { cnt = 0; frame (ratio); }
        return y;
    }

private:
    static float wrap (float a) { a = std::fmod (a + kPi, 2.0f * kPi); if (a < 0) a += 2.0f * kPi; return a - kPi; }

    void frame (float r)
    {
        for (int n = 0; n < N; ++n) spec[(size_t) n] = { inF[(size_t) ((idx + n) % N)] * win[(size_t) n], 0.0f };
        fft.run (spec.data(), false);

        float mx = 0.0f;
        const float expct = (float) (kTwoPi * hop / N);
        for (int k = 0; k < nb; ++k)
        {
            mag[(size_t) k] = std::abs (spec[(size_t) k]);
            ph[(size_t) k] = std::arg (spec[(size_t) k]);
            const float dp = wrap (ph[(size_t) k] - prevPh[(size_t) k] - expct * (float) k);
            omega[(size_t) k] = (expct * (float) k + dp) / (float) hop;   // rad / échantillon
            prevPh[(size_t) k] = ph[(size_t) k];
            mx = std::max (mx, mag[(size_t) k]);
        }

        std::fill (outSpec.begin(), outSpec.end(), std::complex<float> {});
        int np = 0;
        const float thr = std::max (mx * 1.0e-4f, 1.0e-7f);
        for (int k = 1; k < nb - 2; ++k)
        {
            const float m = mag[(size_t) k];
            if (m > thr && m >= mag[(size_t) k - 1] && m > mag[(size_t) k + 1]
                && (k < 2 || m >= mag[(size_t) k - 2]) && m > mag[(size_t) k + 2])
                peaks[(size_t) np++] = k;
        }

        if (np > 0 && mx > 1.0e-9f)
        {
            for (int i = 0; i < np; ++i)
            {
                const int p = peaks[(size_t) i];
                const int lo = i == 0 ? 0 : (peaks[(size_t) i - 1] + p) / 2 + 1;
                const int hi = i == np - 1 ? nb - 1 : (p + peaks[(size_t) i + 1]) / 2;
                const float kTrue = omega[(size_t) p] * (float) N / (float) kTwoPi;
                const int s = (int) std::lround (kTrue * r - kTrue);
                const int nbp = p + s;
                if (nbp < 1 || nbp >= nb - 1) continue;
                const float theta = prevOut[(size_t) nbp] + r * omega[(size_t) p] * (float) hop;
                const float rot = theta - ph[(size_t) p];
                const std::complex<float> rv = std::polar (1.0f, rot);
                for (int b = lo; b <= hi; ++b)
                {
                    const int ob = b + s;
                    if (ob < 1 || ob >= nb - 1) continue;
                    outSpec[(size_t) ob] += spec[(size_t) b] * rv;
                }
            }
        }
        for (int k = 0; k < nb; ++k)
            prevOut[(size_t) k] = std::abs (outSpec[(size_t) k]) > 0.0f ? std::arg (outSpec[(size_t) k]) : prevOut[(size_t) k] + (float) (kTwoPi * hop * k / N);
        for (int k = 1; k < N / 2; ++k) outSpec[(size_t) (N - k)] = std::conj (outSpec[(size_t) k]);
        outSpec[0] = {}; outSpec[(size_t) (N / 2)] = {};

        fft.run (outSpec.data(), true);
        const float sc = olaScale;
        for (int n = 0; n < N; ++n) outF[(size_t) ((idx + n) % N)] += outSpec[(size_t) n].real() * win[(size_t) n] * sc;
    }

    int N = 2048, hop = 512, nb = 1025, idx = 0, cnt = 0;
    float olaScale = 1.0f;
    FFT fft;
    std::vector<float> win, inF, outF, mag, ph, prevPh, prevOut, omega;
    std::vector<int> peaks;
    std::vector<std::complex<float>> spec, outSpec;
};

//==============================================================================
// Ligne de retard simple (alignement du signal sec sur la latence du mode CHORDS)
class DelayLine
{
public:
    void prepare (int maxDelay) { buf.assign ((size_t) maxDelay + 1, 0.0f); w = 0; }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    float process (float x, int d)
    {
        const int n = (int) buf.size();
        buf[(size_t) w] = x;
        int r = w - d; if (r < 0) r += n;
        w = (w + 1) % n;
        return buf[(size_t) r];
    }
private:
    std::vector<float> buf; int w = 0;
};

//==============================================================================
struct Settings
{
    int   programIdx = 1;        // Whammy Oct Up
    bool  chords = false;        // false = CLASSIC (notes seules), true = CHORDS (accords)
    bool  studio = false;        // CHORDS : moteur « Studio » (vocodeur de phase, très propre, forte latence -> ré-amping / mixage)
    bool  active = true;         // footswitch ON / OFF
    float pedal = 0.0f;          // 0 = orteil levé (position haute), 1 = orteil enfoncé (position basse)
    float smoothMs = 30.0f;      // lissage de la pédale (contrôleurs MIDI en marches d'escalier)
    float dryLevel = 1.0f;       // niveau sec (Harmony / Detune)
    float wetLevel = 1.0f;       // niveau de la voix traitée
};

class Engine
{
public:
    void prepare (double fs, int /*maxBlock*/)
    {
        sr = fs;
        classic.prepare (fs, false);
        live.prepare (fs, true);
        int n = 8192; while ((double) n < fs * 0.17) n <<= 1;     // ~170 ms : sépare les notes d'un accord sur une 7 cordes en G
        studioPV.prepare (n, 8);
        dryDelay.prepare (n + 8);
        wetAlign.prepare (n + 8);
        fadeStep = (float) (1.0 / (0.012 * fs));
        bypassStep = (float) (1.0 / (0.010 * fs));
        pedalSm = cur.pedal;
        applied = cur;
        mixGain = 1.0f; bypassGain = cur.active ? 1.0f : 0.0f;
        reset();
    }
    void reset()
    {
        classic.reset(); live.reset(); studioPV.reset(); dryDelay.reset(); wetAlign.reset();
    }

    void setSettings (const Settings& s) { cur = s; }

    // latence annoncée au DAW : 0 en CLASSIC et CHORDS live, longueur de FFT en CHORDS Studio
    int latencyFor (const Settings& s) const { return (s.chords && s.studio) ? studioPV.latency() : 0; }
    int currentLatency() const { return latencyFor (applied); }

    float currentSemitones() const { return lastSemis; }
    float smoothedPedal() const { return pedalSm; }
    float detectedHz() const { const float T = classic.getPeriod(); return T > 0 ? (float) sr / T : 0.0f; }

    void process (const float* in, float* out, int n)
    {
        const float a = std::exp (-1.0f / (std::max (0.5f, cur.smoothMs) * 0.001f * (float) sr));
        for (int i = 0; i < n; ++i)
        {
            // changement de programme / de moteur : fondu court vers 0, bascule, puis retour
            const bool engineChange = cur.chords != applied.chords || cur.studio != applied.studio;
            if (engineChange || cur.programIdx != applied.programIdx)
            {
                mixGain -= fadeStep;
                if (mixGain <= 0.0f)
                {
                    mixGain = 0.0f;
                    if (engineChange) reset();
                    applied.programIdx = cur.programIdx; applied.chords = cur.chords; applied.studio = cur.studio;
                }
            }
            else if (mixGain < 1.0f) mixGain = std::min (1.0f, mixGain + fadeStep);

            const float bTarget = cur.active ? 1.0f : 0.0f;
            if (bypassGain < bTarget) bypassGain = std::min (bTarget, bypassGain + bypassStep);
            else if (bypassGain > bTarget) bypassGain = std::max (bTarget, bypassGain - bypassStep);

            pedalSm = cur.pedal + a * (pedalSm - cur.pedal);

            const Program& p = program (applied.programIdx);
            float semis, wetLvl = cur.wetLevel, dryLvl = 0.0f;
            if (p.type == Detune)
            {
                semis = p.up * 0.01f;            // cents -> demi-tons
                wetLvl *= pedalSm;               // la pédale dose la voix désaccordée
                dryLvl = cur.dryLevel;
            }
            else
            {
                semis = p.up + (p.down - p.up) * pedalSm;
                if (p.type == Harmony) { dryLvl = cur.dryLevel; wetLvl *= 0.9f; }
            }
            lastSemis = semis;
            const float ratio = std::exp2 (semis / 12.0f);

            const float x = in[i];
            const int lat = latencyFor (applied);
            float wet;
            if (! applied.chords)                 wet = classic.process (x, ratio);
            else if (applied.studio && p.type != Detune) wet = studioPV.process (x, ratio);
            else                                  wet = wetAlign.process (live.process (x, ratio), lat);
            const float dry = dryDelay.process (x, lat);

            // en WHAMMY le sec disparaît ; pendant un fondu de bascule il remplace la voix traitée (pas de trou)
            float fx;
            if (p.type == Whammy) fx = wet * cur.wetLevel * mixGain + dry * (1.0f - mixGain);
            else                  fx = dry * dryLvl + wet * wetLvl * mixGain;

            out[i] = dry + (fx - dry) * bypassGain;
        }
    }

private:
    double sr = 48000;
    GrainShifter classic, live;
    ChordShifter studioPV;
    DelayLine dryDelay, wetAlign;
    Settings cur, applied;
    float pedalSm = 0, mixGain = 1, bypassGain = 1, fadeStep = 0.001f, bypassStep = 0.001f, lastSemis = 0;
};

} // namespace wharry
