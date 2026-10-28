#include "PeachyDSP.h"

#include "NAM/dsp.h"
#include "NAM/get_dsp.h"
#include "NAM/slimmable.h"
#include "json.hpp"
#include "NAM/model_config.h"
#include "NAM/container.h"
#include "NAM/wavenet/model.h"

namespace peachy
{
    constexpr double FuzzTone::coef[9][3];
    constexpr float Engine::gridG[Engine::numModels];

    //==========================================================================
    // PREAMP
    //==========================================================================
    void Preamp::prepare (double sr)
    {
        fs = sr; fsOS = sr * Oversampler4::factor;
        os.design();
        hpIn.highpass (fs, 30.0, 0.707);
        tight.lowShelf (fs, 160.0, -7.0, 0.7);           // grave serre avant la saturation (7 cordes accordees bas)
        push.peak (fs, 850.0, 0.8, 3.5);                 // poussee de mediums avant l'ecretage (attaque, « crunch »)
        cpl1.setLP (fsOS, 60.0);  mil1.setLP (fsOS, 7500.0);
        cpl2.setLP (fsOS, 38.0);  mil2.setLP (fsOS, 6000.0);
        cpl3.setLP (fsOS, 25.0);
        cut1.lowpass (fsOS, 11000.0, 0.6);
        postLP.lowpass (fs, 9500.0, 0.6);
        lastB = lastM = lastT = lastP = -1;
        reset();
    }

    void Preamp::reset()
    {
        os.reset(); hpIn.reset(); tight.reset(); push.reset(); cut1.reset(); cut2.reset(); presF.reset(); postLP.reset();
        cpl1.reset(); cpl2.reset(); cpl3.reset(); mil1.reset(); mil2.reset(); ts.reset();
    }

    void Preamp::updateTone (float bass, float mid, float treble, float pres)
    {
        if (std::abs (bass - lastB) > 0.004f || std::abs (mid - lastM) > 0.004f || std::abs (treble - lastT) > 0.004f)
        {
            lastB = bass; lastM = mid; lastT = treble;
            const double x = clampf (bass, 0.0f, 10.0f) / 10.0;
            const double l = (std::exp (3.4 * x) - 1.0) / (std::exp (3.4) - 1.0);   // potentiometre logarithmique
            const double m = clampf (mid, 0.0f, 10.0f) / 10.0;
            const double t = clampf (treble, 0.0f, 10.0f) / 10.0;
            ts.set (fsOS, t, m, l);
        }
        if (std::abs (pres - lastP) > 0.004f)
        {
            lastP = pres;
            const double x = (clampf (pres, 0.0f, 10.0f) - 5.0) / 5.0;               // meme pente des deux cotes de midi
            presF.highShelf (fs, 3200.0, 9.0 * x + 1.5 * x * x * x, 0.75);
        }
    }

    // etage a triode : ecretage asymetrique doux (point de repos decale), composante continue retiree ensuite
    static inline float triode (float v) noexcept
    {
        constexpr float bias = 0.14f;
        const float u = v + bias;
        const float y = u > 0.0f ? std::tanh (u) : std::tanh (1.45f * u) / 1.45f;
        return y - 0.139092f;                                        // tanh (bias)
    }

    float Preamp::process (float x, float gain, float pres, float bass, float mid, float treble, int& counter) noexcept
    {
        if ((counter++ & 31) == 0) updateTone (bass, mid, treble, pres);
        const float gn = clampf (gain, 0.0f, 10.0f) / 10.0f;
        const float g1 = dbToGain (4.0f + 34.0f * std::pow (gn, 1.1f));
        const float g2 = dbToGain (2.0f + 16.0f * gn);

        x = push.process (tight.process (hpIn.process (x))) * g1;
        float u[4];
        os.up (x, u);
        for (auto& s : u)
        {
            float a = mil1.lp (cpl1.hp (triode (s)));
            float b = mil2.lp (cpl2.hp (triode (a * g2) * 0.9f));
            float c = ts.process (b) * 4.2f;                          // tone stack puis etage de rattrapage
            c = cpl3.hp (std::tanh (0.85f * c) / 0.85f);
            s = cut1.process (c);
        }
        float y = os.down (u);
        y = postLP.process (presF.process (y));
        return y * 0.30f;
    }

    //==========================================================================
    // ENGINE
    //==========================================================================
    Engine::Engine() = default;
    Engine::~Engine() = default;

