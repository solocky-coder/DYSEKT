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
                           juce::Colour (0xFF1A6B3A));
    playStopBtn.setColour (juce::TextButton::textColourOffId,
                           juce::Colours::white);
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
                            juce::Colour (0xFF4AABFF));
    volumeSlider.setColour (juce::Slider::trackColourId,
                            juce::Colour (0xFF2A4060));
    volumeSlider.onValueChange = [this]
    {
        transport.setGain ((float) volumeSlider.getValue());
    };
    transport.setGain (0.8f);
    addChildComponent (volumeSlider);

    // ── File name label ───────────────────────────────────────────────────────
    fileNameLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
    fileNameLabel.setColour (juce::Label::textColourId,
                             juce::Colours::white.withAlpha (0.7f));
    fileNameLabel.setMinimumHorizontalScale (0.5f);
    addChildComponent (fileNameLabel);
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
    g.fillAll (juce::Colour (0xFF050608));

    if (previewVisible)
    {
        // Draw a slightly raised bar behind the preview controls
        auto bar = getLocalBounds().removeFromBottom (kBarH);
        g.setColour (juce::Colour (0xFF040507));
        g.fillRect (bar);
        g.setColour (juce::Colour (0xFF0C1018));
        g.drawLine ((float) bar.getX(), (float) bar.getY(),
                    (float) bar.getRight(), (float) bar.getY(), 1.0f);

        // Draw PLAY (green triangle) or STOP (red square) icon over the button
        if (playStopBtn.isVisible())
        {
            const bool playing = transport.isPlaying();
            auto b = playStopBtn.getBounds().toFloat();
            float cx = b.getCentreX();
            float cy = b.getCentreY();
            const float sz = juce::jmin (b.getWidth(), b.getHeight()) * 0.36f;

            if (playing)
            {
                // Red square = STOP
                g.setColour (juce::Colours::red.brighter (0.3f));
                g.fillRect (juce::Rectangle<float> (cx - sz, cy - sz, sz * 2.0f, sz * 2.0f));
            }
            else
            {
                // Green right-pointing triangle = PLAY
                juce::Path tri;
                tri.addTriangle (cx - sz * 0.7f, cy - sz,
                                 cx - sz * 0.7f, cy + sz,
                                 cx + sz,        cy);
                g.setColour (juce::Colours::limegreen.brighter (0.2f));
                g.fillPath (tri);
            }
        }
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
        playStopBtn.setButtonText ("");
        playStopBtn.setColour (juce::TextButton::buttonColourId,
                               juce::Colour (0xFF8B1A1A));
    }
    else
    {
        playStopBtn.setButtonText ("");
        playStopBtn.setColour (juce::TextButton::buttonColourId,
                               juce::Colour (0xFF1A6B3A));
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
