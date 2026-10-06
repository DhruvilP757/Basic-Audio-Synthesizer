#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QListView>
#include <cmath>

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
    // Audio Generators
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
QWidget *MainWindow::createChannelTab(const QString &title, ChannelKnobs &k, ChannelEffects &fx)
{
    QWidget *tab = new QWidget();
    QGridLayout *grid = new QGridLayout(tab);

    auto addKnob = [&](const QString &name, QDial *&d, QSpinBox *&s, int min, int max, int val, const QString &unit, int r, int c) {
        QWidget *w = new QWidget();
        QVBoxLayout *l = new QVBoxLayout(w);
        l->setContentsMargins(2, 2, 2, 2); l->setSpacing(2);

        QLabel *lbl = new QLabel(name);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("font-weight: bold; font-size: 11px; color: #58a6ff;");

        d = new QDial(); d->setRange(min, max); d->setValue(val); d->setNotchesVisible(true);
        s = new QSpinBox(); s->setRange(min, max); s->setValue(val); s->setSuffix(unit);
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
    addKnob("High-Pass", k.hpDial, k.hpSpin, 20, 2000, 20, " Hz", 0, 0);
    addKnob("Low-Pass", k.lpDial, k.lpSpin, 200, 20000, 20000, " Hz", 0, 1);
    addKnob("Overdrive", k.odDial, k.odSpin, 0, 100, 0, " %", 0, 2);
    addKnob("Distortion", k.distDial, k.distSpin, 5, 100, 100, " %", 0, 3);
    addKnob("Bitcrush", k.bitDial, k.bitSpin, 2, 16, 16, " bit", 0, 4);

    QPushButton *resetBtn = new QPushButton("Reset " + title + "\nEffects");
    resetBtn->setStyleSheet("background: #30363d; border: 1px solid #484f58; color: #f0f6fc; font-weight: bold; padding: 6px;");
    grid->addWidget(resetBtn, 0, 5);

    // Row 1: Space, Modulation & Volume
    addKnob("Reverb", k.revDial, k.revSpin, 0, 90, 0, " %", 1, 0);
    addKnob("Echo Time", k.delDial, k.delSpin, 20, 500, 250, " ms", 1, 1);
    addKnob("Echo Repeat", k.fbDial, k.fbSpin, 0, 80, 0, " %", 1, 2);
    addKnob("Tremolo Rate", k.tRateDial, k.tRateSpin, 1, 20, 5, " Hz", 1, 3);
    addKnob("Tremolo Depth", k.tDepthDial, k.tDepthSpin, 0, 100, 0, " %", 1, 4);
    addKnob("Master Vol", k.volDial, k.volSpin, 0, 100, 80, " %", 1, 5);

    connect(resetBtn, &QPushButton::clicked, [&k, this]() {
        k.hpDial->setValue(20); k.lpDial->setValue(20000); k.odDial->setValue(0);
        k.distDial->setValue(100); k.bitDial->setValue(16); k.revDial->setValue(0);
        k.delDial->setValue(250); k.fbDial->setValue(0); k.tRateDial->setValue(5);
        k.tDepthDial->setValue(0); k.volDial->setValue(80);
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

    auto styleCombo = [](QComboBox *c) {
        c->setView(new QListView(c));
        c->setStyleSheet("QComboBox { padding: 4px; border: 1px solid #3c485c; border-radius: 4px; background: #272f3d; color: #f0f6fc; }"
                         "QComboBox QAbstractItemView { background: #202632; color: #f0f6fc; selection-background-color: #1f6feb; }");
    };

    waveSelect = new QComboBox();
    styleCombo(waveSelect);
    waveSelect->addItems({"Sine Wave", "Square Wave", "Sawtooth Wave", "Triangle Wave", "Pulse Wave (25%)", "White Noise"});

    waveSelect2 = new QComboBox();
    styleCombo(waveSelect2);
    waveSelect2->addItems({"None (Single Wave)", "Sine Wave", "Square Wave", "Sawtooth Wave", "Triangle Wave", "Pulse Wave (25%)", "White Noise"});

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

    durationSlider = new QSlider(Qt::Horizontal);
    durationSlider->setRange(5, 50); // 0.5s - 5.0s
    durationSlider->setValue(20);

    durationSpinBox = new QDoubleSpinBox();
    durationSpinBox->setRange(0.5, 5.0);
    durationSpinBox->setSingleStep(0.1);
    durationSpinBox->setValue(2.0);
    durationSpinBox->setSuffix(" s");

    // Layout Row 0: Waveforms & Shift
    genLayout->addWidget(new QLabel("Wave 1:"), 0, 0);
    genLayout->addWidget(waveSelect, 0, 1);
    genLayout->addWidget(new QLabel("Wave 2:"), 0, 2);
    genLayout->addWidget(waveSelect2, 0, 3);
    genLayout->addWidget(new QLabel("Shift:"), 0, 4);
    genLayout->addWidget(wave2ShiftSpin, 0, 5);

    // Layout Row 1: Pitch & Duration
    genLayout->addWidget(new QLabel("Pitch:"), 1, 0);
    genLayout->addWidget(freqSlider, 1, 1);
    genLayout->addWidget(freqSpinBox, 1, 2);
    genLayout->addWidget(freqLabel, 1, 3);
    genLayout->addWidget(new QLabel("Duration:"), 1, 4);

    QHBoxLayout *durBox = new QHBoxLayout();
    durBox->addWidget(durationSlider);
    durBox->addWidget(durationSpinBox);
    genLayout->addLayout(durBox, 1, 5);

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

    fxTabs->addTab(createChannelTab("Wave 1", knobs1, chan1), "Wave 1 Effects (Primary)");
    fxTabs->addTab(createChannelTab("Wave 2", knobs2, chan2), "Wave 2 Effects (Superimposed)");
    fxLayout->addWidget(fxTabs);

    mainLayout->addWidget(fxBox);

    // 3. Waveform Visualizer
    visualizer = new WaveformDisplay(this);
    mainLayout->addWidget(new QLabel("Real-Time Waveform Monitor (First 1,000 Samples):"));
    mainLayout->addWidget(visualizer);

    // 4. Playback & Export Controls
    QHBoxLayout *actionLayout = new QHBoxLayout();
    QPushButton *playBtn = new QPushButton("Play Sound"), *stopBtn = new QPushButton("Stop"), *saveBtn = new QPushButton("Save Lossless .WAV File...");
    autoPlayCheck = new QCheckBox("Live Hearing (Auto-play on change)");

    actionLayout->addWidget(playBtn);
    actionLayout->addWidget(stopBtn);
    actionLayout->addWidget(autoPlayCheck);
    actionLayout->addStretch();
    actionLayout->addWidget(saveBtn);
    mainLayout->addLayout(actionLayout);

    statusLabel = new QLabel("Ready.");
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

    connect(durationSlider, &QSlider::valueChanged, [this](int v) {
        durationSpinBox->blockSignals(true); durationSpinBox->setValue(v / 10.0); durationSpinBox->blockSignals(false);
        onParametersChanged();
    });
    connect(durationSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double v) {
        durationSlider->blockSignals(true); durationSlider->setValue((int)(v * 10.0)); durationSlider->blockSignals(false);
        onParametersChanged();
    });

    connect(playBtn, &QPushButton::clicked, this, &MainWindow::onPlayClicked);
    connect(stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::onSaveWavClicked);
}

void MainWindow::updateChannelEffects(ChannelKnobs &k, ChannelEffects &fx)
{
    fx.highPass->setCutoff((float)k.hpDial->value());
    fx.lowPass->setCutoff((float)k.lpDial->value());
    fx.overdrive->setDrive(k.odDial->value() / 100.0f);
    fx.dist->setThreshold(k.distDial->value() / 100.0f);
    fx.bitcrush->setBitDepth(k.bitDial->value());
    fx.reverb->setRoomSize(k.revDial->value() / 100.0f);
    fx.echo->setDelayTime(k.delDial->value() / 1000.0f);
    fx.echo->setFeedback(k.fbDial->value() / 100.0f);
    fx.tremolo->setRate((float)k.tRateDial->value());
    fx.tremolo->setDepth(k.tDepthDial->value() / 100.0f);
    fx.gain->setVolume(k.volDial->value() / 100.0f);
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
        float f2 = freq * std::pow(2.0f, wave2ShiftSpin->value() / 12.0f);
        osc2->setFrequency(f2);
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
    float duration = durationSlider->value() / 10.0f;
    currentSampleCount = static_cast<int>(SAMPLE_RATE * duration);

    delete[] currentBuffer;
    currentBuffer = new float[currentSampleCount];

    osc->reset();
    osc2->reset();
    chan1.reset();
    chan2.reset();

    bool superimpose = (waveSelect2->currentIndex() > 0);

    for (int i = 0; i < currentSampleCount; ++i)
    {
        // Each wave is processed through its own individual effects pipeline
        float s1 = chan1.process(osc->process(0.0f));

        if (superimpose)
        {
            float s2 = chan2.process(osc2->process(0.0f));
            // Superimpose both effected waves: y = (y1 + y2) / 2
            currentBuffer[i] = 0.5f * (s1 + s2);
        }
        else
        {
            currentBuffer[i] = s1;
        }
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
        statusLabel->setText("Playing audio live...");
    }
}

void MainWindow::onStopClicked()
{
#ifdef _WIN32
    PlaySoundW(NULL, NULL, 0);
#endif
    statusLabel->setText("Audio stopped.");
}

void MainWindow::onSaveWavClicked()
{
    QString path = QFileDialog::getSaveFileName(this, "Export Lossless WAV", "synthesizer_output.wav", "WAV Audio (*.wav)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".wav", Qt::CaseInsensitive)) path += ".wav";

    if (WavWriter::save(path.toStdString(), currentBuffer, currentSampleCount))
    {
        statusLabel->setText("Saved successfully: " + path);
        QMessageBox::information(this, "Export Success", "Lossless WAV file successfully saved to:\n" + path);
    }
    else QMessageBox::critical(this, "Error", "Could not save WAV file.");
}