    // Les architectures NAM s'enregistrent par des objets statiques ; dans la bibliotheque statique
    // du plugin (Windows / macOS), l'editeur de liens ecarte ces fichiers si rien ne les appelle :
    // le chargement echouait alors (fuzz muet). Enregistrement explicite, qui force aussi l'edition de liens.
    static void ensureNamArchitectures()
    {
        auto& reg = nam::ConfigParserRegistry::instance();
        if (! reg.has ("SlimmableContainer")) reg.registerParser ("SlimmableContainer", nam::container::create_config);
        if (! reg.has ("WaveNet"))            reg.registerParser ("WaveNet", nam::wavenet::create_config);
    }

    bool Engine::loadModel (int index, const char* json, size_t size)
    {
        if (index < 0 || index >= numModels) return false;
        try
        {
            ensureNamArchitectures();
            const auto j = nlohmann::json::parse (json, json + size);
            models[index] = nam::get_dsp (j);
        }
        catch (const std::exception& e) { models[index].reset(); lastError = e.what(); }
        catch (...) { models[index].reset(); lastError = "unknown error"; }
        modelsOk = true;
        for (auto& m : models) modelsOk = modelsOk && (m != nullptr);
        return models[index] != nullptr;
    }

    bool Engine::repairModels()
    {
        bool any = false;
        for (int i = 0; i < numModels; ++i)
            if (modelBad[i].load() && models[i])
            {
                models[i]->Reset (48000.0, max48);
                modelBad[i].store (false);
                any = true;
            }
        return any;
    }

    void Engine::setEco (bool eco)
    {
        for (auto& m : models)
            if (auto* s = dynamic_cast<nam::SlimmableModel*> (m.get()))
                s->SetSlimmableSize (eco ? 0.0 : 1.0);
    }

    int Engine::measureResamplerLatency()
    {
        // impulsion a travers la chaine de reechantillonnage (milieu = identite), par blocs comme en vrai
        const int prime = down.hw + (int) std::ceil (up.hw * 48000.0 / fs) + 8;
        up.reset(); down.reset();
        for (int i = 0; i < prime; ++i) down.push (0.0f);
        int best = 0; float bestV = 0; int t = 0;
        std::vector<float> mid ((size_t) max48);
        for (int blk = 0; blk < 64; ++blk)
        {
            for (int i = 0; i < maxChunk; ++i) up.push ((blk == 0 && i == 0) ? 1.0f : 0.0f);
            int m = 0; while (up.canPull() && m < max48) mid[(size_t) m++] = up.pull();
            for (int j = 0; j < m; ++j) down.push (mid[(size_t) j]);
            for (int i = 0; i < maxChunk; ++i, ++t)
            {
                const float v = down.canPull() ? down.pull() : 0.0f;
                if (std::abs (v) > bestV) { bestV = std::abs (v); best = t; }
            }
        }
        up.reset(); down.reset();
        for (int i = 0; i < prime; ++i) down.push (0.0f);
        return best;
    }

    void Engine::prepare (double sampleRate, int)
    {
        fs = sampleRate;
        resample = std::abs (fs - 48000.0) > 0.5;
        maxChunk = 256;
        max48 = (int) std::ceil (maxChunk * 48000.0 / fs) + 64;
        in48.assign ((size_t) max48, 0.0f); out48.assign ((size_t) max48, 0.0f);
        mix48.assign ((size_t) max48, 0.0f); tmp48.assign ((size_t) max48, 0.0f); io48.assign ((size_t) max48, 0.0f);
        chunkBuf.assign ((size_t) maxChunk, 0.0f); gateLine.assign ((size_t) maxChunk, 1.0f);
        for (auto& m : models) if (m) m->Reset (48000.0, max48);

        if (resample)
        {
            up.prepare (fs, 48000.0);
            down.prepare (48000.0, fs);
            fuzzLat = measureResamplerLatency();
        }
        else fuzzLat = 0;
        preLat = (int) std::lround (Oversampler4::latency());
        latencySamples = fuzzLat + preLat;
        dryF.assign ((size_t) (fuzzLat + 1), 0.0f); dryFPos = 0;
        dryP.assign ((size_t) (preLat + 1), 0.0f);  dryPPos = 0;

        poly.prepare (fs); polyCold = true; toneF.fs = 48000.0;
        for (auto* s : { &sDist, &sTone, &sVol }) s->setTime (48000.0, 25.0);
        sOct.setTime (fs, 5.0);
        pre.prepare (fs);
        for (auto* s : { &sGain, &sPres, &sBass, &sMid, &sTreb, &sMaster }) s->setTime (fs, 25.0);
        sFuzzOn.setTime (fs, 6.0); sPreOn.setTime (fs, 6.0);

        gateDetHP.highpass (fs, 110.0, 0.707);
        gateEnv = 0.0f; gateGain = 1.0f; gateOpen = true; gateHold = 0;
        gateRelEnv = (float) std::exp (-1.0 / (0.030 * fs));
        gateAtt    = (float) (1.0 - std::exp (-1.0 / (0.0008 * fs)));
        gateRel    = (float) (1.0 - std::exp (-1.0 / (0.120 * fs)));
        gateHoldLen = (int) (0.060 * fs);
        for (auto& w : wCur) w = 0.0f;
        first = true;
    }

