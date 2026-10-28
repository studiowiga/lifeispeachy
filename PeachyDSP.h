#pragma once
#include <string>
//==============================================================================
// PEACHY (Studio Wiga) - moteur audio
//
// Reproduction d'une pedale double « preampli + fuzz » au son des sessions metal de 1994-1996
// (7 cordes accordees bas, preampli serre et sature, fuzz laineux a gros sustain, octave grave).
//
// Trajet du signal (mono, comme la pedale) :
//   entree -> [FUZZ] -> [PREAMP] -> sortie        (le fuzz attaque le preampli : « le fuzz se melange au drive »)
//
// FUZZ (footswitch droit, potards DISTORTION / TONE / VOLUME, inverseur OCTAVE)
//   Le son vient directement des 45 captures NAM de la vraie pedale (fuzz seul, DISTORTION 50 a 100 %,
//   TONE 1 a 9), fournies par l'utilisateur :
//    - analyse des captures : le TONE agit comme un filtre lineaire APRES l'ecretage (coherence ~1 entre
//      sinus glisse et riff, memes courbes quel que soit le gain) ; il est donc reproduit par un filtre exact
//      (rapport de deux polynomes du 2e ordre ajustes sur les 9 positions mesurees, erreur rms 0,4 dB),
//      applique a la sortie des captures TONE 5 ;
//    - DISTORTION : les 5 captures TONE 5 (G50 / 65 / 75 / 90 / 100) tournent en reseau WaveNet
//      (NeuralAmpModelerCore, MIT) et sont fondues entre deux voisines ; sous 50 % : capture G50 avec une
//      attenuation d'entree (moins de saturation, comme le potard d'origine) ;
//    - les captures sont a 48 kHz : reechantillonnage sinc (Kaiser) si le projet tourne a une autre frequence ;
//    - OCTAVE : octave grave analogique (bascule a la frequence moitie multipliee par le signal, facon
//      diviseur a bascule), ajoutee devant le fuzz ; suit les cordes graves (G de 7 cordes et plus bas).
// PREAMP (footswitch gauche, potards GAIN / MASTER / BASS / MID / TREBLE / PRESENCE)
//   Pas de captures du preampli : modelisation d'un preampli a lampes high gain (trois etages, ecretage
//   asymetrique, serrage du grave avant la saturation), tone stack passif a 3 bandes calcule exactement
//   (fonction de transfert de Yeh, valeurs d'une tete britannique), presence en sortie ; sur-echantillonnage x4.
//==============================================================================
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <memory>
#include <vector>
#include <atomic>

namespace nam { class DSP; }

namespace peachy
{
    constexpr double pi = 3.14159265358979323846;
    inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
    inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

