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

    // ── Audio preview setup ───────────────────────────────────────────────────
    formatManager.registerBasicFormats();
    sourcePlayer.setSource (&transport);
    transport.addChangeListener (this);

    // ── Play/Stop icon button ─────────────────────────────────────────────────
    playStopBtn.setTooltip ("Preview");
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
    volumeSlider.setColour (juce::Slider::thumbColourId, getTheme().accent);
    volumeSlider.setColour (juce::Slider::trackColourId, getTheme().accent.withAlpha (0.25f));
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
    fileNameLabel.setEditable (false, false, false);
    addChildComponent (fileNameLabel);

    // Hiding browser filename bar & making editors read-only for safety
    auto enforceReadOnly = [this]
    {
        std::function<void(juce::Component*)> walk = [&](juce::Component* comp)
        {
            if (auto* te = dynamic_cast<juce::TextEditor*> (comp))
            {
                te->setReadOnly (true);
                te->setCaretVisible (false);
                te->setMouseCursor (juce::MouseCursor::NormalCursor);
                te->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xFF000000));
                te->setColour (juce::TextEditor::outlineColourId, getTheme().separator);
                te->setColour (juce::TextEditor::focusedOutlineColourId, getTheme().accent.withAlpha (0.5f));
                te->setColour (juce::TextEditor::textColourId, getTheme().accent);
                te->setVisible (false);
            }
            if (auto* lb = dynamic_cast<juce::Label*> (comp))
            {
                lb->setEditable (false, false, false);
                lb->setColour (juce::Label::backgroundColourId, juce::Colour (0xFF000000));
                lb->setColour (juce::Label::textColourId, getTheme().accent);
            }
            for (int i = 0; i < comp->getNumChildComponents(); ++i)
                walk (comp->getChildComponent (i));
        };
        walk (&browser);
    };

    juce::Timer::callAfterDelay (100,  [enforceReadOnly] { enforceReadOnly(); });
    juce::Timer::callAfterDelay (500,  [enforceReadOnly] { enforceReadOnly(); });
    // ── Cloud bookmarks ───────────────────────────────────────────────────────
    detectCloudFolders();
    loadCustomBookmarks();
    rebuildBookmarkBar();

    addBmBtn.setButtonText ("+");
    addBmBtn.setTooltip    ("Add folder bookmark");
    addBmBtn.setColour (juce::TextButton::buttonColourId,  getTheme().darkBar.darker (0.5f));
    addBmBtn.setColour (juce::TextButton::textColourOffId, getTheme().accent.withAlpha (0.90f));
    addBmBtn.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Choose a folder to bookmark",
            juce::File::getSpecialLocation (juce::File::userHomeDirectory));

        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectDirectories,
            [this] (const juce::FileChooser& fc)
            {
                auto result = fc.getResult();
                if (result.isDirectory())
                {
                    bookmarks.add ({ result.getFileName(), result, true });
                    saveCustomBookmarks();
                    rebuildBookmarkBar();
                    resized();
                    repaint();
                }
            });
    };
    addAndMakeVisible (addBmBtn);
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

// ── Layout ─────────────────────────────────────────────────────────────

void FileBrowserPanel::resized()
{
    auto bounds = getLocalBounds();

    // ── Bookmark bar at the top ────────────────────────────────────────────
    {
        auto bmBar = bounds.removeFromTop (kBmH);
        const int addW = 22;
        const int gap  = 3;
        const int n    = bmBtns.size();
        const int avail = bmBar.getWidth() - addW - gap - 4;
        const int btnW = n > 0 ? juce::jmin (90, juce::jmax (40, (avail - gap * (n - 1)) / n)) : 0;

        int bx = bmBar.getX() + 2;
        for (auto* btn : bmBtns)
        {
            btn->setBounds (bx, bmBar.getY() + 4, btnW, bmBar.getHeight() - 8);
            bx += btnW + gap;
        }
        addBmBtn.setBounds (bmBar.getRight() - addW - 2,
                            bmBar.getY() + 4, addW, bmBar.getHeight() - 8);
    }

    // -- Preview bar at the bottom if visible
    if (previewVisible)
    {
        auto bar = bounds.removeFromBottom (kBarH);

        playStopBtn.setBounds (bar.removeFromLeft (kBarH).reduced (4));
        volumeSlider.setBounds (bar.removeFromRight (90).reduced (4, 8));
        fileNameLabel.setBounds (bar.reduced (6, 4));
    }

    // Browser fills remaining space. We give it ~28px extra height below
    // the visible area so JUCE's internal filename text bar is clipped out.
    browser.setBounds (bounds.withBottom (bounds.getBottom() + 28));
}

