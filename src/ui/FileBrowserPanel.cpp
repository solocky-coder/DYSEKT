#include "FileBrowserPanel.h"
#include "DysektLookAndFeel.h"
#include "../PluginProcessor.h"

// ── ArchiveListModel ─────────────────────────────────────────────────────────

int FileBrowserPanel::ArchiveListModel::getNumRows()
{
    return owner ? owner->archiveRows.size() : 0;
}

void FileBrowserPanel::ArchiveListModel::paintListBoxItem (int row, juce::Graphics& g,
                                                           int w, int h, bool selected)
{
    if (! owner) return;
    if (row < 0 || row >= owner->archiveRows.size()) return;

    const auto& R = owner->archiveRows[row];
    const auto& T = getTheme();

    if (selected)
    {
        g.setColour (T.accent.withAlpha (0.12f));
        g.fillAll();
    }

    g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));

    // Folder icon placeholder
    if (R.isFolder)
    {
        g.setColour (T.accent.withAlpha (0.6f));
        g.drawText (u8"\U0001F4C1", 4, 0, 18, h, juce::Justification::centredLeft);
        g.setColour (selected ? T.accent : T.foreground.withAlpha (0.8f));
        g.drawText (R.name, 24, 0, w - 28, h, juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour (selected ? T.accent : T.foreground.withAlpha (0.75f));
        g.drawText (R.name, 4, 0, w - 120, h, juce::Justification::centredLeft, true);

        // Format badge
        if (R.format.isNotEmpty())
        {
            auto badgeW = 40;
            auto badgeRect = juce::Rectangle<int> (w - badgeW - 60, (h - 13) / 2, badgeW, 13);
            g.setColour (T.accent.withAlpha (0.18f));
            g.fillRoundedRectangle (badgeRect.toFloat(), 2.0f);
            g.setColour (T.accent.withAlpha (0.85f));
            g.setFont (juce::Font (juce::FontOptions{}.withHeight (9.0f)));
            g.drawText (R.format, badgeRect, juce::Justification::centred);
        }

        // Size
        if (R.sizeBytes > 0)
        {
            g.setFont (juce::Font (juce::FontOptions{}.withHeight (9.5f)));
            g.setColour (T.foreground.withAlpha (0.4f));
            juce::String sizeStr;
            if (R.sizeBytes >= 1024 * 1024)
                sizeStr = juce::String (R.sizeBytes / (1024 * 1024)) + " MB";
            else
                sizeStr = juce::String (R.sizeBytes / 1024) + " KB";
            g.drawText (sizeStr, w - 58, 0, 55, h, juce::Justification::centredRight, true);
        }
    }
}

void FileBrowserPanel::ArchiveListModel::listBoxItemClicked (int /*row*/, const juce::MouseEvent&) {}

void FileBrowserPanel::ArchiveListModel::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (! owner) return;
    if (row < 0 || row >= owner->archiveRows.size()) return;

    const auto& R = owner->archiveRows[row];

    if (R.isFolder)
    {
        owner->showCollectionItem (R.folderId);
    }
    else
    {
        owner->loadArchiveFile (R);
    }
}

// ── Constructor / Destructor ─────────────────────────────────────────────────

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
    smallLAF.refreshTheme();
    addAndMakeVisible (browser);

    // ── Archive list view (initially hidden) ─────────────────────────────────
    archiveModel.owner = this;
    archiveList.setModel (&archiveModel);
    archiveList.setLookAndFeel (&smallLAF);
    archiveList.setRowHeight (18);
    archiveList.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    archiveList.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    addChildComponent (archiveList);

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

    // Hiding browser filename bar & making editors read-only
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

    // ── Local folder bookmarks ────────────────────────────────────────────────
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

    // ── Internet Archive bookmarks ────────────────────────────────────────────
    addArchiveBtn.setButtonText ("IA");
    addArchiveBtn.setTooltip    ("Add Internet Archive bookmark");
    addArchiveBtn.setColour (juce::TextButton::buttonColourId,  getTheme().darkBar.darker (0.5f));
    addArchiveBtn.setColour (juce::TextButton::textColourOffId, getTheme().accent.withAlpha (0.90f));
    addArchiveBtn.onClick = [this] { showArchiveUrlDialog(); };
    addAndMakeVisible (addArchiveBtn);

    loadArchiveBookmarks();
    rebuildArchiveButtons();
}

