#pragma once
#include <QMainWindow>
#include <QSlider>
#include <QDial>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTabWidget>
#include "AudioEngine.h"
#include "WaveformDisplay.h"

// Groups the 10 UI knob dials and spinboxes for a single wave channel
struct ChannelKnobs
{
    QDial *hpDial, *lpDial, *odDial, *distDial, *bitDial, *revDial, *delDial, *fbDial, *tRateDial, *tDepthDial, *volDial;
    QSpinBox *hpSpin, *lpSpin, *odSpin, *distSpin, *bitSpin, *revSpin, *delSpin, *fbSpin, *tRateSpin, *tDepthSpin, *volSpin;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

private:
    // Audio Generators & Separate Channel Effects (OOP: Composition & RAII)
    Oscillator *osc, *osc2;
    ChannelEffects chan1, chan2;

    // UI: Generator & Duration
    QComboBox *waveSelect, *waveSelect2;
    QSlider *freqSlider, *durationSlider;
    QSpinBox *freqSpinBox, *wave2ShiftSpin;
    QDoubleSpinBox *durationSpinBox;
    QLabel *freqLabel;

    // UI: Individual Effects Tabs
    QTabWidget *fxTabs;
    ChannelKnobs knobs1, knobs2;

    QCheckBox *autoPlayCheck;
    WaveformDisplay *visualizer;
    QLabel *statusLabel;

    // Audio Buffer
    float *currentBuffer;
    int currentSampleCount;

    void setupUI();
    QWidget *createChannelTab(const QString &title, ChannelKnobs &k, ChannelEffects &fx);
    void updateChannelEffects(ChannelKnobs &k, ChannelEffects &fx);
    void renderAudioBuffer();

private slots:
    void onParametersChanged();
    void onPlayClicked();
    void onStopClicked();
    void onSaveWavClicked();

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
};