void FileBrowserPanel::paint (juce::Graphics& g)
{
    // ── Bookmark bar background ────────────────────────────────────────────
    {
        const auto& T = getTheme();
        auto bmRect = getLocalBounds().removeFromTop (kBmH).toFloat();
        juce::ColourGradient bmGrad (T.darkBar.darker (0.5f), 0, bmRect.getY(),
                                     T.darkBar.darker (0.3f), 0, bmRect.getBottom(), false);
        g.setGradientFill (bmGrad);
        g.fillRect (bmRect);
        g.setColour (T.accent.withAlpha (0.20f));
        g.drawLine (bmRect.getX(), bmRect.getBottom(),
                    bmRect.getRight(), bmRect.getBottom(), 1.0f);
    }

    // LCD frame
    const auto ac = getTheme().accent;
    auto b = getLocalBounds();
    b.removeFromTop (kBmH);   // skip bookmark bar

    juce::ColourGradient outerGrad (juce::Colour (0xFF131313), 0, 0,
                                    juce::Colour (0xFF0E0E0E), 0, (float) b.getHeight(), false);
    g.setGradientFill (outerGrad);
    g.fillRect (b);                                          // sharp corners

    g.setColour (ac.withAlpha (0.20f));
    g.drawRect (b.toFloat(), 1.0f);                          // sharp border

    auto screen = b.reduced (4);
    g.setColour (getTheme().darkBar.darker (0.55f));
    g.fillRect (screen);                                     // sharp inner screen

    g.setColour (juce::Colours::black.withAlpha (0.18f));
    for (int y = screen.getY(); y < screen.getBottom(); y += 2)
        g.drawHorizontalLine (y, (float) screen.getX(), (float) screen.getRight());

    juce::ColourGradient glow (ac.withAlpha (0.06f), 0, (float) screen.getY(),
                                juce::Colours::transparentBlack, 0, (float) (screen.getY() + 20), false);
    g.setGradientFill (glow);
    g.fillRect (screen);                                     // sharp glow

    g.setColour (ac.withAlpha (0.12f));
    g.drawRect (screen.expanded (0), 1.0f);                  // sharp inner border

    // Preview bar background when visible
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

// -- FileBrowserListener ----------------------------------------------------

void FileBrowserPanel::fileClicked (const juce::File& f, const juce::MouseEvent&)
{
    if (! f.existsAsFile()) return;

    const bool wasVisible = previewVisible;
    previewFile    = f;
    previewVisible = true;

    fileNameLabel.setText (f.getFileName(), juce::dontSendNotification);

    // Stop preview & update icon (don’t auto-play)
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
        processor.loadSoundFontAsync (f);
        if (onFileLoaded) onFileLoaded();
        return;
    }

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

// -- Preview engine ---------------------------------------------------------

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
        playStopBtn.setState (IconButton::Playing);
    else
        playStopBtn.setState (IconButton::Stopped);
}

void FileBrowserPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    juce::MessageManager::callAsync ([this]
    {
        if (! transport.isPlaying())
            updatePlayButton();
    });
}

void FileBrowserPanel::refreshTheme()
{
    smallLAF.refreshTheme();
    repaint();
    browser.repaint();
}

// ── Cloud bookmark detection ──────────────────────────────────────────────────

