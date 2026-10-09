#pragma once
#include <QWidget>
#include <QPainter>
#include <vector>
#include <algorithm>

// Real-time audio waveform visualizer widget
class WaveformDisplay : public QWidget
{
    Q_OBJECT
private:
    std::vector<float> preview;

public:
    WaveformDisplay(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(110);
        setStyleSheet("background-color: #1a1e24; border: 1px solid #333d4b; border-radius: 4px;");
    }

    void updateWaveform(const float *buf, int count)
    {
        if (buf && count > 0) preview.assign(buf, buf + std::min(1000, count));
        else preview.clear();
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        int w = width(), h = height(), midY = h / 2;

        p.setPen(QPen(QColor(70, 85, 105), 1, Qt::DashLine));
        p.drawLine(0, midY, w, midY); // Center reference line

        if (preview.empty()) return;

        p.setPen(QPen(QColor(0, 220, 130), 2));
        int count = std::min(1000, (int)preview.size());
        float step = (float)count / (float)w;

        for (int x = 0; x < w - 1; ++x)
        {
            int i1 = (int)(x * step), i2 = (int)((x + 1) * step);
            if (i2 >= (int)preview.size()) break;
            p.drawLine(x, midY - (int)(preview[i1] * (h * 0.42f)), x + 1, midY - (int)(preview[i2] * (h * 0.42f)));
        }
    }
};