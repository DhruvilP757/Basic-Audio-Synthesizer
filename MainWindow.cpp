#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QListView>
#include <QApplication>
#include <cmath>

const float LIVE_PREVIEW_DURATION = 3.0f; // Fixed 3 seconds live sound playback

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

// Helper: Calculate musical note name from frequency
static QString getNoteName(float freq)
{
    if (freq < 16.0f) return "Sub-bass";
    if (freq > 12500.0f) return "High Treble";
    const char *n[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int midi = static_cast<int>(std::round(69.0f + 12.0f * std::log2(freq / 440.0f)));
    return (midi < 0) ? "Sub-bass" : QString("%1%2").arg(n[(midi % 12 + 12) % 12]).arg((midi / 12) - 1);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), currentBuffer(nullptr), currentSampleCount(0)
{
    osc = new Oscillator(440.0f, WaveType::SINE);
    osc2 = new Oscillator(440.0f, WaveType::SINE);

    setupUI();
    setWindowTitle("Audio Synthesizer - OOP Project");
    resize(870, 740);

    onParametersChanged(); // Initial sound generation
}

MainWindow::~MainWindow()
{
    onStopClicked();
    delete osc;
    delete osc2;
    delete[] currentBuffer;
}

// Modular helper to create an individual channel's effect tab
QWidget *MainWindow::createChannelTab(const QString &title, ChannelKnobs &k)
{
    QWidget *tab = new QWidget();
    QGridLayout *grid = new QGridLayout(tab);

    auto addKnob = [&](const QString &name, QDial *&d, int min, int max, int val, const QString &unit, int r, int c) {
        QWidget *w = new QWidget();
        QVBoxLayout *l = new QVBoxLayout(w);
        l->setContentsMargins(2, 2, 2, 2); l->setSpacing(2);

        QLabel *lbl = new QLabel(name);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("font-weight: bold; font-size: 11px; color: #58a6ff;");

        d = new QDial(); d->setRange(min, max); d->setValue(val); d->setNotchesVisible(true);
        QSpinBox *s = new QSpinBox(); s->setRange(min, max); s->setValue(val); s->setSuffix(unit);
        s->setAlignment(Qt::AlignCenter); s->setButtonSymbols(QAbstractSpinBox::NoButtons);
        s->setStyleSheet("background: #272f3d; color: #f0f6fc; border: 1px solid #3c485c; border-radius: 3px; font-size: 11px; min-width: 55px;");

        l->addWidget(lbl);
        l->addWidget(d, 0, Qt::AlignCenter);
        l->addWidget(s, 0, Qt::AlignCenter);

        connect(d, &QDial::valueChanged, s, &QSpinBox::setValue);
        connect(s, QOverload<int>::of(&QSpinBox::valueChanged), d, &QDial::setValue);
        connect(d, &QDial::valueChanged, this, &MainWindow::onParametersChanged);

        grid->addWidget(w, r, c);
    };

    // Row 0: Tone & Saturation
    addKnob("High-Pass", k.hp, 20, 2000, 20, " Hz", 0, 0);
    addKnob("Low-Pass", k.lp, 200, 20000, 20000, " Hz", 0, 1);
    addKnob("Overdrive", k.od, 0, 100, 0, " %", 0, 2);
    addKnob("Distortion", k.dist, 5, 100, 100, " %", 0, 3);
    addKnob("Bitcrush", k.bit, 2, 16, 16, " bit", 0, 4);

    QPushButton *resetBtn = new QPushButton("Reset " + title + "\nEffects");
    resetBtn->setStyleSheet("background: #30363d; border: 1px solid #484f58; color: #f0f6fc; font-weight: bold; padding: 6px;");
    grid->addWidget(resetBtn, 0, 5);

    // Row 1: Space, Modulation & Volume
    addKnob("Reverb", k.rev, 0, 90, 0, " %", 1, 0);
    addKnob("Echo Time", k.del, 20, 500, 250, " ms", 1, 1);
    addKnob("Echo Repeat", k.fb, 0, 80, 0, " %", 1, 2);
    addKnob("Tremolo Rate", k.tRate, 1, 20, 5, " Hz", 1, 3);
    addKnob("Tremolo Depth", k.tDepth, 0, 100, 0, " %", 1, 4);
    addKnob("Master Vol", k.vol, 0, 100, 80, " %", 1, 5);

    connect(resetBtn, &QPushButton::clicked, [&k]() {
        k.hp->setValue(20); k.lp->setValue(20000); k.od->setValue(0);
        k.dist->setValue(100); k.bit->setValue(16); k.rev->setValue(0);
        k.del->setValue(250); k.fb->setValue(0); k.tRate->setValue(5);
        k.tDepth->setValue(0); k.vol->setValue(80);
    });

    return tab;
}

void MainWindow::setupUI()
{
    QWidget *central = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);

    // 1. Generator & Wave Superposition Box
    QGroupBox *genBox = new QGroupBox("1. Sound Generator && Wave Superposition");
    QGridLayout *genLayout = new QGridLayout(genBox);

    const QStringList waves = {"Sine Wave", "Square Wave", "Sawtooth Wave", "Triangle Wave", "Pulse Wave (25%)", "White Noise"};
    auto makeCombo = [&](bool allowNone) {
        QComboBox *c = new QComboBox();
        c->setView(new QListView(c));
        c->setStyleSheet("QComboBox { padding: 4px; border: 1px solid #3c485c; border-radius: 4px; background: #272f3d; color: #f0f6fc; }"
                         "QComboBox QAbstractItemView { background: #202632; color: #f0f6fc; selection-background-color: #1f6feb; }");
        if (allowNone) c->addItem("None (Single Wave)");
        c->addItems(waves);
        return c;
    };

    waveSelect = makeCombo(false);
    waveSelect2 = makeCombo(true);

    wave2ShiftSpin = new QSpinBox();
    wave2ShiftSpin->setRange(-24, 24);
    wave2ShiftSpin->setValue(0);
    wave2ShiftSpin->setSuffix(" st");
    wave2ShiftSpin->setToolTip("Wave 2 semitone pitch shift (0 = unison, 12 = octave up, -12 = octave down)");

    freqSlider = new QSlider(Qt::Horizontal);
    freqSlider->setRange(20, 20000); // Full human hearing range (20 Hz - 20,000 Hz)
    freqSlider->setValue(440);

    freqSpinBox = new QSpinBox();
    freqSpinBox->setRange(20, 20000);
    freqSpinBox->setValue(440);
    freqSpinBox->setSuffix(" Hz");

    freqLabel = new QLabel("(Note: A4)");
    freqLabel->setStyleSheet("color: #7ee787; font-weight: bold; min-width: 80px;");

    durationSpinBox = new QDoubleSpinBox();
    durationSpinBox->setRange(0.5, 600.0); // Time input in seconds up to 10 minutes (600s)
    durationSpinBox->setSingleStep(1.0);
    durationSpinBox->setValue(3.0);
    durationSpinBox->setDecimals(1);
    durationSpinBox->setSuffix(" s");
    durationSpinBox->setToolTip("Length of exported WAV file in seconds (0.5 s to 600.0 s / 10 minutes)");

    // Layout Row 0: Waveforms & Shift
    genLayout->addWidget(new QLabel("Wave 1:"), 0, 0);
    genLayout->addWidget(waveSelect, 0, 1);
    genLayout->addWidget(new QLabel("Wave 2:"), 0, 2);
    genLayout->addWidget(waveSelect2, 0, 3);
    genLayout->addWidget(new QLabel("Shift:"), 0, 4);
    genLayout->addWidget(wave2ShiftSpin, 0, 5);

    // Layout Row 1: Pitch & Export File Length
    genLayout->addWidget(new QLabel("Pitch:"), 1, 0);
    genLayout->addWidget(freqSlider, 1, 1);
    genLayout->addWidget(freqSpinBox, 1, 2);
    genLayout->addWidget(freqLabel, 1, 3);
    genLayout->addWidget(new QLabel("File Length:"), 1, 4);
    genLayout->addWidget(durationSpinBox, 1, 5);

    mainLayout->addWidget(genBox);

    // 2. Separate Channel Effects Tabs (Apply individual effects to Wave 1 and Wave 2)
    QGroupBox *fxBox = new QGroupBox("2. Individual Effects Processing (Per-Wave Channels)");
    QVBoxLayout *fxLayout = new QVBoxLayout(fxBox);

    fxTabs = new QTabWidget();
    fxTabs->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #3c485c; border-radius: 4px; background: #1e232d; }"
        "QTabBar::tab { background: #272f3d; color: #8b949e; padding: 6px 16px; font-weight: bold; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:selected { background: #1f6feb; color: #ffffff; }"
    );

    fxTabs->addTab(createChannelTab("Wave 1", knobs1), "Wave 1 Effects (Primary)");
    fxTabs->addTab(createChannelTab("Wave 2", knobs2), "Wave 2 Effects (Superimposed)");
    fxLayout->addWidget(fxTabs);

    mainLayout->addWidget(fxBox);

    // 3. Waveform Visualizer
    visualizer = new WaveformDisplay(this);
    mainLayout->addWidget(new QLabel("Real-Time Waveform Monitor (First 1,000 Samples):"));
    mainLayout->addWidget(visualizer);

    // 4. Playback & Export Controls
    QHBoxLayout *actionLayout = new QHBoxLayout();
    QPushButton *playBtn = new QPushButton("Play Sound (3s)"), *stopBtn = new QPushButton("Stop"), *saveBtn = new QPushButton("Export Studio Master .WAV...");
    autoPlayCheck = new QCheckBox("Live Hearing (Always On)");
    autoPlayCheck->setChecked(true); // Always on live play option enabled by default
    autoPlayCheck->setToolTip("Automatically play fixed 3-second live sound whenever any parameter is adjusted");

    actionLayout->addWidget(playBtn);
    actionLayout->addWidget(stopBtn);
    actionLayout->addWidget(autoPlayCheck);
    actionLayout->addStretch();
    actionLayout->addWidget(saveBtn);
    mainLayout->addLayout(actionLayout);

    statusLabel = new QLabel("Ready (Studio Master: 192 kHz, 32-bit Float).");
    statusLabel->setStyleSheet("color: #7ee787; font-weight: bold;");
    mainLayout->addWidget(statusLabel);

    setCentralWidget(central);

    // Signal Connections
    connect(waveSelect, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onParametersChanged);
    connect(waveSelect2, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onParametersChanged);
    connect(wave2ShiftSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onParametersChanged);

    connect(freqSlider, &QSlider::valueChanged, freqSpinBox, &QSpinBox::setValue);
    connect(freqSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), freqSlider, &QSlider::setValue);
    connect(freqSlider, &QSlider::valueChanged, this, &MainWindow::onParametersChanged);

    connect(durationSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double v) {
        statusLabel->setText(QString("File export length set to %1 s (Max 600 s / 10 min).").arg(v, 0, 'f', 1));
    });
    connect(autoPlayCheck, &QCheckBox::toggled, [this](bool checked) {
        if (!checked) onStopClicked();
        else onPlayClicked();
    });

    connect(playBtn, &QPushButton::clicked, this, &MainWindow::onPlayClicked);
    connect(stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::onSaveWavClicked);
}