    //--------------------------------------------------------------------------
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
        { b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0; }
        void lowpass (double sr, double fc, double q = 0.7071)
        {
            fc = std::min (std::max (fc, 10.0), sr * 0.45);
            const double w = 2 * pi * fc / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
            set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
        }
        void highpass (double sr, double fc, double q = 0.7071)
        {
            fc = std::min (std::max (fc, 5.0), sr * 0.45);
            const double w = 2 * pi * fc / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
            set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
        }
        void peak (double sr, double fc, double q, double db)
        {
            fc = std::min (std::max (fc, 10.0), sr * 0.45);
            const double A = std::pow (10.0, db / 40.0), w = 2 * pi * fc / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
            set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
        }
        void lowShelf (double sr, double fc, double db, double s = 0.8)
        {
            fc = std::min (std::max (fc, 10.0), sr * 0.45);
            const double A = std::pow (10.0, db / 40.0), w = 2 * pi * fc / sr, c = std::cos (w), sn = std::sin (w);
            const double al = sn / 2 * std::sqrt ((A + 1 / A) * (1 / s - 1) + 2), sa = 2 * std::sqrt (A) * al;
            set (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
                 (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
        }
        void highShelf (double sr, double fc, double db, double s = 0.8)
        {
            fc = std::min (std::max (fc, 10.0), sr * 0.45);
            const double A = std::pow (10.0, db / 40.0), w = 2 * pi * fc / sr, c = std::cos (w), sn = std::sin (w);
            const double al = sn / 2 * std::sqrt ((A + 1 / A) * (1 / s - 1) + 2), sa = 2 * std::sqrt (A) * al;
            set (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
                 (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
        }
        inline float process (float xf) noexcept
        {
            const double x = xf, y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y;
            return (float) y;
        }
        void reset() { z1 = z2 = 0; }
    };

    struct OnePole
    {
        float a = 0, z = 0;
        void setLP (double sr, double fc) { a = (float) std::exp (-2 * pi * fc / sr); }
        inline float lp (float x) noexcept { z = x + a * (z - x); return z; }
        inline float hp (float x) noexcept { z = x + a * (z - x); return x - z; }
        void reset() { z = 0; }
    };

    struct Smooth2
    {
        float c = 0.01f, s1 = 0, s2 = 0;
        void setTime (double sr, double ms) { c = (float) (1.0 - std::exp (-1.0 / (ms * 0.001 * sr))); }
        void reset (float v) { s1 = s2 = v; }
        inline float next (float t) noexcept { s1 += (t - s1) * c; s2 += (s1 - s2) * c; return s2; }
    };

    //--------------------------------------------------------------------------
    // Sur-echantillonnage x4 (RIF a phase lineaire, Kaiser), polyphase - repris de Doorn
    struct Oversampler4
    {
        static constexpr int factor = 4, taps = 128;
        float h[taps] {}, hist[taps * 2] {}, upHist[taps / factor * 2] {};
        int pos = 0, upPos = 0;
        static double bessel0 (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; }
        void design()
        {
            const double fc = 0.5 / factor * 0.90, beta = 8.6; double sum = 0;
            for (int n = 0; n < taps; ++n)
            {
                const double m = n - (taps - 1) * 0.5, x = 2 * fc * m;
                const double sinc = std::abs (x) < 1e-12 ? 1.0 : std::sin (pi * x) / (pi * x);
                const double r = 2.0 * n / (taps - 1) - 1.0;
                h[n] = (float) (2 * fc * sinc * bessel0 (beta * std::sqrt (std::max (0.0, 1 - r * r))) / bessel0 (beta));
                sum += h[n];
            }
            for (auto& v : h) v = (float) (v / sum);
            reset();
        }
        void reset() { std::fill (std::begin (hist), std::end (hist), 0.0f); std::fill (std::begin (upHist), std::end (upHist), 0.0f); pos = upPos = 0; }
        static double latency() { return (taps - 1) / (double) factor; }
        inline void up (float x, float* out) noexcept
        {
            constexpr int L = taps / factor;
            upPos = (upPos + L - 1) % L; upHist[upPos] = x; upHist[upPos + L] = x;
            const float* hp = upHist + upPos;
            for (int ph = 0; ph < factor; ++ph) { float acc = 0; for (int k = 0; k < L; ++k) acc += h[k * factor + ph] * hp[k]; out[ph] = acc * factor; }
        }
        inline float down (const float* in) noexcept
        {
            for (int i = 0; i < factor; ++i) { pos = (pos + taps - 1) % taps; hist[pos] = in[i]; hist[pos + taps] = in[i]; }
            const float* hp = hist + pos; float acc = 0;
            for (int k = 0; k < taps; ++k) acc += h[k] * hp[k];
            return acc;
        }
    };

    //--------------------------------------------------------------------------
    // Reechantillonneur en flux (sinc fenetre de Kaiser, table interpolee) : frequence du projet <-> 48 kHz
    struct SincResampler
    {
        static constexpr int ringSize = 1 << 14, ringMask = ringSize - 1, tabRes = 256;
        std::vector<float> ring, tab;
        int64_t nIn = 0, tInt = 0; double tFrac = 0, step = 1;
        int hw = 24;

        void prepare (double fsIn, double fsOut)
        {
            step = fsIn / fsOut;
            const double scale = std::min (1.0, fsOut / fsIn);
            hw = (int) std::ceil (24.0 / scale);
            const double fc = 0.5 * scale * 0.94, beta = 9.0;
            tab.assign ((size_t) (hw * tabRes + 2), 0.0f);
            for (int i = 0; i <= hw * tabRes + 1; ++i)
            {
                const double d = (double) i / tabRes, x = 2 * fc * d;
                const double sinc = std::abs (x) < 1e-12 ? 1.0 : std::sin (pi * x) / (pi * x);
                const double r = d / hw;
                const double w = r >= 1.0 ? 0.0 : Oversampler4::bessel0 (beta * std::sqrt (1 - r * r)) / Oversampler4::bessel0 (beta);
                tab[(size_t) i] = (float) (2 * fc * sinc * w);
            }
            ring.assign (ringSize, 0.0f);
            reset();
        }
        void reset() { std::fill (ring.begin(), ring.end(), 0.0f); nIn = 0; tInt = 0; tFrac = 0; }
        inline void push (float x) noexcept { ring[(size_t) (nIn & ringMask)] = x; ++nIn; }
        inline bool canPull() const noexcept { return tInt + hw + 1 < nIn; }
        inline float h (double d) const noexcept
        {
            d = std::abs (d) * tabRes;
            const int i = (int) d; if (i >= hw * tabRes) return 0.0f;
            const float f = (float) (d - i);
            return tab[(size_t) i] + (tab[(size_t) i + 1] - tab[(size_t) i]) * f;
        }
        inline float pull() noexcept
        {
            float acc = 0;
            for (int k = -hw + 1; k <= hw; ++k)
                acc += ring[(size_t) ((tInt + k) & ringMask)] * h ((double) k - tFrac);
            tFrac += step;
            const double fl = std::floor (tFrac);
            tInt += (int64_t) fl; tFrac -= fl;
            return acc;
        }
    };

    //--------------------------------------------------------------------------
    // TONE du fuzz : rapport N_T(s) / N_5(s) (polynomes du 2e ordre ajustes sur les captures TONE 1..9),
    // s normalise a 2 pi x 1 kHz. Coefficients (c2, c1, c0) pour TONE = 1..9.
    struct FuzzTone
    {
        static constexpr double coef[9][3] = {
            { 0.081209, 1.158855, 3.821663 },
            { 0.318717, 2.323742, 3.858816 },
            { 0.601054, 3.755854, 3.561338 },
            { 0.711720, 4.293808, 3.284278 },
            { 0.854392, 4.614294, 3.112302 },
            { 0.815425, 4.870272, 2.801257 },
            { 0.891895, 4.862276, 2.018288 },
            { 1.785458, 3.804186, 1.448075 },
            { 2.284732, 3.010262, 1.427896 } };
        Biquad bq;
        double fs = 48000;
        // knob 0..10 -> position de capture 1..9 (5 = 5)
        void set (float knob)
        {
            const double T = 1.0 + 0.8 * clampf (knob, 0.0f, 10.0f);
            const int i0 = std::min (7, std::max (0, (int) std::floor (T) - 1));
            const double f = std::min (1.0, std::max (0.0, T - (i0 + 1)));
            double c[3];
            for (int k = 0; k < 3; ++k) c[k] = std::exp (std::log (coef[i0][k]) * (1 - f) + std::log (coef[i0 + 1][k]) * f);
            const double K = 2 * fs / (2 * pi * 1000.0), K2 = K * K;
            const double* d = coef[4];
            bq.set (c[0] * K2 + c[1] * K + c[2], -2 * c[0] * K2 + 2 * c[2], c[0] * K2 - c[1] * K + c[2],
                    d[0] * K2 + d[1] * K + d[2], -2 * d[0] * K2 + 2 * d[2], d[0] * K2 - d[1] * K + d[2]);
        }
    };

    //--------------------------------------------------------------------------
    // Octave grave analogique : detection (filtre + comparateur a hysteresis), bascule a f/2,
    // signal filtre multiplie par la bascule (enveloppe et dynamique du jeu conservees)
    struct OctaveDown
    {
        Biquad det1, det2, body; OnePole smooth, envF;
        float env = 0, relEnv = 0.999f, ff = 1.0f; bool high = false;
        double fs = 48000;
        void prepare (double sr)
        {
            fs = sr;
            det1.lowpass (sr, 220.0, 0.6); det2.lowpass (sr, 300.0, 0.6);
            body.lowpass (sr, 900.0, 0.7);
            smooth.setLP (sr, 1400.0);
            relEnv = (float) std::exp (-1.0 / (0.040 * sr));
            reset();
        }
        void reset() { det1.reset(); det2.reset(); body.reset(); smooth.reset(); env = 0; ff = 1.0f; high = false; }
        inline float process (float x) noexcept
        {
            const float d = det2.process (det1.process (x));
            env = std::max (std::abs (d), env * relEnv);
            const float th = 0.18f * env + 1.0e-5f;
            if (! high && d > th)        { high = true; ff = -ff; }       // front montant : bascule
            else if (high && d < -th)    { high = false; }
            return smooth.lp (body.process (x) * ff);
        }
    };

    //--------------------------------------------------------------------------
    // Tone stack passif 3 bandes (fonction de transfert analogique exacte, Yeh & Smith 2006),
    // valeurs d'une tete britannique : C1 470 pF, C2 = C3 = 22 nF, treble 220 k, bass 1 M (log), mid 25 k, R4 33 k.
    struct ToneStack
    {
        double b[4] {}, a[4] {}, z[3] {};
        void set (double fs, double t, double m, double l)
        {
            const double C1 = 470e-12, C2 = 22e-9, C3 = 22e-9, R1 = 220e3, R2 = 1e6, R3 = 25e3, R4 = 33e3;
            const double b1 = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
            const double b2 = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4) - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                            + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                            + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
                            + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                            + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
            const double b3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                            - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                            + m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                            + t * C1 * C2 * C3 * R1 * R3 * R4 - t * m * C1 * C2 * C3 * R1 * R3 * R4
                            + t * l * C1 * C2 * C3 * R1 * R2 * R4;
            const double a1 = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) + m * C3 * R3 + l * (C1 * R2 + C2 * R2);
            const double a2 = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                            + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3) - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                            + l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
                            + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);
            const double a3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                            - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                            + m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4)
                            + l * C1 * C2 * C3 * R1 * R2 * R4 + C1 * C2 * C3 * R1 * R3 * R4;
            // transformation bilineaire (s = c (1 - z^-1) / (1 + z^-1))
            const double c = 2 * fs, c2 = c * c, c3 = c2 * c;
            const double B0 = -b1 * c - b2 * c2 - b3 * c3, B1 = -b1 * c + b2 * c2 + 3 * b3 * c3, B2 = b1 * c + b2 * c2 - 3 * b3 * c3, B3 = b1 * c - b2 * c2 + b3 * c3;
            const double A0 = -1 - a1 * c - a2 * c2 - a3 * c3, A1 = -3 - a1 * c + a2 * c2 + 3 * a3 * c3, A2 = -3 + a1 * c + a2 * c2 - 3 * a3 * c3, A3 = -1 + a1 * c - a2 * c2 + a3 * c3;
            b[0] = B0 / A0; b[1] = B1 / A0; b[2] = B2 / A0; b[3] = B3 / A0;
            a[1] = A1 / A0; a[2] = A2 / A0; a[3] = A3 / A0;
        }
        inline float process (float xf) noexcept
        {
            const double x = xf, y = b[0] * x + z[0];
            z[0] = b[1] * x - a[1] * y + z[1];
            z[1] = b[2] * x - a[2] * y + z[2];
            z[2] = b[3] * x - a[3] * y;
            return (float) y;
        }
        void reset() { z[0] = z[1] = z[2] = 0; }
    };

    //--------------------------------------------------------------------------
    struct Params
    {
        float master = 5, gain = 6, volume = 5, distortion = 7.5f, bass = 5, mid = 5, treble = 5, tone = 5, presence = 5;
        bool  octave = false, preampOn = true, fuzzOn = false;
        int   gate = 1;           // 0 off, 1 light, 2 medium, 3 strong
    };

    // volume en sortie : midi = 0 dB (niveau des captures), courbe audio continue, 0 = coupe
    inline float levelGain (float knob)
    {
        const float v = clampf (knob, 0.0f, 10.0f) / 5.0f;
        return std::pow (v, 1.7f);
    }

    //--------------------------------------------------------------------------
    // Preampli (frequence du projet, etages non lineaires en x4)
    class Preamp
    {
    public:
        void prepare (double sr);
        void reset();
        // un echantillon ; parametres deja lisses
        float process (float x, float gain, float pres, float bass, float mid, float treble, int& counter) noexcept;
    private:
        void updateTone (float bass, float mid, float treble, float pres);
        double fs = 48000, fsOS = 192000;
        Oversampler4 os;
        Biquad hpIn, tight, push, cut1, cut2, presF, postLP;
        OnePole cpl1, cpl2, cpl3, mil1, mil2;
        ToneStack ts;
        float lastB = -1, lastM = -1, lastT = -1, lastP = -1;
    };

    //--------------------------------------------------------------------------
    class Engine
    {
    public:
        static constexpr int numModels = 5;
        static constexpr float gridG[numModels] = { 50.0f, 65.0f, 75.0f, 90.0f, 100.0f };

        Engine();
        ~Engine();
        // modeles NAM (JSON .nam en memoire) ; a appeler hors du thread audio
        bool loadModel (int index, const char* json, size_t size);
        void setEco (bool eco);                                  // hors thread audio : sous-modele leger
        void prepare (double sampleRate, int maxBlock);
        void process (float* const* io, int numChannels, int numSamples, const Params& p) noexcept;
        int  latency() const { return latencySamples; }
        bool modelsReady() const { return modelsOk; }
        const std::string& loadError() const { return lastError; }

        std::atomic<float> fuzzShown { 0.0f }, preShown { 0.0f };
        std::atomic<bool>  modelBad[numModels] {};       // reseau ecarte (sortie non finie), a reinitialiser
        std::atomic<int>   badEvents { 0 };
        bool anyBad() const { for (auto& b : modelBad) if (b.load()) return true; return false; }
        bool repairModels();                             // hors thread audio (processeur suspendu)

    private:
        void processChunk (float* mono, int n, const Params& p) noexcept;
        void fuzz48 (float* buf, int m) noexcept;
        int  measureResamplerLatency();

        std::unique_ptr<nam::DSP> models[numModels];
        bool modelsOk = false;
        std::string lastError;
        double fs = 48000; bool resample = false;
        SincResampler up, down;
        int maxChunk = 256, max48 = 512, latencySamples = 0, fuzzLat = 0, preLat = 32;
        std::vector<float> in48, out48, mix48, tmp48, io48, dryF, dryP, chunkBuf, gateLine;
        int dryFPos = 0, dryPPos = 0;
        // etat fuzz (domaine 48 kHz)
        OctaveDown oct; FuzzTone toneF; Smooth2 sDist, sTone, sVol, sOct;
        float wCur[numModels] {}; int toneCounter = 0;
        // etat general (frequence du projet)
        Preamp pre; Smooth2 sGain, sPres, sBass, sMid, sTreb, sMaster, sFuzzOn, sPreOn; int preCounter = 0;
        Biquad gateDetHP; float gateEnv = 0, gateGain = 1, gateRelEnv = 0.999f, gateAtt = 0.03f, gateRel = 0.0002f; bool gateOpen = true; int gateHold = 0, gateHoldLen = 2880;
        bool first = true;
        Params cur;
    };
}
