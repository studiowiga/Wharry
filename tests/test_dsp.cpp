// Banc de test hors DAW du moteur Wharry : justesse des intervalles (Classic / Chords), notes très graves, accords, balayage pédale
#include "../WharryDSP.h"
#include <cstdio>
#include <string>

using namespace wharry;

static std::vector<float> tone (double fs, int n, std::vector<double> freqs, bool saw)
{
    std::vector<float> x ((size_t) n, 0.0f);
    for (double f : freqs)
        for (int i = 0; i < n; ++i)
        {
            const double ph = std::fmod (f * i / fs, 1.0);
            x[(size_t) i] += (float) (saw ? 0.25 * (2.0 * ph - 1.0) : 0.3 * std::sin (kTwoPi * ph));
        }
    return x;
}

// fréquences des pics spectraux (FFT de 65536 points, Hann, interpolation parabolique)
static std::vector<double> peaksHz (const std::vector<float>& y, int start, double fs, int count, double minHz = 25.0)
{
    const int N = 65536;
    FFT fft; fft.prepare (N);
    std::vector<std::complex<float>> s ((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos ((float) (kTwoPi * i / N));
        const int j = start + i;
        s[(size_t) i] = { j < (int) y.size() ? y[(size_t) j] * w : 0.0f, 0.0f };
    }
    fft.run (s.data(), false);
    std::vector<float> m ((size_t) N / 2);
    for (int k = 0; k < N / 2; ++k) m[(size_t) k] = std::abs (s[(size_t) k]);
    std::vector<std::pair<float, double>> pk;
    const int kmin = (int) (minHz * N / fs);
    for (int k = std::max (2, kmin); k < N / 2 - 2; ++k)
        if (m[(size_t) k] > m[(size_t) k - 1] && m[(size_t) k] >= m[(size_t) k + 1])
        {
            const float a = std::log (m[(size_t) k - 1] + 1e-12f), b = std::log (m[(size_t) k] + 1e-12f), c = std::log (m[(size_t) k + 1] + 1e-12f);
            const double off = 0.5 * (a - c) / (a - 2 * b + c);
            pk.push_back ({ m[(size_t) k], (k + off) * fs / N });
        }
    std::sort (pk.begin(), pk.end(), [] (auto& p, auto& q) { return p.first > q.first; });
    std::vector<double> out;
    for (int i = 0; i < count && i < (int) pk.size(); ++i) out.push_back (pk[(size_t) i].second);
    return out;
}

static double cents (double a, double b) { return 1200.0 * std::log2 (a / b); }

static std::vector<float> run (Engine& e, Settings s, const std::vector<float>& x)
{
    e.setSettings (s);
    std::vector<float> y (x.size());
    e.process (x.data(), y.data(), (int) x.size());
    return y;
}

int main()
{
    int fails = 0;
    const double fs = 48000.0;
    const int n = (int) (fs * 2.2);

    std::printf ("== Justesse WHAMMY orteil enfoncé (sinus) ==\n");
    for (int chords = 0; chords < 3; ++chords)
        for (double f0 : { 49.0, 82.41, 196.0, 440.0 })
            for (int pi = 0; pi < 10; ++pi)
            {
                const Program& p = program (pi);
                const double expect = f0 * std::exp2 (p.down / 12.0);
                if (expect < 30.0 || expect > 3500.0) continue;
                Engine e; e.prepare (fs, 512);
                Settings s; s.programIdx = pi; s.chords = chords > 0; s.studio = chords == 2; s.pedal = 1.0f; s.smoothMs = 5.0f;
                e.setSettings (s); e.prepare (fs, 512);
                auto y = run (e, s, tone (fs, n, { f0 }, false));
                auto pk = peaksHz (y, (int) (fs * 0.8), fs, 1, 20.0);
                const double c = pk.empty() ? 9999 : cents (pk[0], expect);
                const bool ok = std::abs (c) < 8.0;
                if (! ok) ++fails;
                std::printf ("%s %-18s f0=%7.2f attendu=%8.2f mesuré=%8.2f  écart=%+6.1f c %s\n",
                             chords == 2 ? "STUDIO " : (chords ? "CHORDS " : "CLASSIC"), p.name, f0, expect, pk.empty() ? 0.0 : pk[0], c, ok ? "ok" : "<-- ECHEC");
            }

    std::printf ("\n== Accord en mode CHORDS (G2 + D3 + G3), OCT UP orteil enfoncé ==\n");
    {
        Engine e; Settings s; s.programIdx = 1; s.chords = true; s.pedal = 1.0f; s.smoothMs = 5.0f;
        e.setSettings (s); e.prepare (fs, 512);
        auto y = run (e, s, tone (fs, n, { 98.0, 146.83, 196.0 }, false));
        auto pk = peaksHz (y, (int) (fs * 0.8), fs, 3, 60.0);
        std::sort (pk.begin(), pk.end());
        const double ex[3] = { 196.0, 293.66, 392.0 };
        for (int i = 0; i < 3 && i < (int) pk.size(); ++i)
        {
            const double c = cents (pk[(size_t) i], ex[i]);
            const bool ok = std::abs (c) < 10.0; if (! ok) ++fails;
            std::printf ("  pic %d : %.2f Hz (attendu %.2f)  %+5.1f c %s\n", i, pk[(size_t) i], ex[i], c, ok ? "ok" : "<-- ECHEC");
        }
    }

    std::printf ("\n== Accord grave en CHORDS live (G1 + D2 + G2), 5TH UP ==\n");
    {
        Engine e; Settings s; s.programIdx = 2; s.chords = true; s.pedal = 1.0f; s.smoothMs = 5.0f;
        e.setSettings (s); e.prepare (fs, 512);
        auto y = run (e, s, tone (fs, n, { 49.0, 73.42, 98.0 }, false));
        auto pk = peaksHz (y, (int) (fs * 0.8), fs, 3, 40.0);
        std::sort (pk.begin(), pk.end());
        const double r = std::exp2 (7.0 / 12.0), ex[3] = { 49.0 * r, 73.42 * r, 98.0 * r };
        for (int i = 0; i < 3 && i < (int) pk.size(); ++i)
        {
            const double c = cents (pk[(size_t) i], ex[i]);
            const bool ok = std::abs (c) < 15.0; if (! ok) ++fails;
            std::printf ("  pic %d : %.2f Hz (attendu %.2f)  %+5.1f c %s\n", i, pk[(size_t) i], ex[i], c, ok ? "ok" : "<-- ECHEC");
        }
    }

    std::printf ("\n== Accord grave en CHORDS Studio (G1 + D2 + G2), 5TH UP ==\n");
    {
        Engine e; Settings s; s.programIdx = 2; s.chords = true; s.studio = true; s.pedal = 1.0f; s.smoothMs = 5.0f;
        e.setSettings (s); e.prepare (fs, 512);
        auto y = run (e, s, tone (fs, n, { 49.0, 73.42, 98.0 }, false));
        auto pk = peaksHz (y, (int) (fs * 0.8), fs, 3, 40.0);
        std::sort (pk.begin(), pk.end());
        const double r = std::exp2 (7.0 / 12.0), ex[3] = { 49.0 * r, 73.42 * r, 98.0 * r };
        for (int i = 0; i < 3 && i < (int) pk.size(); ++i)
        {
            const double c = cents (pk[(size_t) i], ex[i]);
            const bool ok = std::abs (c) < 15.0; if (! ok) ++fails;
            std::printf ("  pic %d : %.2f Hz (attendu %.2f)  %+5.1f c %s\n", i, pk[(size_t) i], ex[i], c, ok ? "ok" : "<-- ECHEC");
        }
    }

    std::printf ("\n== HARMONY 3RD/4TH orteil levé : sec + tierce (sinus A3) ==\n");
    {
        Engine e; Settings s; s.programIdx = 18; s.pedal = 0.0f;
        e.setSettings (s); e.prepare (fs, 512);
        auto y = run (e, s, tone (fs, n, { 220.0 }, false));
        auto pk = peaksHz (y, (int) (fs * 0.8), fs, 2, 60.0);
        std::sort (pk.begin(), pk.end());
        const double ex[2] = { 220.0, 220.0 * std::exp2 (4.0 / 12.0) };
        for (int i = 0; i < 2 && i < (int) pk.size(); ++i)
        {
            const double c = cents (pk[(size_t) i], ex[i]);
            const bool ok = std::abs (c) < 8.0; if (! ok) ++fails;
            std::printf ("  pic %d : %.2f Hz (attendu %.2f)  %+5.1f c %s\n", i, pk[(size_t) i], ex[i], c, ok ? "ok" : "<-- ECHEC");
        }
    }

    std::printf ("\n== Balayage pédale en marches MIDI (0..127), CLASSIC OCT UP, guitare simulée ==\n");
    {
        Engine e; Settings s; s.programIdx = 1; s.smoothMs = 30.0f;
        e.setSettings (s); e.prepare (fs, 512);
        auto x = tone (fs, n, { 110.0 }, true);
        std::vector<float> y (x.size());
        float peak = 0.0f; bool finite = true;
        for (int b = 0; b < n / 256; ++b)
        {
            const int cc = std::min (127, b * 127 / (n / 256 / 2));
            s.pedal = (float) (b < n / 512 ? cc : 127 - std::min (127, (b - n / 512) * 127 / (n / 512))) / 127.0f;
            e.setSettings (s);
            e.process (x.data() + b * 256, y.data() + b * 256, 256);
        }
        for (float v : y) { finite = finite && std::isfinite (v); peak = std::max (peak, std::abs (v)); }
        const bool ok = finite && peak < 1.5f && peak > 0.05f; if (! ok) ++fails;
        std::printf ("  crête %.3f, valeurs finies : %s %s\n", peak, finite ? "oui" : "non", ok ? "ok" : "<-- ECHEC");
    }

    std::printf ("\n== Bypass : sortie = entrée (CLASSIC) ==\n");
    {
        Engine e; Settings s; s.active = false;
        e.setSettings (s); e.prepare (fs, 512);
        auto x = tone (fs, 48000, { 330.0 }, true);
        auto y = run (e, s, x);
        float err = 0.0f; for (size_t i = 0; i < x.size(); ++i) err = std::max (err, std::abs (x[i] - y[i]));
        const bool ok = err < 1e-6f; if (! ok) ++fails;
        std::printf ("  écart max %.2e %s\n", err, ok ? "ok" : "<-- ECHEC");
    }

    std::printf ("\n%s (%d échec%s)\n", fails == 0 ? "TOUS LES TESTS PASSENT" : "DES TESTS ÉCHOUENT", fails, fails > 1 ? "s" : "");
    return fails == 0 ? 0 : 1;
}
