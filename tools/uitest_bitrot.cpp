/* Offscreen render of the Bitrot editor, with a bit of audio pushed through first
   so the scope actually has a waveform in it. */
#include "BitrotEditor.h"
#include "BitrotProcessor.h"

#include <cstdio>

int main (int argc, char** argv)
{
    const char* outPath = argc > 1 ? argv[1] : "bitrot.png";

    juce::ScopedJuceInitialiser_GUI init;

    BitrotProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    if (auto* p = proc.apvts.getParameter ("loss"))
        p->setValueNotifyingHost (0.72f);

    if (auto* p = proc.apvts.getParameter ("gate"))
        p->setValueNotifyingHost (0.52f);

    if (auto* p = proc.apvts.getParameter ("ftype"))
        p->setValueNotifyingHost (1.0f / 3.0f);   // LP24 (index 2 of 0..6)

    if (auto* p = proc.apvts.getParameter ("cutoff"))
        p->setValueNotifyingHost (p->convertTo0to1 (4200.0f));

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    double phase = 0.0;

    for (int block = 0; block < 40; ++block)
    {
        for (int i = 0; i < 512; ++i)
        {
            const float s = (float) (0.6 * std::sin (6.283185307179586 * 220.0 * phase));
            phase += 1.0 / 48000.0;
            buffer.setSample (0, i, s);
            buffer.setSample (1, i, s);
        }

        proc.processBlock (buffer, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());

    if (auto* bitrot = dynamic_cast<BitrotEditor*> (editor.get()))
        bitrot->updateScopeNow();

    if (editor == nullptr)
    {
        std::printf ("FAIL: no editor\n");
        return 1;
    }

    editor->setVisible (true);

    juce::Image image (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
    {
        juce::Graphics g (image);
        editor->paintEntireComponent (g, true);
    }

    // also grab a CLEAN-mode frame for comparison
    if (auto* m = proc.apvts.getParameter ("mode"))
    {
        m->setValueNotifyingHost (0.0f);
        std::printf ("mode after set: %.3f\n", proc.apvts.getRawParameterValue ("mode")->load());
        juce::Image clean (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
        { juce::Graphics cg (clean); editor->paintEntireComponent (cg, true); }
        juce::File cf (juce::File::getCurrentWorkingDirectory().getChildFile ("bitrot_clean.png"));
        cf.deleteFile();
        if (auto cs = std::unique_ptr<juce::FileOutputStream> (cf.createOutputStream()))
        { juce::PNGImageFormat png; png.writeImageToStream (clean, *cs); }
    }

    // variant with the stereo field at 0, to check the H fade
    if (auto* f = proc.apvts.getParameter ("field"))
    {
        f->setValueNotifyingHost (0.0f);
        juce::Image f0 (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
        { juce::Graphics cg (f0); editor->paintEntireComponent (cg, true); }
        juce::File ff (juce::File::getCurrentWorkingDirectory().getChildFile ("bitrot_field0.png"));
        ff.deleteFile();
        if (auto fs = std::unique_ptr<juce::FileOutputStream> (ff.createOutputStream()))
        { juce::PNGImageFormat png; png.writeImageToStream (f0, *fs); }
        f->setValueNotifyingHost (0.5f);
    }

    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (outPath));
    out.deleteFile();

    if (auto stream = std::unique_ptr<juce::FileOutputStream> (out.createOutputStream()))
    {
        juce::PNGImageFormat png;
        png.writeImageToStream (image, *stream);
        std::printf ("OK wrote %s (%dx%d)\n", out.getFullPathName().toRawUTF8(),
                     image.getWidth(), image.getHeight());
    }
    else
    {
        std::printf ("FAIL: cannot write\n");
        return 2;
    }

    editor.reset();
    return 0;
}