void MainWindow::updateChannelEffects(ChannelKnobs &k, ChannelEffects &fx)
{
    fx.highPass->setCutoff((float)k.hp->value());
    fx.lowPass->setCutoff((float)k.lp->value());
    fx.overdrive->setDrive(k.od->value() / 100.0f);
    fx.dist->setThreshold(k.dist->value() / 100.0f);
    fx.bitcrush->setBitDepth(k.bit->value());
    fx.reverb->setRoomSize(k.rev->value() / 100.0f);
    fx.echo->setDelayTime(k.del->value() / 1000.0f);
    fx.echo->setFeedback(k.fb->value() / 100.0f);
    fx.tremolo->setRate((float)k.tRate->value());
    fx.tremolo->setDepth(k.tDepth->value() / 100.0f);
    fx.gain->setVolume(k.vol->value() / 100.0f);
}

void MainWindow::onParametersChanged()
{
    // 1. Primary & Superimposed Oscillators
    int freq = freqSlider->value();
    QString note = getNoteName((float)freq);
    freqLabel->setText(note.isEmpty() ? "" : QString("(Note: %1)").arg(note));

    osc->setFrequency((float)freq);
    osc->setWaveType(static_cast<WaveType>(waveSelect->currentIndex()));

    int w2 = waveSelect2->currentIndex();
    if (w2 > 0)
    {
        osc2->setWaveType(static_cast<WaveType>(w2 - 1));
        osc2->setFrequency(freq * std::pow(2.0f, wave2ShiftSpin->value() / 12.0f));
    }

    // 2. Update individual effect pipelines for both wave channels
    updateChannelEffects(knobs1, chan1);
    updateChannelEffects(knobs2, chan2);

    // 3. Render & Display
    renderAudioBuffer();
    visualizer->updateWaveform(currentBuffer, currentSampleCount);
    if (autoPlayCheck->isChecked()) onPlayClicked();
}

