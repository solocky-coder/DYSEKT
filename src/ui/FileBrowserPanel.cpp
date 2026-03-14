#include "FileBrowserPanel.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"

FileBrowserPanel::FileBrowserPanel (DysektProcessor& p)
    : processor (p),
      fileFilter ("*.wav;*.aif;*.aiff;*.ogg;*.flac;*.mp3;*.sf2;*.sfz", "*", "Audio & SoundFont Files"),
      dirList (&fileFilter, ioThread),
      browser (juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectFiles,
               juce::File::getSpecialLocation (juce::File::userHomeDirectory),
               &fileFilter, nullptr)
{
    ioThread.startThread();
    browser.addListener (this);
    browser.setLookAndFeel (&smallLAF);
    addAndMakeVisible (browser);

    // Audio preview setup
    formatManager.registerBasicFormats();
    sourcePlayer.setSource (&transport);
    transport.addChangeListener (this);

    // Play/Stop button
    playStopBtn.setColour (juce::TextButton::buttonColourId,
                           getTheme().accent.withAlpha (0.35f));
    playStopBtn.setColour (juce::TextButton::textColourOffId,
                           getTheme().foreground);
    playStopBtn.onClick = [this]
    {
        if (transport.isPlaying())
            stopPreview();
        else
            startPreview (previewFile);
    };
    addChildComponent (playStopBtn);

    // Volume slider
    volumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.setRange (0.0, 1.0);
    volumeSlider.setValue (0.8);
    volumeSlider.setColour (juce::Slider::thumbColourId, getTheme().accent);
    volumeSlider.setColour (juce::Slider::trackColourId, getTheme().accent.withAlpha (0.25f));
    volumeSlider.onValueChange = [this]
    {
        transport.setGain ((float) volumeSlider.getValue());
    };
    transport.setGain (0.8f);
    addChildComponent (volumeSlider);

    // File name label
    fileNameLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
    fileNameLabel.setColour (juce::Label::textColourId, getTheme().accent);
    fileNameLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0x00000000));
    fileNameLabel.setMinimumHorizontalScale (0.5f);
    fileNameLabel.setEditable (false, false, false);
    addChildComponent (fileNameLabel);

    // Make browser text/panels read-only
    auto enforceReadOnly = [this]
    {
        std::function<void(juce::Component*)> walk = [&](juce::Component* comp)
        {
            if (auto* te = dynamic_cast<juce::TextEditor*> (comp))
            {
                te->setReadOnly (true);
                te->setCaretVisible (false);
                te->setMouseCursor (juce::MouseCursor::NormalCursor);
                te->setColour (juce::TextEditor::backgroundColourId,    juce::Colour (0xFF000000));
                te->setColour (juce::TextEditor::outlineColourId,       getTheme().separator);
                te->setColour (juce::TextEditor::focusedOutlineColourId, getTheme().accent.withAlpha (0.5f));
                te->setColour (juce::TextEditor::textColourId,          getTheme().accent);
            }
            if (auto* lb = dynamic_cast<juce::Label*> (comp))
            {
                lb->setEditable (false, false, false);
                lb->setColour (juce::Label::backgroundColourId, juce::Colour (0xFF000000));
                lb->setColour (juce::Label::textColourId,       getTheme().accent);
            }
            for (int i = 0; i < comp->getNumChildComponents(); ++i)
                walk (comp->getChildComponent (i));
        };
        walk (&browser);
    };

    juce::Timer::callAfterDelay (100,  [enforceReadOnly] { enforceReadOnly(); });
    juce::Timer::callAfterDelay (500,  [enforceReadOnly] { enforceReadOnly(); });
}

FileBrowserPanel::~FileBrowserPanel()
{
    browser.setLookAndFeel (nullptr);
    transport.removeChangeListener (this);
    transport.stop();
    transport.setSource (nullptr);
    readerSource.reset();
    sourcePlayer.setSource (nullptr);
    if (deviceManager.getCurrentAudioDevice() != nullptr)
        deviceManager.removeAudioCallback (&sourcePlayer);
    browser.removeListener (this);
    ioThread.stopThread (2000);
}

// ---- Layout ----

void FileBrowserPanel::resized()
{
    auto bounds = getLocalBounds();

    if (previewVisible)
    {
        auto bar = bounds.removeFromBottom (kBarH);
        playStopBtn.setBounds (bar.removeFromLeft (kBarH).reduced (4));
        volumeSlider.setBounds (bar.removeFromRight (90).reduced (4, 8));
        fileNameLabel.setBounds (bar.reduced (6, 4));
    }

    browser.setBounds (bounds);
}