FileBrowserPanel::~FileBrowserPanel()
{
    stopTimer();
    browser.setLookAndFeel (nullptr);
    archiveList.setLookAndFeel (nullptr);
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

// ── Timer (spinner) ──────────────────────────────────────────────────────────

void FileBrowserPanel::timerCallback()
{
    ++spinnerFrame;
    rebuildArchiveButtons();
    resized();
}

// ── Layout ───────────────────────────────────────────────────────────────────

void FileBrowserPanel::resized()
{
    // All layout happens inside the inner screen area (4 px inset on all sides).
    auto inner = getLocalBounds().reduced (4);

    // ── Preview bar at the BOTTOM of the inner area ───────────────────────────
    if (previewVisible)
    {
        auto bar = inner.removeFromBottom (kBarH);
        playStopBtn  .setBounds (bar.removeFromLeft (kBarH).reduced (4));
        volumeSlider .setBounds (bar.removeFromRight (90).reduced (4, 8));
        fileNameLabel.setBounds (bar.reduced (6, 4));
    }

    // ── Bookmark bar row 1: local folders ────────────────────────────────────
    {
        auto bmBar = inner.removeFromTop (kBmH);
        const int addW  = 22;
        const int gap   = 3;
        const int n     = bmBtns.size();
        const int avail = bmBar.getWidth() - addW - gap - 4;
        const int btnW  = n > 0 ? juce::jmin (90, juce::jmax (40, (avail - gap * (n - 1)) / n)) : 0;

        int bx = bmBar.getX() + 2;
        for (auto* btn : bmBtns)
        {
            btn->setBounds (bx, bmBar.getY() + 4, btnW, bmBar.getHeight() - 8);
            bx += btnW + gap;
        }
        addBmBtn.setBounds (bmBar.getRight() - addW - 2,
                            bmBar.getY() + 4, addW, bmBar.getHeight() - 8);
    }

    // ── Bookmark bar row 2: archive bookmarks ─────────────────────────────────
    {
        auto archBar = inner.removeFromTop (kBmH);
        const int addW  = 22;
        const int gap   = 3;
        const int n     = archiveBtns.size();
        const int avail = archBar.getWidth() - addW - gap - 4;
        const int btnW  = n > 0 ? juce::jmin (90, juce::jmax (40, (avail - gap * (n - 1)) / n)) : 0;

        int bx = archBar.getX() + 2;
        for (auto* btn : archiveBtns)
        {
            btn->setBounds (bx, archBar.getY() + 4, btnW, archBar.getHeight() - 8);
            bx += btnW + gap;
        }
        addArchiveBtn.setBounds (archBar.getRight() - addW - 2,
                                 archBar.getY() + 4, addW, archBar.getHeight() - 8);
    }

    // ── Main content area: archive list or local browser ──────────────────────
    if (archiveViewActive)
    {
        archiveList.setVisible (true);
        browser.setVisible (false);
        archiveList.setBounds (inner);
    }
    else
    {
        archiveList.setVisible (false);
        browser.setVisible (true);
        browser.setBounds (inner);
    }
}

void FileBrowserPanel::paint (juce::Graphics& g)
{
    // Paint the LCD frame over the FULL component bounds (including the preview
    // bar).  The preview bar is drawn inside the frame, not outside it.
    const auto& T  = getTheme();
    const auto  ac = T.accent;
    const auto  b  = getLocalBounds();

    // ── Outer rounded shell ───────────────────────────────────────────────────
    juce::ColourGradient outerGrad (juce::Colour (0xFF131313), 0, 0,
                                    juce::Colour (0xFF0E0E0E), 0, (float) b.getHeight(), false);
    g.setGradientFill (outerGrad);
    g.fillRoundedRectangle (b.toFloat(), 4.0f);

    g.setColour (ac.withAlpha (0.65f));
    g.drawRoundedRectangle (b.toFloat().reduced (0.5f), 4.0f, 1.0f);

    // ── Inner screen area (inset 4 px all around) ────────────────────────────
    const auto screen = b.reduced (4);

    g.setColour (T.darkBar.darker (0.55f));
    g.fillRoundedRectangle (screen.toFloat(), 2.0f);

    // Scanlines
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (screen);
        for (int y = screen.getY(); y < screen.getBottom(); y += 2)
            g.drawHorizontalLine (y, (float) screen.getX(), (float) screen.getRight());
    }

    // Top glow
    juce::ColourGradient glow (ac.withAlpha (0.06f), 0, (float) screen.getY(),
                                juce::Colours::transparentBlack, 0,
                                (float)(screen.getY() + 20), false);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (screen.toFloat(), 2.0f);

    g.setColour (ac.withAlpha (0.12f));
    g.drawRoundedRectangle (screen.toFloat().expanded (0.5f), 2.0f, 1.0f);

    // ── Preview bar (bottom, inside frame) ────────────────────────────────────
    if (previewVisible)
    {
        const auto bar = screen.removeFromBottom (kBarH);     // visual only — doesn't mutate screen
        // Use a local copy so we can removeFromBottom without side-effects
        auto screenCopy = screen;
        const auto barRect = b.reduced (4).withTop (b.getBottom() - 4 - kBarH);

        g.setColour (T.darkBar.darker (0.6f));
        g.fillRect (barRect);
        g.setColour (ac.withAlpha (0.25f));
        g.drawLine ((float) barRect.getX(), (float) barRect.getY(),
                    (float) barRect.getRight(), (float) barRect.getY(), 1.0f);
    }

    // ── Bookmark bar row 1 background ────────────────────────────────────────
    {
        const auto bmRect = b.reduced (4).withHeight (kBmH).toFloat();
        juce::ColourGradient bmGrad (T.darkBar.darker (0.5f), 0, bmRect.getY(),
                                     T.darkBar.darker (0.3f), 0, bmRect.getBottom(), false);
        g.setGradientFill (bmGrad);
        g.fillRect (bmRect);
        g.setColour (T.accent.withAlpha (0.20f));
        g.drawLine (bmRect.getX(), bmRect.getBottom(),
                    bmRect.getRight(), bmRect.getBottom(), 1.0f);
    }

    // ── Bookmark bar row 2 background (archive) ───────────────────────────────
    {
        const auto archRect = b.reduced (4).withTop (b.getY() + 4 + kBmH)
                                           .withHeight (kBmH).toFloat();
        juce::ColourGradient bmGrad (T.darkBar.darker (0.45f), 0, archRect.getY(),
                                     T.darkBar.darker (0.25f), 0, archRect.getBottom(), false);
        g.setGradientFill (bmGrad);
        g.fillRect (archRect);
        g.setColour (T.accent.withAlpha (0.15f));
        g.drawLine (archRect.getX(), archRect.getBottom(),
                    archRect.getRight(), archRect.getBottom(), 1.0f);

        if (archiveViewActive && archiveListTitle.isNotEmpty())
        {
            g.setColour (T.accent.withAlpha (0.6f));
            g.setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f)));
            g.drawText (archiveListTitle, archRect.reduced (4, 0).toNearestInt(),
                        juce::Justification::centredRight);
        }
    }
}