    // poids des 5 captures pour une position de DISTORTION (%), fonctions « chapeau » entre voisines
    static void weightsFor (float g, float* w)
    {
        g = clampf (g, Engine::gridG[0], Engine::gridG[Engine::numModels - 1]);
        for (int i = 0; i < Engine::numModels; ++i) w[i] = 0.0f;
        for (int i = 0; i < Engine::numModels - 1; ++i)
            if (g <= Engine::gridG[i + 1])
            {
                const float f = (g - Engine::gridG[i]) / (Engine::gridG[i + 1] - Engine::gridG[i]);
                w[i] = 1.0f - f; w[i + 1] = f;
                return;
            }
        w[Engine::numModels - 1] = 1.0f;
    }

    void Engine::fuzz48 (float* buf, int m) noexcept
    {
        const bool active = cur.fuzzOn || sFuzzOn.s2 > 1.0e-4f;
        if (! active || ! modelsOk)
        {
            std::fill (buf, buf + m, 0.0f);
            return;
        }
        // entree des reseaux : attenuation sous 50 % de DISTORTION
        float wStart[numModels], wEnd[numModels];
        weightsFor (sDist.s2 * 10.0f, wStart);
        float* in = in48.data();
        for (int j = 0; j < m; ++j)
        {
            const float d = sDist.next (cur.distortion) * 10.0f;         // %
            const float att = d < 50.0f ? dbToGain (-0.46f * (50.0f - d)) : 1.0f;
            in[j] = buf[j] * att;
            mix48[(size_t) j] = d;                                       // memorise la position (poids par echantillon)
        }
        weightsFor (sDist.s2 * 10.0f, wEnd);
        std::fill (out48.begin(), out48.begin() + m, 0.0f);
        for (int i = 0; i < numModels; ++i)
        {
            if (wStart[i] <= 0.0f && wEnd[i] <= 0.0f) continue;
            if (modelBad[i].load (std::memory_order_relaxed)) continue;     // en reinitialisation (thread de messages)
            float* ip = in; float* op = tmp48.data();
            models[i]->process (&ip, &op, m);
            // garde-fou : un reseau qui sort une valeur non finie est ecarte puis reinitialise hors du thread audio
            bool ok = true;
            for (int j = 0; j < m; ++j) if (! std::isfinite (tmp48[(size_t) j])) { ok = false; break; }
            if (! ok) { modelBad[i].store (true); ++badEvents; continue; }
            for (int j = 0; j < m; ++j)
            {
                float w[numModels]; weightsFor (mix48[(size_t) j], w);
                out48[(size_t) j] += w[i] * tmp48[(size_t) j];
            }
        }
        for (int j = 0; j < m; ++j)
        {
            if ((toneCounter++ & 31) == 0) toneF.set (sTone.next (cur.tone)); else sTone.next (cur.tone);
            const float y = toneF.bq.process (out48[(size_t) j]);
            buf[j] = y * levelGain (sVol.next (cur.volume));
        }
    }