void FileBrowserPanel::paint (juce::Graphics& g)
{
    // LCD/sys theme for the browser panel
    const auto ac = getTheme().accent;
    auto b = getLocalBounds();

    juce::ColourGradient outerGrad (juce::Colour (0xFF131313), 0, 0,
                                     juce::Colour (0xFF0E0E0E), 0, (float) b.getHeight(), false);
    g.setGradientFill (outerGrad);
    g.fillRoundedRectangle (b.toFloat(), 4.0f);

    g.setColour (ac.withAlpha (0.20f));
    g.drawRoundedRectangle (b.toFloat().reduced (0.5f), 4.0f, 1.0f);

    auto screen = b.reduced (4);
    g.setColour (getTheme().darkBar.darker (0.55f));
    g.fillRoundedRectangle (screen.toFloat(), 2.0f);

    g.setColour (juce::Colours::black.withAlpha (0.18f));
    for (int y = screen.getY(); y < screen.getBottom(); y += 2)
        g.drawHorizontalLine (y, (float) screen.getX(), (float) screen.getRight());

    juce::ColourGradient glow (ac.withAlpha (0.06f), 0, (float) screen.getY(),
                                juce::Colours::transparentBlack, 0, (float) (screen.getY() + 20), false);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (screen.toFloat(), 2.0f);

    g.setColour (ac.withAlpha (0.12f));
    g.drawRoundedRectangle (screen.toFloat().expanded (0.5f), 2.0f, 1.0f);

    // Preview bar at bottom
    if (previewVisible)
    {
        auto bar = getLocalBounds().removeFromBottom (kBarH);
        g.setColour (getTheme().darkBar.darker (0.6f));
        g.fillRect (bar);
        g.setColour (getTheme().accent.withAlpha (0.25f));
        g.drawLine ((float) bar.getX(), (float) bar.getY(),
                    (float) bar.getRight(), (float) bar.getY(), 1.0f);
    }
}

// ---- FileBrowserListener ----

void FileBrowserPanel::fileClicked (const juce::File& f, const juce::MouseEvent&)
{
    if (! f.existsAsFile()) return;

    const bool wasVisible = previewVisible;
    previewFile    = f;
    previewVisible = true;

    fileNameLabel.setText (f.getFileName(), juce::dontSendNotification);

    stopPreview();
    updatePlayButton();

    playStopBtn.setVisible   (true);
    volumeSlider.setVisible  (true);
    fileNameLabel.setVisible (true);

    if (! wasVisible)
        resized();

    repaint();
}

void FileBrowserPanel::fileDoubleClicked (const juce::File& f)
{
    stopPreview();
    if (! f.existsAsFile()) return;

    auto ext = f.getFileExtension().toLowerCase();

    if (ext == ".sf2" || ext == ".sfz")
    {
        // SF2/SFZ loading
        processor.loadSoundFontAsync (f);
        // Always reset zoom/scroll after file load
        processor.zoom.store(1.0f);
        processor.scroll.store(0.0f);
        if (onFileLoaded) onFileLoaded();
        return;
    }

    // Route audio files to waveform: always reset zoom/scroll after file load
    if (onLoadRequest)
    {
        onLoadRequest (f);
        processor.zoom.store(1.0f);
        processor.scroll.store(0.0f);
        if (onFileLoaded) onFileLoaded();
    }
    else
    {
        processor.loadFileAsync (f);
        processor.zoom.store(1.0f);
        processor.scroll.store(0.0f);
        if (onFileLoaded) onFileLoaded();
    }
}

// ---- Preview engine ----

void FileBrowserPanel::startPreview (const juce::File& f)
{
    if (! f.existsAsFile()) return;

    if (deviceManager.getCurrentAudioDevice() == nullptr)
    {
        deviceManager.initialise (0, 2, nullptr, true, {}, nullptr);
        deviceManager.addAudioCallback (&sourcePlayer);
    }

    transport.stop();
    transport.setSource (nullptr);
    readerSource.reset();

    auto* reader = formatManager.createReaderFor (f);
    if (reader == nullptr) return;

    readerSource = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
    transport.setSource (readerSource.get(), 0, nullptr, reader->sampleRate);
    transport.setGain ((float) volumeSlider.getValue());
    transport.setPosition (0.0);
    transport.start();

    updatePlayButton();
}

void FileBrowserPanel::stopPreview()
{
    transport.stop();
    transport.setPosition (0.0);
    updatePlayButton();
}

void FileBrowserPanel::updatePlayButton()
{
    if (transport.isPlaying())
    {
        playStopBtn.setButtonText ("STOP");
        playStopBtn.setColour (juce::TextButton::buttonColourId,
                               getTheme().accent.withAlpha (0.55f));
    }
    else
    {
        playStopBtn.setButtonText ("PLAY");
        playStopBtn.setColour (juce::TextButton::buttonColourId,
                               getTheme().accent.withAlpha (0.25f));
    }
}

void FileBrowserPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    juce::MessageManager::callAsync ([this]
    {
        if (! transport.isPlaying())
            updatePlayButton();
    });
}