void MainWindow::renderAudioBuffer()
{
    // Fixed 3 seconds live preview audio buffer (576,000 samples @ 192 kHz)
    const int targetSampleCount = static_cast<int>(SAMPLE_RATE * LIVE_PREVIEW_DURATION);
    if (!currentBuffer || currentSampleCount != targetSampleCount)
    {
        delete[] currentBuffer;
        currentBuffer = new float[currentSampleCount = targetSampleCount];
    }

    osc->reset();
    osc2->reset();
    chan1.reset();
    chan2.reset();

    bool superimpose = (waveSelect2->currentIndex() > 0);
    for (int i = 0; i < currentSampleCount; ++i)
    {
        float s = chan1.process(osc->process(0.0f));
        if (superimpose) s = 0.5f * (s + chan2.process(osc2->process(0.0f)));
        currentBuffer[i] = s;
    }
}

void MainWindow::onPlayClicked()
{
    if (!currentBuffer || currentSampleCount <= 0) return;
    QString tempPath = QDir::temp().filePath("synth_preview.wav");
    if (WavWriter::save(tempPath.toStdString(), currentBuffer, currentSampleCount))
    {
#ifdef _WIN32
        PlaySoundW(reinterpret_cast<LPCWSTR>(tempPath.utf16()), NULL, SND_FILENAME | SND_ASYNC);
#endif
        statusLabel->setText("Playing 3s live preview (192 kHz, 32-bit Float)...");
    }
}