    void Engine::processChunk (float* x, int n, const Params& p) noexcept
    {
        cur = p;
        // noise gate : detection sur l'entree ; applique a la sortie retardee (anticipation naturelle)
        for (int i = 0; i < n; ++i)
        {
            float gg = 1.0f;
            if (p.gate > 0)
            {
                const float d = std::abs (gateDetHP.process (x[i]));
                gateEnv = std::max (d, gateEnv * gateRelEnv);
                static const float openDb[4] = { -200.0f, -72.0f, -64.0f, -54.0f };
                const float thrOpen = dbToGain (openDb[std::min (3, p.gate)]), thrClose = thrOpen * 0.5f;
                if (gateEnv > thrOpen)       { gateOpen = true; gateHold = gateHoldLen; }
                else if (gateEnv < thrClose) { if (gateHold > 0) --gateHold; else gateOpen = false; }
                const float tg = gateOpen ? 1.0f : 0.0f;
                gateGain += (tg - gateGain) * (tg > gateGain ? gateAtt : gateRel);
                gg = gateGain * gateGain;
            }
            else { gateGain = 1.0f; gateOpen = true; }
            gateLine[(size_t) i] = gg;
        }

        // ---- OCTAVE : generateur d'octaves polyphonique (son du preset Baritone Growl de FOG Z), avant le fuzz,
        // actif meme fuzz coupe
        if (p.octave || sOct.s2 > 1.0e-4f)
        {
            if (polyCold) { poly.reset(); polyCold = false; }
            for (int i = 0; i < n; ++i)
            {
                const float o = sOct.next (p.octave ? 1.0f : 0.0f);
                const float y = poly.process (x[i]);
                x[i] += (y - x[i]) * o;
            }
        }
        else { sOct.reset (0.0f); polyCold = true; }

        // ---- FUZZ (domaine 48 kHz des captures)
        float* fz = chunkBuf.data();
        if (resample)
        {
            for (int i = 0; i < n; ++i) up.push (x[i]);
            int m = 0;
            while (up.canPull() && m < max48) io48[(size_t) m++] = up.pull();
            fuzz48 (io48.data(), m);
            for (int j = 0; j < m; ++j) down.push (io48[(size_t) j]);
            for (int i = 0; i < n; ++i) fz[i] = down.canPull() ? down.pull() : 0.0f;
        }
        else
        {
            std::copy (x, x + n, fz);
            fuzz48 (fz, n);
        }

        for (int i = 0; i < n; ++i)
        {
            // signal direct aligne sur la latence du fuzz
            float dryFz = x[i];
            if (fuzzLat > 0)
            {
                dryF[(size_t) dryFPos] = x[i];
                dryFPos = (dryFPos + 1) % (int) dryF.size();
                dryFz = dryF[(size_t) dryFPos];
            }
            const float fon = sFuzzOn.next (p.fuzzOn ? 1.0f : 0.0f);
            const float y1 = dryFz + (fz[i] - dryFz) * fon;

            // ---- PREAMP
            const float pr = pre.process (y1, sGain.next (p.gain), sPres.next (p.presence), sBass.next (p.bass),
                                          sMid.next (p.mid), sTreb.next (p.treble), preCounter)
                             * levelGain (sMaster.next (p.master));
            dryP[(size_t) dryPPos] = y1;
            dryPPos = (dryPPos + 1) % (int) dryP.size();
            const float dryPr = dryP[(size_t) dryPPos];
            const float pon = sPreOn.next (p.preampOn ? 1.0f : 0.0f);
            float y = dryPr + (pr - dryPr) * pon;

            const float anyOn = std::max (std::max (fon, pon), sOct.s2);
            y *= 1.0f - anyOn * (1.0f - gateLine[(size_t) i]);
            // garde-fou (jamais de valeur aberrante vers l'hote)
            if (! std::isfinite (y)) y = 0.0f;
            x[i] = clampf (y, -4.0f, 4.0f);
        }
        fuzzShown = sFuzzOn.s2; preShown = sPreOn.s2;
    }

    void Engine::process (float* const* io, int numChannels, int numSamples, const Params& p) noexcept
    {
        if (first)
        {
            sDist.reset (p.distortion); sTone.reset (p.tone); sVol.reset (p.volume); sOct.reset (p.octave ? 1.0f : 0.0f);
            sGain.reset (p.gain); sPres.reset (p.presence); sBass.reset (p.bass); sMid.reset (p.mid); sTreb.reset (p.treble);
            sMaster.reset (p.master); sFuzzOn.reset (p.fuzzOn ? 1.0f : 0.0f); sPreOn.reset (p.preampOn ? 1.0f : 0.0f);
            toneF.set (p.tone);
            first = false;
        }
        int done = 0;
        float mono[256];
        while (done < numSamples)
        {
            const int n = std::min (maxChunk, numSamples - done);
            for (int i = 0; i < n; ++i)
            {
                float s = 0.0f;
                for (int c = 0; c < numChannels; ++c) s += io[c][done + i];
                mono[i] = numChannels > 1 ? s / (float) numChannels : s;
            }
            processChunk (mono, n, p);
            for (int c = 0; c < numChannels; ++c)
                std::copy (mono, mono + n, io[c] + done);
            done += n;
        }
    }
}
