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
#include "AudioEngine.h"
#include "WaveformDisplay.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

private:
    // Audio Engine Nodes (OOP: Dynamic Memory & Polymorphism)
    Oscillator *osc, *osc2;
    HighPassFilter *highPass, *hp2;
    LowPassFilter *lowPass, *lp2;
    Overdrive *overdrive, *od2;
    Distortion *dist, *dist2;
    Bitcrusher *bitcrush, *bit2;
    Tremolo *tremolo, *trem2;
    Echo *echo, *echo2;
    Reverb *reverb, *rev2;
    Gain *masterGain, *gain2;
    EffectsRack *rack, *rack2;

    // UI: Generator & Duration
    QComboBox *waveSelect, *waveSelect2;
    QSlider *freqSlider, *durationSlider;
    QSpinBox *freqSpinBox, *wave2ShiftSpin;
    QDoubleSpinBox *durationSpinBox;
    QLabel *freqLabel;

    // UI: Knobs with Synchronized Bottom SpinBoxes
    QComboBox *fxTargetSelect;
    QDial *hpDial, *lpDial, *odDial, *distDial, *bitDial, *revDial, *delDial, *fbDial, *tRateDial, *tDepthDial, *volDial;
    QSpinBox *hpSpin, *lpSpin, *odSpin, *distSpin, *bitSpin, *revSpin, *delSpin, *fbSpin, *tRateSpin, *tDepthSpin, *volSpin;

    QCheckBox *autoPlayCheck;
    WaveformDisplay *visualizer;
    QLabel *statusLabel;

    // Audio Buffer
    float *currentBuffer;
    int currentSampleCount;

    void setupUI();
    void renderAudioBuffer();

private slots:
    void onParametersChanged();
    void onPlayClicked();
    void onStopClicked();
    void onSaveWavClicked();
    void onResetEffectsClicked();

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
};