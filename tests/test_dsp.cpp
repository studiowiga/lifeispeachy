// Banc d'essai hors plugin : charge les captures, passe un fichier brut float32 mono 48k a travers Peachy
// usage : test_dsp in.f32 out.f32 sr preampOn fuzzOn gain master dist tone vol bass mid treble pres octave
#include "PeachyDSP.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <chrono>
static std::string readFile (const char* p) { FILE* f = fopen (p, "rb"); std::string s; if (!f) return s; fseek (f, 0, SEEK_END); s.resize ((size_t) ftell (f)); fseek (f, 0, SEEK_SET); fread (&s[0], 1, s.size(), f); fclose (f); return s; }
int main (int argc, char** argv)
{
    if (argc < 4) { fprintf (stderr, "usage\n"); return 1; }
    peachy::Engine e;
    const char* names[5] = { "models/fuzz_G50.nam", "models/fuzz_G65.nam", "models/fuzz_G75.nam", "models/fuzz_G90.nam", "models/fuzz_G100.nam" };
    for (int i = 0; i < 5; ++i) { auto s = readFile (names[i]); if (! e.loadModel (i, s.data(), s.size())) fprintf (stderr, "load fail %d\n", i); }
    const double sr = atof (argv[3]);
    peachy::Params p;
    auto A = [&] (int i, float d) { return argc > i ? (float) atof (argv[i]) : d; };
    p.preampOn = A (4, 1) > 0.5f; p.fuzzOn = A (5, 0) > 0.5f; p.gain = A (6, 6); p.master = A (7, 5);
    p.distortion = A (8, 7.5f); p.tone = A (9, 5); p.volume = A (10, 5); p.bass = A (11, 5); p.mid = A (12, 5);
    p.treble = A (13, 5); p.presence = A (14, 5); p.octave = A (15, 0) > 0.5f; p.gate = (int) A (16, 0);
    if (argc > 17) e.setEco (atoi (argv[17]) != 0);
    FILE* f = fopen (argv[1], "rb"); fseek (f, 0, SEEK_END); long n = ftell (f) / 4; fseek (f, 0, SEEK_SET);
    std::vector<float> x ((size_t) n); fread (x.data(), 4, (size_t) n, f); fclose (f);
    const int B = 512; e.prepare (sr, B);
    fprintf (stderr, "latency %d\n", e.latency());
    std::vector<float> L (B), R (B), out;
    auto t0 = std::chrono::steady_clock::now();
    for (long pos = 0; pos < n; pos += B)
    {
        const int k = (int) std::min<long> (B, n - pos);
        for (int i = 0; i < k; ++i) L[i] = R[i] = x[(size_t) (pos + i)];
        float* io[2] = { L.data(), R.data() };
        e.process (io, 2, k, p);
        out.insert (out.end(), L.begin(), L.begin() + k);
    }
    const double el = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    fprintf (stderr, "cpu %.1f %% of realtime\n", 100.0 * el / (n / sr));
    f = fopen (argv[2], "wb"); fwrite (out.data(), 4, out.size(), f); fclose (f);
}