void FileBrowserPanel::detectCloudFolders()
{
    auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    // Fixed-path services (macOS / Windows)
    struct CloudDef { const char* name; const char* rel; };
    static const CloudDef defs[] =
    {
        { "iCloud",       "Library/Mobile Documents/com~apple~CloudDocs" },
        { "Dropbox",      "Dropbox"                                       },
        { "Dropbox",      "Dropbox (Personal)"                           },
        { "OneDrive",     "OneDrive"                                      },
        { "OneDrive",     "OneDrive - Personal"                           },
    };

    for (auto& d : defs)
    {
        auto f = home.getChildFile (d.rel);
        if (f.isDirectory())
        {
            bool dupe = false;
            for (auto& b : bookmarks)
                if (b.path == f) { dupe = true; break; }
            if (! dupe)
                bookmarks.add ({ d.name, f, false });
        }
    }

    // macOS ~/Library/CloudStorage — Google Drive, OneDrive 365, Box, etc.
    auto cs = home.getChildFile ("Library/CloudStorage");
    if (cs.isDirectory())
    {
        auto children = cs.findChildFiles (juce::File::findDirectories, false);
        for (auto& dir : children)
        {
            auto raw = dir.getFileName();
            juce::String display;
            if (raw.startsWithIgnoreCase ("GoogleDrive"))  display = "Google Drive";
            else if (raw.startsWithIgnoreCase ("OneDrive")) display = "OneDrive";
            else if (raw.startsWithIgnoreCase ("Dropbox"))  display = "Dropbox";
            else if (raw.startsWithIgnoreCase ("Box"))      display = "Box";
            else display = raw.upToFirstOccurrenceOf ("-", false, false);
            if (display.isEmpty()) display = raw;

            bool dupe = false;
            for (auto& b : bookmarks)
                if (b.path == dir) { dupe = true; break; }
            if (! dupe)
                bookmarks.add ({ display, dir, false });
        }
    }
}

// ── Bookmark persistence ──────────────────────────────────────────────────────

static juce::File getBookmarksFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("DYSEKT/bookmarks.txt");
}

void FileBrowserPanel::loadCustomBookmarks()
{
    auto f = getBookmarksFile();
    if (! f.existsAsFile()) return;

    juce::StringArray lines;
    f.readLines (lines);
    for (auto& line : lines)
    {
        juce::File dir (line.trim());
        if (dir.isDirectory())
        {
            bool dupe = false;
            for (auto& b : bookmarks)
                if (b.path == dir) { dupe = true; break; }
            if (! dupe)
                bookmarks.add ({ dir.getFileName(), dir, true });
        }
    }
}

void FileBrowserPanel::saveCustomBookmarks()
{
    auto f = getBookmarksFile();
    f.getParentDirectory().createDirectory();

    juce::StringArray lines;
    for (auto& b : bookmarks)
        if (b.removable)
            lines.add (b.path.getFullPathName());

    f.replaceWithText (lines.joinIntoString ("\n"));
}

// ── Bookmark bar rebuild ──────────────────────────────────────────────────────

void FileBrowserPanel::rebuildBookmarkBar()
{
    bmBtns.clear();
    const auto& T = getTheme();

    for (int i = 0; i < bookmarks.size(); ++i)
    {
        auto* btn = bmBtns.add (new RemovableButton());
        btn->setButtonText (bookmarks[i].name);
        btn->setTooltip    (bookmarks[i].path.getFullPathName());
        btn->setColour (juce::TextButton::buttonColourId,  T.darkBar.darker (0.3f));
        btn->setColour (juce::TextButton::buttonOnColourId,T.accent.withAlpha (0.2f));
        btn->setColour (juce::TextButton::textColourOffId, T.accent.withAlpha (0.85f));
        btn->setColour (juce::TextButton::textColourOnId,  T.accent);

        btn->onClick = [this, i] { browser.setRoot (bookmarks[i].path); };

        if (bookmarks[i].removable)
        {
            btn->onRightClick = [this, i]
            {
                juce::PopupMenu menu;
                menu.addItem (1, "Remove bookmark");
                menu.showMenuAsync (juce::PopupMenu::Options(),
                    [this, i] (int result)
                    {
                        if (result == 1)
                        {
                            bookmarks.remove (i);
                            saveCustomBookmarks();
                            rebuildBookmarkBar();
                            resized();
                            repaint();
                        }
                    });
            };
        }

        addAndMakeVisible (btn);
    }
}