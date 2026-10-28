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
#include <cstring>
#include <iterator>
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
        void bandpass (double sr, double fc, double q)
        {
            fc = std::min (std::max (fc, 5.0), sr * 0.45);
            const double w = 2 * pi * fc / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
            set (al, 0, -al, 1 + al, -2 * c, 1 - al);
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
    // Moteur d'octaves polyphonique facon FOG Z (d'apres sa notice) : banc de ~190 filtres complexes (3 etages),
    // seuls les pics du spectre parlent, phase divisee par 2 / par 4 suivie echantillon par echantillon,
    // porte de bruit par bande, suiveur d'arret, porte de repos (12 dB au-dessus du bruit appris).
    // Preset Baritone Growl : DRY -14,4 dB, -2 -17,0 dB, -1 +1,1 dB, +1/+2 coupes, LP 1,3 kHz Q 3,2 (niveau compense),
    // DRY FX off (le direct reste propre), limiteur doux en sortie.
    struct PolyOctave
    {
        static constexpr double Fi = 12000.0;              // cadence interne
        struct Band { float cr, ci, g0;                   // pole complexe et gain de normalisation
                      float s[3][2];                      // 3 etages (re, im)
                      float mag = 0, gain = 0;
                      float u1r = 1, u1i = 0, u2r = 1, u2i = 0; bool live = false; };
        std::vector<Band> bands; std::vector<float> mags;
        SincResampler dn, upR; Biquad lpf, dcIn; double fs = 48000;
        float att = 0, rel = 0, comp = 1;
        float envF = 0, envS = 0, floorN = 1e-4f, aF = 0, aS = 0, floorUp = 0, stopG = 0, idleG = 0, aStop = 0, aIdle = 0;
        float gDry = 0.1905f, g1 = 1.135f, g2 = 0.141f;
        void prepare (double sr)
        {
            fs = sr; bands.clear();
            dn.prepare (sr, Fi); upR.prepare (Fi, sr);
            std::vector<double> cf;
            for (double f = 21.0; f < 360.0; f += 7.0) cf.push_back (f);
            for (double f = 360.0; f < 5200.0; f *= std::pow (2.0, 1.0 / 36.0)) cf.push_back (f);
            for (size_t k = 0; k < cf.size(); ++k)
            {
                const double spacing = k + 1 < cf.size() ? cf[k + 1] - cf[k] : cf[k] - cf[k - 1];
                const double b1 = (0.5 * spacing * 1.15) / 0.5098;          // demi-largeur par etage (3 etages -> -3 dB a +-espacement/2)
                const double r = std::exp (-2 * pi * b1 / Fi), w = 2 * pi * cf[k] / Fi;
                Band b; b.cr = (float) (r * std::cos (w)); b.ci = (float) (r * std::sin (w)); b.g0 = (float) (1 - r);
                std::memset (b.s, 0, sizeof (b.s)); bands.push_back (b);
            }
            mags.assign (bands.size(), 0.0f);
            lpf.lowpass (Fi, 1300.0, 3.2); comp = (float) (1.0 / std::sqrt (3.2 / 0.7071));
            dcIn.highpass (Fi, 18.0, 0.7);
            att = (float) std::exp (-1.0 / (0.003 * Fi)); rel = (float) std::exp (-1.0 / (0.012 * Fi));
            aF = (float) std::exp (-1.0 / (0.030 * Fi)); aS = (float) std::exp (-1.0 / (0.200 * Fi));
            floorUp = (float) std::exp (-1.0 / (3.0 * Fi));
            aStop = (float) std::exp (-1.0 / (0.002 * Fi)); aIdle = (float) std::exp (-1.0 / (0.010 * Fi));
            reset();
        }
        void reset()
        {
            for (auto& b : bands) { std::memset (b.s, 0, sizeof (b.s)); b.mag = b.gain = 0; b.live = false; }
            dn.reset(); upR.reset(); lpf.reset(); dcIn.reset();
            envF = envS = 0; floorN = 1.0e-4f; stopG = idleG = 0;
        }
        static inline void csqrtTrack (float xr, float xi, float& pr, float& pi_)   // racine du phaseur unitaire, continue
        {
            float a = std::sqrt (std::max (0.0f, 0.5f * (1 + xr))), b = std::sqrt (std::max (0.0f, 0.5f * (1 - xr)));
            if (xi < 0) b = -b;
            if (a * pr + b * pi_ < 0) { a = -a; b = -b; }
            pr = a; pi_ = b;
        }
        float tick (float x)                                 // un echantillon a 12 kHz -> voix d'octaves filtrees
        {
            x = (float) dcIn.process (x);
            const float ax = std::abs (x);
            envF = std::max (ax, envF * aF); envS = std::max (ax, envS * aS);
            floorN = envS < floorN ? envS + 0.0005f * (floorN - envS) : floorN / floorUp;   // plancher de bruit appris
            floorN = std::max (floorN, 3.0e-6f);
            const bool idleOpen = envS > 4.0f * floorN;                                    // 12 dB au-dessus du bruit
            const bool stopping = envF < 0.12f * envS;                                     // la corde vient d'etre etouffee
            const float tS = stopping ? 0.0f : 1.0f; stopG = tS + aStop * (stopG - tS);
            const float tI = idleOpen ? 1.0f : 0.0f; idleG = tI + aIdle * (idleG - tI);
            const size_t N = bands.size();
            for (size_t k = 0; k < N; ++k)
            {
                Band& b = bands[k];
                float ir = x, ii = 0;
                for (int st = 0; st < 3; ++st)
                {
                    const float sr = b.s[st][0], si = b.s[st][1];
                    const float nr = ir + b.cr * sr - b.ci * si, ni = ii + b.cr * si + b.ci * sr;
                    b.s[st][0] = nr; b.s[st][1] = ni; ir = nr * b.g0; ii = ni * b.g0;
                }
                mags[k] = std::sqrt (ir * ir + ii * ii);
            }
            float v1 = 0, v2 = 0;
            const float bandFloor = 2.0f * floorN + 2.0e-5f;
            for (size_t k = 0; k < N; ++k)
            {
                Band& b = bands[k];
                const float m = mags[k];
                const bool peak = m > bandFloor && (k == 0 || m > mags[k - 1]) && (k + 1 == N || m >= mags[k + 1]);
                const float t = peak ? 1.0f : 0.0f;
                b.gain = t + (t > b.gain ? att : rel) * (b.gain - t);
                if (b.gain < 1.0e-4f) { b.live = false; continue; }
                const float zr = b.s[2][0] * b.g0, zi = b.s[2][1] * b.g0;
                const float inv = m > 1e-12f ? 1.0f / m : 0.0f, ur = zr * inv, ui = zi * inv;
                if (! b.live)
                {   // reprise : on part de la racine principale
                    b.u1r = 1; b.u1i = 0; csqrtTrack (ur, ui, b.u1r, b.u1i);
                    b.u2r = 1; b.u2i = 0; csqrtTrack (b.u1r, b.u1i, b.u2r, b.u2i); b.live = true;
                }
                else { csqrtTrack (ur, ui, b.u1r, b.u1i); csqrtTrack (b.u1r, b.u1i, b.u2r, b.u2i); }
                const float a = m * b.gain;
                v1 += a * b.u1r; v2 += a * b.u2r;
            }
            const float wet = 2.0f * (g1 * v1 + g2 * v2) * stopG * idleG;   // filtre complexe : moitie de l amplitude
            return (float) lpf.process (wet) * comp;
        }
        float process (float x)                               // cadence de l'hote
        {
            dn.push (x);
            while (dn.canPull()) upR.push (tick (dn.pull()));
            const float o = upR.canPull() ? upR.pull() : 0.0f;
            const float y = gDry * x + o;
            return std::tanh (y * 0.8f) / 0.8f;                // limiteur doux
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
        PolyOctave poly; bool polyCold = true; FuzzTone toneF; Smooth2 sDist, sTone, sVol, sOct;
        float wCur[numModels] {}; int toneCounter = 0;
        // etat general (frequence du projet)
        Preamp pre; Smooth2 sGain, sPres, sBass, sMid, sTreb, sMaster, sFuzzOn, sPreOn; int preCounter = 0;
        Biquad gateDetHP; float gateEnv = 0, gateGain = 1, gateRelEnv = 0.999f, gateAtt = 0.03f, gateRel = 0.0002f; bool gateOpen = true; int gateHold = 0, gateHoldLen = 2880;
        bool first = true;
        Params cur;
    };
}
