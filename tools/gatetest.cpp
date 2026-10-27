/* Unity-gain check for the STFT gate: a bypassed gate must reconstruct the input. */
#include "../src/DSP.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

int main (int argc, char** argv)
{
    constexpr double sr = 48000.0;
    const double freq = argc > 1 ? std::atof (argv[1]) : 1000.0;

    tag::SpectralGate gate;
    gate.prepare (sr);

    double sumIn = 0.0, sumOut = 0.0;
    double peakIn = 0.0, peakOut = 0.0;

    for (int n = 0; n < (int) sr; ++n)
    {
        const float x = (float) (0.5 * std::sin (6.283185307179586 * freq * n / sr));
        const float y = gate.process (x, 0.01f, false);

        if (n > 20000)
        {
            sumIn += (double) x * x;
            sumOut += (double) y * y;
            peakIn = std::max (peakIn, (double) std::abs (x));
            peakOut = std::max (peakOut, (double) std::abs (y));
        }
    }

    std::printf ("in peak %.3f out peak %.3f  gain %+.2f dB\n",
                 peakIn, peakOut, 10.0 * std::log10 (sumOut / sumIn));
    return 0;
}
