/* Renders a short animation of the editor: dumps frame_000.png ... into a folder,
   so the README demo gif can be assembled from real UI frames. */
#include "BitrotEditor.h"
#include "BitrotProcessor.h"

#include <cmath>
#include <cstdio>

int main (int argc, char** argv)
{
    const char* outDir = argc > 1 ? argv[1] : "frames";
    const int frameCount = argc > 2 ? std::atoi (argv[2]) : 60;

    juce::ScopedJuceInitialiser_GUI init;

    BitrotProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    auto setParam = [&proc] (const char* id, float normalised)
    {
        if (auto* p = proc.apvts.getParameter (id))
            p->setValueNotifyingHost (normalised);
    };

    setParam ("lmode", 0.0f);        // STANDARD
    setParam ("amount", 0.62f);
    setParam ("speed", 0.45f);
    setParam ("texture", 0.4f);
    setParam ("weird", 0.45f);
    setParam ("gate", 0.35f);

    juce::File dir = juce::File::getCurrentWorkingDirectory().getChildFile (outDir);
    dir.createDirectory();

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());

    if (editor == nullptr)
    {
        std::printf ("FAIL: no editor\n");
        return 1;
    }

    editor->setVisible (true);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    double phase = 0.0;
    double thump = 0.0;
    int frame = 0;
    int blockInFrame = 0;
    const int blocksPerFrame = 4;   // ~43 ms per frame at 512 samples

    const int totalBlocks = frameCount * blocksPerFrame;

    for (int block = 0; block < totalBlocks; ++block)
    {
        const double t = (double) block * 512.0 / 48000.0;

        for (int i = 0; i < 512; ++i)
        {
            const double time = t + i / 48000.0;

            // a little arpeggio plus a kick-ish transient every 500 ms
            const int step = (int) (time * 2.0) % 4;
            const double notes[4] = { 110.0, 164.81, 220.0, 146.83 };

            double s = 0.30 * std::sin (juce::MathConstants<double>::twoPi * notes[step] * time);
            s += 0.12 * std::sin (juce::MathConstants<double>::twoPi * notes[step] * 2.0 * time);
            s += 0.05 * std::sin (juce::MathConstants<double>::twoPi * 3200.0 * time);

            const double local = std::fmod (time, 0.5);
            thump = std::exp (-local * 18.0) * 0.5;
            s += thump * std::sin (juce::MathConstants<double>::twoPi * 55.0 * time);

            const float sample = (float) juce::jlimit (-1.0, 1.0, s);
            buffer.setSample (0, i, sample);
            buffer.setSample (1, i, sample * (float) (0.92 + 0.08 * std::sin (time * 3.1)));
        }

        proc.processBlock (buffer, midi);

        if (++blockInFrame >= blocksPerFrame)
        {
            blockInFrame = 0;

            if (auto* bitrot = dynamic_cast<BitrotEditor*> (editor.get()))
                bitrot->updateScopeNow();

            juce::Image image (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
            {
                juce::Graphics g (image);
                editor->paintEntireComponent (g, true);
            }

            const auto file = dir.getChildFile (juce::String ("frame_") + juce::String (frame).paddedLeft ('0', 3) + ".png");
            file.deleteFile();

            if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
            {
                juce::PNGImageFormat png;
                png.writeImageToStream (image, *stream);
            }

            ++frame;
        }
    }

    std::printf ("OK wrote %d frames to %s\n", frame, dir.getFullPathName().toRawUTF8());
    editor.reset();
    (void) phase;
    return 0;
}