void MainWindow::onStopClicked()
{
#ifdef _WIN32
    PlaySoundW(NULL, NULL, 0);
#endif
    statusLabel->setText("Playback stopped.");
}

void MainWindow::onSaveWavClicked()
{
    QString path = QFileDialog::getSaveFileName(this, "Export Studio Master WAV", "synthesizer_192k_32bit.wav", "Studio Master WAV (*.wav);;All Files (*.*)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".wav", Qt::CaseInsensitive)) path += ".wav";

    float fileDuration = static_cast<float>(durationSpinBox->value());
    statusLabel->setText(QString("Exporting %1s Studio Master WAV file...").arg(fileDuration, 0, 'f', 1));
    QApplication::processEvents();

    bool superimpose = (waveSelect2->currentIndex() > 0);
    bool ok = WavWriter::saveStream(path.toStdString(), osc, osc2, chan1, chan2, superimpose, fileDuration);

    renderAudioBuffer();

    if (ok)
    {
        statusLabel->setText("Saved successfully: " + path);
        QMessageBox::information(this, "Export Success", QString("Studio Master Ultra Hi-Res WAV (%1 seconds, 192 kHz, 32-bit Float) successfully saved to:\n%2")
                                 .arg(fileDuration, 0, 'f', 1).arg(path));
    }
    else
    {
        QMessageBox::critical(this, "Error", "Could not save WAV file.");
    }
}