// ── FileBrowserListener ──────────────────────────────────────────────────────

void FileBrowserPanel::fileClicked (const juce::File& f, const juce::MouseEvent&)
{
    if (! f.existsAsFile()) return;

    const bool wasVisible = previewVisible;
    previewFile    = f;
    previewVisible = true;

    fileNameLabel.setText (f.getFileName(), juce::dontSendNotification);
    startPreview (f);

    playStopBtn.setVisible   (true);
    volumeSlider.setVisible  (true);
    fileNameLabel.setVisible (true);

    if (! wasVisible) resized();
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

// ── Preview engine ───────────────────────────────────────────────────────────

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

// ── Local bookmark persistence ────────────────────────────────────────────────

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

        btn->onClick = [this, i]
        {
            exitArchiveView();
            browser.setRoot (bookmarks[i].path);
        };

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

// ── Internet Archive bookmark persistence ─────────────────────────────────────

juce::File FileBrowserPanel::getArchiveBookmarksFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("DYSEKT/archive_bookmarks.txt");
}

void FileBrowserPanel::loadArchiveBookmarks()
{
    auto f = getArchiveBookmarksFile();
    if (! f.existsAsFile()) return;

    juce::StringArray lines;
    f.readLines (lines);

    for (auto& line : lines)
    {
        auto url = line.trim();
        if (url.isEmpty()) continue;
        if (! ArchiveIntegration::isValidArchiveUrl (url)) continue;

        bool dupe = false;
        for (auto& b : archiveBookmarks)
            if (b.url == url) { dupe = true; break; }
        if (dupe) continue;

        // Add as pending while we re-resolve the title
        ArchiveBookmark bm;
        bm.url     = url;
        bm.title   = ArchiveIntegration::identifierFromUrl (url);
        bm.pending = true;
        archiveBookmarks.add (bm);

        // Fire resolve (captures index)
        int idx = archiveBookmarks.size() - 1;
        ArchiveIntegration::fetchItem (url, [this, idx] (bool ok, ArchiveIntegration::Item item)
        {
            if (idx >= archiveBookmarks.size()) return;
            auto& bm = archiveBookmarks.getReference (idx);
            bm.pending      = false;
            bm.isCollection = item.isCollection;
            if (ok && item.title.isNotEmpty())
                bm.title = item.title;

            // Stop spinner if no more pending
            bool anyPending = false;
            for (auto& b : archiveBookmarks)
                if (b.pending) { anyPending = true; break; }
            if (! anyPending) stopTimer();

            rebuildArchiveButtons();
            resized();
            repaint();
        });
    }

    if (! archiveBookmarks.isEmpty())
    {
        bool anyPending = false;
        for (auto& b : archiveBookmarks)
            if (b.pending) { anyPending = true; break; }
        if (anyPending)
            startTimer (400);
    }
}

