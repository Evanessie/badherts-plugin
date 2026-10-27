/* Checks whether the 3-band split actually sums flat.
   Pure DSP.h, no JUCE - compiled straight with g++. */
#include "../src/DSP.h"

#include <cmath>
#include <cstdio>
#include <vector>

static const double sr = 48000.0;
static const float f1 = 250.0f, f2 = 2500.0f;

int main()
{

    struct Bands
    {
        tag::Lr4Split a, b, c, d; // a=lp1, b=hp1, c=lp2, d=hp2
        void set()
        {
            a.set (f1, sr);
            b.set (f1, sr);
            c.set (f2, sr);
            d.set (f2, sr);
        }
        float sum (float x)
        {
            const float low = a.lp (x);
            const float highSide = b.hp (x);
            const float mid = c.lp (highSide);
            const float high = d.hp (highSide);
            return low + mid + high;
        }
    };

    const double freqs[] = { 40, 100, 250, 500, 1000, 2500, 5000, 10000 };

    for (double f : freqs)
    {
        Bands bands;
        bands.set();

        double phase = 0.0, sumSqIn = 0.0, sumSqOut = 0.0, peakOut = 0.0;
        const int n = (int) sr;

        for (int i = 0; i < n; ++i)
        {
            const float x = (float) std::sin (6.283185307179586 * f * phase);
            phase += 1.0 / sr;
            const float y = bands.sum (x);

            if (i > n / 4) // ignore the start-up transient
            {
                sumSqIn += (double) x * x;
                sumSqOut += (double) y * y;
                peakOut = std::max (peakOut, (double) std::abs (y));
            }
        }

        const double gainDb = 10.0 * std::log10 (sumSqOut / sumSqIn);
        std::printf ("%7.0f Hz   sum %+6.2f dB   peak %.3f\n", f, gainDb, peakOut);
    }

    return 0;
}
