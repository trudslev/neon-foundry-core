#include "nf/testing/ComponentRender.h"

namespace nf::testing
{

juce::Image renderComponent (juce::Component& c, float scale)
{
    const int w = juce::roundToInt ((float) c.getWidth()  * scale);
    const int h = juce::roundToInt ((float) c.getHeight() * scale);
    jassert (w > 0 && h > 0);

    juce::Image image { juce::Image::ARGB, juce::jmax (1, w), juce::jmax (1, h), true };
    juce::Graphics g { image };
    g.addTransform (juce::AffineTransform::scale (scale));

    /*  `paintEntireComponent` rather than `paint`, because the children are the point: the About
        tab and the About box are children of the editor content, and a `paint` call would render
        the background alone and look like a panel with nothing on it. */
    c.paintEntireComponent (g, true);
    return image;
}

bool writeComponentPng (juce::Component& c, const juce::File& destination, float scale)
{
    destination.getParentDirectory().createDirectory();
    destination.deleteFile();

    juce::FileOutputStream out { destination };
    if (! out.openedOk())
        return false;

    juce::PNGImageFormat png;
    return png.writeImageToStream (renderComponent (c, scale), out);
}

} // namespace nf::testing