void FileBrowserPanel::saveArchiveBookmarks()
{
    auto f = getArchiveBookmarksFile();
    f.getParentDirectory().createDirectory();

    juce::StringArray lines;
    for (auto& b : archiveBookmarks)
        if (! b.pending)
            lines.add (b.url);

    f.replaceWithText (lines.joinIntoString ("\n"));
}

// ── Archive bookmark UI ───────────────────────────────────────────────────────

static const juce::String kSpinnerFrames[] = { "|", "/", "-", "\\" };

void FileBrowserPanel::rebuildArchiveButtons()
{
    archiveBtns.clear();
    const auto& T = getTheme();

    for (int i = 0; i < archiveBookmarks.size(); ++i)
    {
        const auto& bm = archiveBookmarks[i];
        auto* btn = archiveBtns.add (new RemovableButton());

        if (bm.pending)
        {
            // Show spinner
            btn->setButtonText (kSpinnerFrames[spinnerFrame % 4]);
            btn->setTooltip ("Resolving: " + bm.url);
            btn->setEnabled (false);
        }
        else
        {
            btn->setButtonText (bm.title);
            btn->setTooltip    (bm.url);
            btn->setEnabled (true);
        }

        btn->setColour (juce::TextButton::buttonColourId,   T.darkBar.darker (0.3f));
        btn->setColour (juce::TextButton::buttonOnColourId,  T.accent.withAlpha (0.2f));
        btn->setColour (juce::TextButton::textColourOffId,   T.accent.withAlpha (bm.pending ? 0.4f : 0.85f));
        btn->setColour (juce::TextButton::textColourOnId,    T.accent);

        if (! bm.pending)
        {
            btn->onClick = [this, i] { showArchiveItem (i); };

            btn->onRightClick = [this, i]
            {
                juce::PopupMenu menu;
                menu.addItem (1, "Remove Archive bookmark");
                menu.addSeparator();

                auto cacheBytes = ArchiveIntegration::getCacheSize();
                juce::String cacheLabel = "Clear download cache";
                if (cacheBytes > 0)
                    cacheLabel += " (" + juce::String (cacheBytes / (1024 * 1024)) + " MB)";
                menu.addItem (2, cacheLabel, cacheBytes > 0);

                menu.showMenuAsync (juce::PopupMenu::Options(),
                    [this, i] (int result)
                    {
                        if (result == 1)
                        {
                            if (activeArchiveIndex == i) exitArchiveView();
                            archiveBookmarks.remove (i);
                            saveArchiveBookmarks();
                            rebuildArchiveButtons();
                            resized();
                            repaint();
                        }
                        else if (result == 2)
                        {
                            ArchiveIntegration::clearCache();
                        }
                    });
            };
        }

        addAndMakeVisible (btn);
    }
}

