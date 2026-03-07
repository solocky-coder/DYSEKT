#include "DysektLookAndFeel.h"
#include "BinaryData.h"

static ThemeData globalTheme = ThemeData::darkTheme();

ThemeData& getTheme() { return globalTheme; }
void setTheme (const ThemeData& t) { globalTheme = t; }

juce::Typeface::Ptr DysektLookAndFeel::sRegularTypeface;
juce::Typeface::Ptr DysektLookAndFeel::sBoldTypeface;
float DysektLookAndFeel::sMenuScale = 1.0f;

DysektLookAndFeel::DysektLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, getTheme().background);

    regularTypeface = juce::Typeface::createSystemTypefaceFor (
        BinaryData::IBMPlexSansRegular_ttf, BinaryData::IBMPlexSansRegular_ttfSize);
    boldTypeface = juce::Typeface::createSystemTypefaceFor (
        BinaryData::IBMPlexSansBold_ttf, BinaryData::IBMPlexSansBold_ttfSize);

    sRegularTypeface = regularTypeface;
    sBoldTypeface = boldTypeface;
}

juce::Font DysektLookAndFeel::makeFont (float pointSize, bool bold)
{
    auto tf = bold ? sBoldTypeface : sRegularTypeface;
    if (tf != nullptr)
        return juce::Font (juce::FontOptions().withTypeface (tf).withPointHeight (pointSize));
    return juce::Font (juce::FontOptions().withHeight (pointSize));
}

juce::Typeface::Ptr DysektLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (f.isBold())
        return boldTypeface;
    return regularTypeface;
}

void DysektLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                  const juce::Colour& /*bgColour*/,
                                                  bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat();

    // Use the button's own colour if it has been explicitly set
    auto btnCol = button.findColour (juce::TextButton::buttonColourId);
    auto baseBg = (btnCol != juce::Colour()) ? btnCol : getTheme().button;

    if (baseBg.isTransparent())
        return;

    // Sharp fill — no rounded corners
    g.setColour (isDown ? baseBg.brighter (0.15f)
                        : isHighlighted ? baseBg.brighter (0.08f)
                                        : baseBg);
    g.fillRect (bounds);

    // Frame: draw a 1px border following the theme
    auto frameCol = button.getToggleState()
                    ? getTheme().accent
                    : getTheme().separator;
    g.setColour (isHighlighted ? frameCol.brighter (0.2f) : frameCol);
    g.drawRect (bounds, 1.0f);
}

void DysektLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                            bool /*isHighlighted*/, bool /*isDown*/)
{
    auto textCol = button.findColour (button.getToggleState()
                                       ? juce::TextButton::textColourOnId
                                       : juce::TextButton::textColourOffId);
    if (textCol.isTransparent())
        textCol = button.getToggleState() ? getTheme().accent : getTheme().foreground;

    g.setColour (textCol);

    // Scale font tightly to button height — sharper appearance for small top buttons
    const int h = button.getHeight();
    float fontSize = h < 16 ? 8.0f
                   : h < 22 ? 10.0f
                   : h < 28 ? 11.0f
                   : 13.0f;
    g.setFont (makeFont (fontSize));
    g.drawText (button.getButtonText(), button.getLocalBounds().reduced (2, 0),
                juce::Justification::centred);
}

void DysektLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (getTheme().darkBar);
    g.setColour (getTheme().separator);
    g.drawRect (0, 0, width, height, 1);
}

void DysektLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                               bool isSeparator, bool isActive, bool isHighlighted,
                                               bool isTicked, bool /*hasSubMenu*/,
                                               const juce::String& text, const juce::String& /*shortcutText*/,
                                               const juce::Drawable* /*icon*/, const juce::Colour* /*textColour*/)
{
    if (isSeparator)
    {
        g.setColour (getTheme().separator);
        g.fillRect (area.reduced (4, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }

    // Always fill the full row background first — this prevents any icon drawable
    // or OS compositing artifact from bleeding through the untouched area.
    g.setColour (isHighlighted && isActive ? getTheme().buttonHover : getTheme().darkBar);
    g.fillRect (area);

    // Tick indicator: draw a small filled rect (no unicode glyphs that may render as random symbols)
    const int tickZoneW = (int) (16 * sMenuScale);
    if (isTicked)
    {
        const int dotSize = (int) (4 * sMenuScale);
        g.setColour (getTheme().accent);
        g.fillRect (area.getX() + (tickZoneW - dotSize) / 2,
                    area.getCentreY() - dotSize / 2,
                    dotSize, dotSize);
    }

    const juce::Colour textCol = isTicked  ? getTheme().accent
                               : isActive  ? getTheme().foreground
                                           : getTheme().foreground.withAlpha (0.4f);
    g.setColour (textCol);
    g.setFont (getPopupMenuFont());

    // Indent text past tick zone to prevent overlap
    auto textArea = area.withLeft (area.getX() + tickZoneW)
                        .withRight (area.getRight() - (int) (4 * sMenuScale));
    g.drawText (text, textArea, juce::Justification::centredLeft);
}

void DysektLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g,
                                                        const juce::Rectangle<int>& area,
                                                        const juce::String& sectionName)
{
    g.setFont (getPopupMenuFont().boldened());
    g.setColour (getTheme().foreground);
    g.drawFittedText (sectionName,
                      area.getX() + (int) (12 * sMenuScale), area.getY(),
                      area.getWidth() - (int) (16 * sMenuScale),
                      (int) ((float) area.getHeight() * 0.8f),
                      juce::Justification::bottomLeft, 1);
}

juce::Font DysektLookAndFeel::getPopupMenuFont()
{
    return makeFont (15.0f * sMenuScale);
}

void DysektLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    g.fillAll (getTheme().darkBar.brighter (0.1f));
    g.setColour (getTheme().separator);
    g.drawRect (0, 0, width, height, 1);
    g.setColour (getTheme().foreground);
    g.setFont (makeFont (14.0f));
    g.drawText (text, 4, 0, width - 8, height, juce::Justification::centredLeft);
}

juce::Rectangle<int> DysektLookAndFeel::getTooltipBounds (const juce::String& text,
                                                              juce::Point<int> screenPos,
                                                              juce::Rectangle<int> parentArea)
{
    int w = (int) makeFont (14.0f).getStringWidthFloat (text) + 14;
    int h = 24;
    int x = screenPos.x;
    int y = screenPos.y + 18;

    if (x + w > parentArea.getRight())
        x = parentArea.getRight() - w;
    if (y + h > parentArea.getBottom())
        y = screenPos.y - h - 4;

    return { x, y, w, h };
}