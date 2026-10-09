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

// Groups the 10 UI knob dials for a single wave channel
struct ChannelKnobs
{
    QDial *hp, *lp, *od, *dist, *bit, *rev, *del, *fb, *tRate, *tDepth, *vol;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

private:
    Oscillator *osc, *osc2;
    ChannelEffects chan1, chan2;

    QComboBox *waveSelect, *waveSelect2;
    QSlider *freqSlider;
    QSpinBox *freqSpinBox, *wave2ShiftSpin;
    QDoubleSpinBox *durationSpinBox;
    QLabel *freqLabel, *statusLabel;
    QTabWidget *fxTabs;
    ChannelKnobs knobs1, knobs2;
    QCheckBox *autoPlayCheck;
    WaveformDisplay *visualizer;

    float *currentBuffer;
    int currentSampleCount;

    void setupUI();
    QWidget *createChannelTab(const QString &title, ChannelKnobs &k);
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