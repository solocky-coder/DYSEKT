#include "LogoBar.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"
#include <BinaryData.h>

LogoBar::LogoBar (DysektProcessor& p) : processor (p) {}

void LogoBar::paint (juce::Graphics& g)
{
    g.fillAll (getTheme().header);

    auto logoDrawable = juce::Drawable::createFromImageData (
        BinaryData::dysekt_logo_svg, BinaryData::dysekt_logo_svgSize);

    if (logoDrawable != nullptr)
    {
        logoDrawable->replaceColour (juce::Colours::black,      getTheme().accent);
        logoDrawable->replaceColour (juce::Colour (0xFF000000), getTheme().accent);

        const int lx = 10, ly = 4, lw = 150, lh = getHeight() - 8;
        logoDrawable->setTransformToFit (
            juce::Rectangle<float> ((float)lx, (float)ly, (float)lw, (float)lh),
            juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid
            | juce::RectanglePlacement::onlyReduceInSize);
        logoDrawable->draw (g, 1.0f);
    }
}