void FileBrowserPanel::showArchiveUrlDialog()
{
    auto* dlg = new juce::AlertWindow ("Add Internet Archive URL",
                                       "Paste an archive.org URL or bare identifier:",
                                       juce::MessageBoxIconType::NoIcon);
    dlg->addTextEditor ("url", "", "URL:");
    dlg->addButton ("Add", 1, juce::KeyPress (juce::KeyPress::returnKey));
    dlg->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    dlg->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, dlg] (int result)
        {
            if (result != 1) { delete dlg; return; }

            auto url = dlg->getTextEditorContents ("url").trim();
            delete dlg;

            if (url.isEmpty()) return;

            if (! ArchiveIntegration::isValidArchiveUrl (url))
            {
                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon,
                    "Invalid URL",
                    "That doesn't look like a valid archive.org URL or identifier.\n\n"
                    "Expected formats:\n"
                    "  https://archive.org/details/IDENTIFIER\n"
                    "  IDENTIFIER",
                    "OK");
                return;
            }

            // Check for duplicate
            for (auto& b : archiveBookmarks)
                if (b.url == url) return;

            resolveAndAddArchiveBookmark (url);
        }),
        true);
}

void FileBrowserPanel::resolveAndAddArchiveBookmark (const juce::String& url)
{
    ArchiveBookmark bm;
    bm.url     = url;
    bm.title   = ArchiveIntegration::identifierFromUrl (url);
    bm.pending = true;
    archiveBookmarks.add (bm);

    int idx = archiveBookmarks.size() - 1;

    rebuildArchiveButtons();
    resized();

    if (! isTimerRunning())
        startTimer (400);

    ArchiveIntegration::fetchItem (url, [this, idx, url] (bool ok, ArchiveIntegration::Item item)
    {
        if (idx >= archiveBookmarks.size()) return;
        auto& bm = archiveBookmarks.getReference (idx);
        bm.pending      = false;
        bm.isCollection = item.isCollection;

        if (! ok || (item.audioFiles.isEmpty() && ! item.isCollection))
        {
            // No usable content
            archiveBookmarks.remove (idx);
            rebuildArchiveButtons();
            resized();

            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::InfoIcon,
                "No Audio Found",
                "No supported audio files were found at that URL.\n\n"
                "Only WAV, FLAC, MP3, OGG, and AIFF files are supported.",
                "OK");
        }
        else
        {
            if (ok && item.title.isNotEmpty())
                bm.title = item.title;

            saveArchiveBookmarks();
            rebuildArchiveButtons();
            resized();
        }

        bool anyPending = false;
        for (auto& b : archiveBookmarks)
            if (b.pending) { anyPending = true; break; }
        if (! anyPending) stopTimer();

        repaint();
    });
}

// ── Archive list view ─────────────────────────────────────────────────────────

