#pragma once
#include <iostream>
#include <fstream>
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cstdlib>

const int SAMPLE_RATE = 44100;
const float PI = 3.14159265f;

// 1. ABSTRACT BASE CLASS (OOP: Inheritance & Virtual Functions)
class AudioNode
{
public:
    virtual ~AudioNode() {}
    virtual float process(float input) = 0; // Pure virtual function
    virtual void reset() {}
};

// 2. OSCILLATOR (6 Waveforms)
enum class WaveType { SINE, SQUARE, SAWTOOTH, TRIANGLE, PULSE, NOISE };

class Oscillator : public AudioNode
{
private:
    float frequency, phase;
    WaveType type;

public:
    Oscillator(float freq = 440.0f, WaveType t = WaveType::SINE) : frequency(freq), phase(0.0f), type(t) {}
    void setFrequency(float f) { frequency = f; }
    void setWaveType(WaveType t) { type = t; }
    void reset() override { phase = 0.0f; }

    float process(float) override
    {
        float s = 0.0f;
        if (type == WaveType::SINE)          s = std::sin(phase);
        else if (type == WaveType::SQUARE)   s = (phase < PI) ? 0.8f : -0.8f;
        else if (type == WaveType::SAWTOOTH) s = (2.0f * (phase / (2.0f * PI))) - 1.0f;
        else if (type == WaveType::TRIANGLE) s = (phase < PI) ? (-1.0f + 2.0f * (phase / PI)) : (1.0f - 2.0f * ((phase - PI) / PI));
        else if (type == WaveType::PULSE)    s = (phase < 0.25f * 2.0f * PI) ? 0.8f : -0.8f;
        else if (type == WaveType::NOISE)    s = (((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f) * 0.5f;

        phase += 2.0f * PI * frequency / SAMPLE_RATE;
        if (phase >= 2.0f * PI) phase -= 2.0f * PI;
        return s;
    }
};

// 3. HIGH-PASS FILTER (Low-Cut EQ)
class HighPassFilter : public AudioNode
{
private:
    float cutoff, lastIn, lastOut;
public:
    HighPassFilter(float cut = 20.0f) : cutoff(cut), lastIn(0.0f), lastOut(0.0f) {}
    void setCutoff(float c) { cutoff = std::max(10.0f, std::min(5000.0f, c)); }
    void reset() override { lastIn = lastOut = 0.0f; }
    float process(float in) override
    {
        if (cutoff <= 25.0f) return in; // Bypass when low
        float rc = 1.0f / (2.0f * PI * cutoff), dt = 1.0f / SAMPLE_RATE, alpha = rc / (rc + dt);
        lastOut = alpha * (lastOut + in - lastIn);
        lastIn = in;
        return lastOut;
    }
};

// 4. LOW-PASS FILTER (High-Cut Tone)
class LowPassFilter : public AudioNode
{
private:
    float cutoff, lastOut;
public:
    LowPassFilter(float cut = 20000.0f) : cutoff(cut), lastOut(0.0f) {}
    void setCutoff(float c) { cutoff = std::max(100.0f, std::min(20000.0f, c)); }
    void reset() override { lastOut = 0.0f; }
    float process(float in) override
    {
        if (cutoff >= 19500.0f) return in; // Bypass when open
        float rc = 1.0f / (2.0f * PI * cutoff), dt = 1.0f / SAMPLE_RATE, alpha = dt / (rc + dt);
        return lastOut = lastOut + alpha * (in - lastOut);
    }
};

// 5. TUBE OVERDRIVE (Soft Saturation)
class Overdrive : public AudioNode
{
private:
    float drive;
public:
    Overdrive(float d = 0.0f) : drive(d) {}
    void setDrive(float d) { drive = std::max(0.0f, std::min(1.0f, d)); }
    float process(float in) override
    {
        if (drive <= 0.01f) return in;
        float x = in * (1.0f + drive * 3.0f);
        return (x > 1.4f) ? 0.95f : ((x < -1.4f) ? -0.95f : x - (x * x * x) / 3.0f);
    }
};

// 6. DISTORTION (Hard Clipper)
class Distortion : public AudioNode
{
private:
    float threshold;
public:
    Distortion(float t = 1.0f) : threshold(t) {}
    void setThreshold(float t) { threshold = std::max(0.01f, std::min(1.0f, t)); }
    float process(float in) override { return std::max(-threshold, std::min(threshold, in)); }
};

// 7. BITCRUSHER (Lo-Fi Quantizer)
class Bitcrusher : public AudioNode
{
private:
    int bitDepth;
public:
    Bitcrusher(int bits = 16) : bitDepth(bits) {}
    void setBitDepth(int b) { bitDepth = std::max(2, std::min(16, b)); }
    float process(float in) override
    {
        if (bitDepth >= 16) return in;
        float steps = std::pow(2.0f, (float)bitDepth);
        return std::round(in * (steps / 2.0f)) / (steps / 2.0f);
    }
};

// 8. REVERB (Multi-Tap Room Simulation)
class Reverb : public AudioNode
{
private:
    float *buf;
    int size, idx;
    float room;
public:
    Reverb(float r = 0.0f) : idx(0), room(r), size(static_cast<int>(SAMPLE_RATE * 0.15f)) { buf = new float[size](); }
    ~Reverb() override { delete[] buf; }
    void reset() override { std::fill(buf, buf + size, 0.0f); idx = 0; }
    void setRoomSize(float r) { room = std::max(0.0f, std::min(0.9f, r)); }
    float process(float in) override
    {
        if (room <= 0.01f) return in;
        float refl = (buf[(idx - 1103 + size) % size] + buf[(idx - 2087 + size) % size] * 0.7f + buf[(idx - 3319 + size) % size] * 0.5f) * 0.45f;
        buf[idx] = in + refl * room;
        idx = (idx + 1) % size;
        return in * (1.0f - room * 0.3f) + refl * (room * 0.7f);
    }
};

// 9. ECHO (Delay Line with Feedback)
class Echo : public AudioNode
{
private:
    float *buf;
    int maxBuf, writeIdx, delaySamples;
    float feedback;
public:
    Echo(float sec = 0.25f, float fb = 0.0f) : writeIdx(0), feedback(fb), maxBuf(SAMPLE_RATE)
    {
        buf = new float[maxBuf]();
        setDelayTime(sec);
    }
    ~Echo() override { delete[] buf; }
    void reset() override { std::fill(buf, buf + maxBuf, 0.0f); writeIdx = 0; }
    void setDelayTime(float s) { delaySamples = std::max(1, std::min((int)(SAMPLE_RATE * s), maxBuf - 1)); }
    void setFeedback(float fb) { feedback = fb; }
    float process(float in) override
    {
        if (feedback <= 0.01f) return in;
        int readIdx = (writeIdx - delaySamples + maxBuf) % maxBuf;
        float out = in + buf[readIdx] * feedback;
        buf[writeIdx] = out;
        writeIdx = (writeIdx + 1) % maxBuf;
        return out;
    }
};

// 10. TREMOLO (Amplitude Modulation)
class Tremolo : public AudioNode
{
private:
    float rate, depth, lfoPhase;
public:
    Tremolo(float r = 5.0f, float d = 0.0f) : rate(r), depth(d), lfoPhase(0.0f) {}
    void setRate(float r) { rate = r; }
    void setDepth(float d) { depth = d; }
    void reset() override { lfoPhase = 0.0f; }
    float process(float in) override
    {
        if (depth <= 0.01f) return in;
        float mod = 1.0f - (depth * 0.5f * (1.0f + std::sin(lfoPhase)));
        lfoPhase += 2.0f * PI * rate / SAMPLE_RATE;
        if (lfoPhase >= 2.0f * PI) lfoPhase -= 2.0f * PI;
        return in * mod;
    }
};

// 11. GAIN (Master Volume)
class Gain : public AudioNode
{
private:
    float volume;
public:
    Gain(float v = 0.8f) : volume(v) {}
    void setVolume(float v) { volume = v; }
    float process(float in) override { return in * volume; }
};

// 12. EFFECTS RACK (OOP: Polymorphism & Dynamic Object Array)
class EffectsRack
{
private:
    AudioNode **chain;
    int capacity, count;
public:
    EffectsRack(int cap = 15) : capacity(cap), count(0) { chain = new AudioNode*[capacity]; }
    ~EffectsRack() { delete[] chain; }
    void addNode(AudioNode *node) { if (count < capacity) chain[count++] = node; }
    void resetAll() { for (int i = 0; i < count; ++i) chain[i]->reset(); }
    float processPipeline(float in)
    {
        float s = in;
        for (int i = 0; i < count; ++i) s = chain[i]->process(s);
        return s;
    }
};

// 13. FILE HANDLING: LOSSLESS 16-BIT WAV WRITER
class WavWriter
{
    struct WavHeader
    {
        char riff[4] = {'R', 'I', 'F', 'F'};
        uint32_t overallSize;
        char wave[4] = {'W', 'A', 'V', 'E'};
        char fmt[4]  = {'f', 'm', 't', ' '};
        uint32_t lenFmt = 16;
        uint16_t format = 1, channels = 1;
        uint32_t sampleRate = SAMPLE_RATE, byteRate = SAMPLE_RATE * 2;
        uint16_t blockAlign = 2, bitsPerSample = 16;
        char data[4] = {'d', 'a', 't', 'a'};
        uint32_t dataSize;
    };
public:
    static bool save(const std::string &filename, const float *buf, int n)
    {
        std::ofstream f(filename, std::ios::binary);
        if (!f.is_open()) return false;
        WavHeader h;
        h.dataSize = n * sizeof(int16_t);
        h.overallSize = h.dataSize + sizeof(WavHeader) - 8;
        f.write(reinterpret_cast<const char*>(&h), sizeof(WavHeader));

        int fadeIn = SAMPLE_RATE * 0.02f, fadeOut = SAMPLE_RATE * 0.04f;
        for (int i = 0; i < n; ++i)
        {
            float env = (i < fadeIn) ? ((float)i / fadeIn) : ((i > n - fadeOut) ? ((float)(n - i) / fadeOut) : 1.0f);
            int16_t s = static_cast<int16_t>(std::max(-1.0f, std::min(1.0f, buf[i] * env)) * 32767.0f);
            f.write(reinterpret_cast<const char*>(&s), sizeof(int16_t));
        }
        return true;
    }
};