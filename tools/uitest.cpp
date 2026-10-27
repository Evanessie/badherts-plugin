/* Renders the plugin editor offscreen to a PNG - catches crashes in the UI code
   and gives us something to eyeball without opening a DAW. */
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cstdio>

static void mark (const char* what)
{
    std::fprintf (stderr, "[ui] %s\n", what);
    std::fflush (stderr);
}

int main (int argc, char** argv)
{
    const char* outPath = argc > 1 ? argv[1] : "ui.png";

    mark ("init");
    juce::ScopedJuceInitialiser_GUI init;
    mark ("processor");

    TapeGrainProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    mark ("createEditor");

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    std::fprintf (stderr, "[ui] editor=%p\n", (void*) editor.get());
    std::fflush (stderr);

    if (editor == nullptr)
    {
        std::printf ("FAIL: createEditor returned null\n");
        return 1;
    }

    mark ("setSize");
    editor->setSize (680, 486);
    mark ("setVisible");
    editor->setVisible (true);
    mark ("image");

    juce::Image image (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);

    mark ("paint");
    {
        juce::Graphics g (image);
        editor->paintEntireComponent (g, true);
    }
    mark ("painted");

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
        std::printf ("FAIL: cannot write %s\n", out.getFullPathName().toRawUTF8());
        return 2;
    }

    mark ("teardown");
    editor.reset();
    mark ("done");
    return 0;
}