void FileBrowserPanel::showArchiveItem (int bookmarkIndex)
{
    if (bookmarkIndex < 0 || bookmarkIndex >= archiveBookmarks.size()) return;

    const auto& bm = archiveBookmarks[bookmarkIndex];
    activeArchiveIndex = bookmarkIndex;

    if (bm.isCollection)
    {
        archiveViewActive  = true;
        archiveRows.clear();
        archiveListTitle   = "Loading collection\u2026";
        archiveList.updateContent();
        resized();
        repaint();

        ArchiveIntegration::fetchCollection (
            ArchiveIntegration::identifierFromUrl (bm.url),
            [this] (bool ok, juce::Array<ArchiveIntegration::CollectionEntry> entries)
            {
                archiveRows.clear();
                if (ok)
                {
                    archiveListTitle = juce::String (entries.size()) + " items";
                    for (auto& e : entries)
                    {
                        ArchiveRow r;
                        r.name     = e.title.isNotEmpty() ? e.title : e.identifier;
                        r.isFolder = true;
                        r.folderId = e.identifier;
                        archiveRows.add (r);
                    }
                }
                else
                {
                    archiveListTitle = "Failed to load collection";
                }
                archiveList.updateContent();
                resized();
                repaint();
            });
    }
    else
    {
        archiveViewActive  = true;
        archiveRows.clear();
        archiveListTitle   = "Loading\u2026";
        archiveList.updateContent();
        resized();
        repaint();

        ArchiveIntegration::fetchItem (bm.url,
            [this] (bool ok, ArchiveIntegration::Item item)
            {
                archiveRows.clear();
                if (ok)
                {
                    archiveListTitle = item.title;
                    for (auto& af : item.audioFiles)
                    {
                        ArchiveRow r;
                        r.name        = af.name;
                        r.format      = af.format;
                        r.downloadUrl = af.downloadUrl;
                        r.sizeBytes   = af.sizeBytes;
                        r.isFolder    = false;
                        archiveRows.add (r);
                    }
                }
                else
                {
                    archiveListTitle = "Failed to load";
                }
                archiveList.updateContent();
                resized();
                repaint();
            });
    }
}

void FileBrowserPanel::showCollectionItem (const juce::String& collectionId)
{
    archiveViewActive  = true;
    archiveRows.clear();
    archiveListTitle   = "Loading " + collectionId + "\u2026";
    archiveList.updateContent();
    resized();
    repaint();

    ArchiveIntegration::fetchItem (collectionId,
        [this] (bool ok, ArchiveIntegration::Item item)
        {
            archiveRows.clear();
            if (ok)
            {
                archiveListTitle = item.title;
                for (auto& af : item.audioFiles)
                {
                    ArchiveRow r;
                    r.name        = af.name;
                    r.format      = af.format;
                    r.downloadUrl = af.downloadUrl;
                    r.sizeBytes   = af.sizeBytes;
                    r.isFolder    = false;
                    archiveRows.add (r);
                }
            }
            else
            {
                archiveListTitle = "Failed to load";
            }
            archiveList.updateContent();
            resized();
            repaint();
        });
}

void FileBrowserPanel::loadArchiveFile (const ArchiveRow& row)
{
    if (row.downloadUrl.isEmpty()) return;

    fileNameLabel.setText ("Downloading: " + row.name, juce::dontSendNotification);
    fileNameLabel.setVisible (true);
    previewVisible = true;
    playStopBtn.setVisible  (false);
    volumeSlider.setVisible (false);
    resized();
    repaint();

    ArchiveIntegration::downloadFile (row.downloadUrl,
        [this] (bool ok, juce::File localFile)
        {
            if (ok)
            {
                fileNameLabel.setText (localFile.getFileName(), juce::dontSendNotification);
                previewFile    = localFile;
                previewVisible = true;
                playStopBtn.setVisible  (true);
                volumeSlider.setVisible (true);
                stopPreview();
                updatePlayButton();

                if (onLoadRequest)
                    onLoadRequest (localFile);
                else
                    processor.loadFileAsync (localFile);

                if (onFileLoaded) onFileLoaded();
            }
            else
            {
                fileNameLabel.setText ("Download failed \u2014 check connection",
                                       juce::dontSendNotification);
            }
            resized();
            repaint();
        });
}

void FileBrowserPanel::exitArchiveView()
{
    archiveViewActive  = false;
    activeArchiveIndex = -1;
    archiveRows.clear();
    archiveListTitle.clear();
    archiveList.updateContent();
    resized();
    repaint();
}
