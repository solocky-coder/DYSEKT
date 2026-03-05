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

    // ── Audio device setup ────────────────────────────────────────────────────
    formatManager.registerBasicFormats();
    deviceManager.initialiseWithDefaultDevices (0, 2);
    deviceManager.addAudioCallback (&sourcePlayer);
    sourcePlayer.setSource (&transport);
    transport.addChangeListener (this);

    // ── Play/Stop button ──────────────────────────────────────────────────────
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

    // ── Volume slider ─────────────────────────────────────────────────────────
    volumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.setRange (0.0, 1.0);
    volumeSlider.setValue (0.8);
    volumeSlider.setColour (juce::Slider::thumbColourId,
                            getTheme().accent);
    volumeSlider.setColour (juce::Slider::trackColourId,
                            getTheme().accent.withAlpha (0.25f));
    volumeSlider.onValueChange = [this]
    {
        transport.setGain ((float) volumeSlider.getValue());
    };
    transport.setGain (0.8f);
    addChildComponent (volumeSlider);

    // ── File name label ───────────────────────────────────────────────────────
    fileNameLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
    fileNameLabel.setColour (juce::Label::textColourId, getTheme().accent);
    fileNameLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0x00000000));
    fileNameLabel.setMinimumHorizontalScale (0.5f);
    fileNameLabel.setEditable (false, false, false);  // read-only: never editable
    addChildComponent (fileNameLabel);

    // Make the FileBrowserComponent's built-in filename TextEditor and path bar
    // read-only with black background — walk ALL descendants recursively.
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
    juce::Timer::callAfterDelay (500,  [enforceReadOnly] { enforceReadOnly(); });  // catch lazy-init children
}

FileBrowserPanel::~FileBrowserPanel()
{
    browser.setLookAndFeel (nullptr);
    transport.removeChangeListener (this);
    transport.stop();
    transport.setSource (nullptr);
    readerSource.reset();
    sourcePlayer.setSource (nullptr);
    deviceManager.removeAudioCallback (&sourcePlayer);
    browser.removeListener (this);
    ioThread.stopThread (2000);
}

// ── Layout ────────────────────────────────────────────────────────────────────

void FileBrowserPanel::resized()
{
    auto bounds = getLocalBounds();

    if (previewVisible)
    {
        // Reserve bar at the bottom
        auto bar = bounds.removeFromBottom (kBarH);

        // Play/stop button — square on the left
        playStopBtn.setBounds (bar.removeFromLeft (kBarH).reduced (4));

        // Volume slider — fixed width on the right
        volumeSlider.setBounds (bar.removeFromRight (90).reduced (4, 8));

        // File name label fills the rest
        fileNameLabel.setBounds (bar.reduced (6, 4));
    }

    browser.setBounds (bounds);
}

void FileBrowserPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF000000));  // true black per design

    if (previewVisible)
    {
        auto bar = getLocalBounds().removeFromBottom (kBarH);
        g.setColour (juce::Colour (0xFF0A0A0A));
        g.fillRect (bar);
        g.setColour (getTheme().separator);
        g.drawLine ((float) bar.getX(), (float) bar.getY(),
                    (float) bar.getRight(), (float) bar.getY(), 1.0f);
    }
}

// ── FileBrowserListener ───────────────────────────────────────────────────────

void FileBrowserPanel::fileClicked (const juce::File& f, const juce::MouseEvent&)
{
    if (! f.existsAsFile()) return;

    // Show the preview bar if not already visible
    const bool wasVisible = previewVisible;
    previewFile    = f;
    previewVisible = true;

    fileNameLabel.setText (f.getFileName(), juce::dontSendNotification);
    updatePlayButton();

    playStopBtn.setVisible   (true);
    volumeSlider.setVisible  (true);
    fileNameLabel.setVisible (true);

    // Auto-start preview on single click
    stopPreview();
    startPreview (f);

    if (! wasVisible)
        resized();    // re-layout to make room for bar

    repaint();
}

void FileBrowserPanel::fileDoubleClicked (const juce::File& f)
{
    stopPreview();
    if (! f.existsAsFile()) return;

    auto ext = f.getFileExtension().toLowerCase();

    if (ext == ".sf2" || ext == ".sfz")
    {
        // SF2/SFZ loading: hand off to the processor's soundfont loader
        processor.loadSoundFontAsync (f);
        if (onFileLoaded) onFileLoaded();
        return;
    }

    // Route regular audio files through the trim dialog if wired up
    if (onLoadRequest)
    {
        onLoadRequest (f);
        if (onFileLoaded) onFileLoaded();
    }
    else
    {
        processor.loadFileAsync (f);
        if (onFileLoaded) onFileLoaded();
    }
}

// ── Preview engine ────────────────────────────────────────────────────────────

void FileBrowserPanel::startPreview (const juce::File& f)
{
    if (! f.existsAsFile()) return;

    transport.stop();
    transport.setSource (nullptr);
    readerSource.reset();

    auto* reader = formatManager.createReaderFor (f);
    if (reader == nullptr) return;

    readerSource = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
    transport.setSource (readerSource.get(), 0, nullptr,
                         reader->sampleRate);
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
    // Called on audio thread — use async to safely update UI
    juce::MessageManager::callAsync ([this]
    {
        // Auto-stop UI when playback reaches end naturally
        if (! transport.isPlaying())
            updatePlayButton();
    });
}
