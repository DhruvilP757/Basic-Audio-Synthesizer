#pragma once
#include <fstream>
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cstdlib>

const int SAMPLE_RATE = 192000; // Studio Master Ultra Hi-Res (192 kHz)
const float PI = 3.14159265f;

// 1. ABSTRACT BASE CLASS (OOP: Inheritance & Virtual Functions)
class AudioNode
{
public:
    virtual ~AudioNode() {}
    virtual float process(float in) = 0; // Pure virtual function
    virtual void reset() {}
};

// 2. OSCILLATOR (6 Waveforms)
enum class WaveType { SINE, SQUARE, SAWTOOTH, TRIANGLE, PULSE, NOISE };

class Oscillator : public AudioNode
{
private:
    float freq, phase = 0.0f;
    WaveType type;

public:
    Oscillator(float f = 440.0f, WaveType t = WaveType::SINE) : freq(f), type(t) {}
    void setFrequency(float f) { freq = f; }
    void setWaveType(WaveType t) { type = t; }
    void reset() override { phase = 0.0f; }

    float process(float) override
    {
        float s = 0.0f;
        if (type == WaveType::SINE)          s = std::sin(phase);
        else if (type == WaveType::SQUARE)   s = (phase < PI) ? 0.8f : -0.8f;
        else if (type == WaveType::SAWTOOTH) s = (phase / PI) - 1.0f;
        else if (type == WaveType::TRIANGLE) s = (phase < PI) ? (-1.0f + 2.0f * (phase / PI)) : (3.0f - 2.0f * (phase / PI));
        else if (type == WaveType::PULSE)    s = (phase < 0.5f * PI) ? 0.8f : -0.8f;
        else if (type == WaveType::NOISE)    s = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * 0.5f;

        phase += 2.0f * PI * freq / SAMPLE_RATE;
        if (phase >= 2.0f * PI) phase -= 2.0f * PI;
        return s;
    }
};

// 3. HIGH-PASS FILTER (Low-Cut EQ)
class HighPassFilter : public AudioNode
{
    float cutoff, lastIn = 0.0f, lastOut = 0.0f;
public:
    HighPassFilter(float cut = 20.0f) : cutoff(cut) {}
    void setCutoff(float c) { cutoff = std::clamp(c, 10.0f, 5000.0f); }
    void reset() override { lastIn = lastOut = 0.0f; }
    float process(float in) override
    {
        if (cutoff <= 25.0f) return in;
        float rc = 1.0f / (2.0f * PI * cutoff), alpha = rc / (rc + 1.0f / SAMPLE_RATE);
        lastOut = alpha * (lastOut + in - lastIn);
        lastIn = in;
        return lastOut;
    }
};

// 4. LOW-PASS FILTER (High-Cut Tone)
class LowPassFilter : public AudioNode
{
    float cutoff, lastOut = 0.0f;
public:
    LowPassFilter(float cut = 20000.0f) : cutoff(cut) {}
    void setCutoff(float c) { cutoff = std::clamp(c, 100.0f, 20000.0f); }
    void reset() override { lastOut = 0.0f; }
    float process(float in) override
    {
        if (cutoff >= 19500.0f) return in;
        float rc = 1.0f / (2.0f * PI * cutoff), alpha = (1.0f / SAMPLE_RATE) / (rc + 1.0f / SAMPLE_RATE);
        return lastOut += alpha * (in - lastOut);
    }
};

// 5. TUBE OVERDRIVE (Soft Saturation)
class Overdrive : public AudioNode
{
    float drive = 0.0f;
public:
    Overdrive(float d = 0.0f) : drive(d) {}
    void setDrive(float d) { drive = std::clamp(d, 0.0f, 1.0f); }
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
    float threshold = 1.0f;
public:
    Distortion(float t = 1.0f) : threshold(t) {}
    void setThreshold(float t) { threshold = std::clamp(t, 0.01f, 1.0f); }
    float process(float in) override { return std::clamp(in, -threshold, threshold); }
};

// 7. BITCRUSHER (Lo-Fi Quantizer)
class Bitcrusher : public AudioNode
{
    int bits = 16;
public:
    Bitcrusher(int b = 16) : bits(b) {}
    void setBitDepth(int b) { bits = std::clamp(b, 2, 16); }
    float process(float in) override
    {
        if (bits >= 16) return in;
        float steps = std::pow(2.0f, (float)bits);
        return std::round(in * (steps * 0.5f)) / (steps * 0.5f);
    }
};

// 8. REVERB (Multi-Tap Room Simulation)
class Reverb : public AudioNode
{
    float *buf;
    int size, idx = 0;
    float room;
public:
    Reverb(float r = 0.0f) : room(r), size(static_cast<int>(SAMPLE_RATE * 0.15f)) { buf = new float[size](); }
    ~Reverb() override { delete[] buf; }
    void reset() override { std::fill(buf, buf + size, 0.0f); idx = 0; }
    void setRoomSize(float r) { room = std::clamp(r, 0.0f, 0.9f); }
    float process(float in) override
    {
        if (room <= 0.01f) return in;
        int t1 = static_cast<int>(SAMPLE_RATE * 0.025f), t2 = static_cast<int>(SAMPLE_RATE * 0.047f), t3 = static_cast<int>(SAMPLE_RATE * 0.075f);
        float refl = (buf[(idx - t1 + size) % size] + buf[(idx - t2 + size) % size] * 0.7f + buf[(idx - t3 + size) % size] * 0.5f) * 0.45f;
        buf[idx] = in + refl * room;
        idx = (idx + 1) % size;
        return in * (1.0f - room * 0.3f) + refl * (room * 0.7f);
    }
};

// 9. ECHO (Delay Line with Feedback)
class Echo : public AudioNode
{
    float *buf;
    int maxBuf, writeIdx = 0, delaySamples;
    float feedback;
public:
    Echo(float sec = 0.25f, float fb = 0.0f) : feedback(fb), maxBuf(SAMPLE_RATE)
    {
        buf = new float[maxBuf]();
        setDelayTime(sec);
    }
    ~Echo() override { delete[] buf; }
    void reset() override { std::fill(buf, buf + maxBuf, 0.0f); writeIdx = 0; }
    void setDelayTime(float s) { delaySamples = std::clamp(static_cast<int>(SAMPLE_RATE * s), 1, maxBuf - 1); }
    void setFeedback(float fb) { feedback = fb; }
    float process(float in) override
    {
        if (feedback <= 0.01f) return in;
        float out = in + buf[(writeIdx - delaySamples + maxBuf) % maxBuf] * feedback;
        buf[writeIdx] = out;
        writeIdx = (writeIdx + 1) % maxBuf;
        return out;
    }
};

// 10. TREMOLO (Amplitude Modulation)
class Tremolo : public AudioNode
{
    float rate, depth, phase = 0.0f;
public:
    Tremolo(float r = 5.0f, float d = 0.0f) : rate(r), depth(d) {}
    void setRate(float r) { rate = r; }
    void setDepth(float d) { depth = d; }
    void reset() override { phase = 0.0f; }
    float process(float in) override
    {
        if (depth <= 0.01f) return in;
        float mod = 1.0f - (depth * 0.5f * (1.0f + std::sin(phase)));
        phase += 2.0f * PI * rate / SAMPLE_RATE;
        if (phase >= 2.0f * PI) phase -= 2.0f * PI;
        return in * mod;
    }
};

// 11. GAIN (Master Volume)
class Gain : public AudioNode
{
    float volume;
public:
    Gain(float v = 0.8f) : volume(v) {}
    void setVolume(float v) { volume = v; }
    float process(float in) override { return in * volume; }
};

// 12. EFFECTS RACK (OOP: Polymorphism & Dynamic Object Array)
class EffectsRack
{
    AudioNode **chain;
    int capacity, count;
public:
    EffectsRack(int cap = 10) : capacity(cap), count(0) { chain = new AudioNode*[capacity]; }
    ~EffectsRack() { delete[] chain; }
    void addNode(AudioNode *n) { if (count < capacity) chain[count++] = n; }
    void resetAll() { for (int i = 0; i < count; ++i) chain[i]->reset(); }
    float processPipeline(float in)
    {
        float s = in;
        for (int i = 0; i < count; ++i) s = chain[i]->process(s);
        return s;
    }
};

// 13. CHANNEL EFFECTS PIPELINE (OOP: Encapsulates all 10 effects for a wave channel)
class ChannelEffects
{
public:
    HighPassFilter *highPass;
    LowPassFilter  *lowPass;
    Overdrive      *overdrive;
    Distortion     *dist;
    Bitcrusher     *bitcrush;
    Reverb         *reverb;
    Echo           *echo;
    Tremolo        *tremolo;
    Gain           *gain;
    EffectsRack    *rack;

    ChannelEffects()
    {
        rack = new EffectsRack(10);
        rack->addNode(highPass  = new HighPassFilter(20.0f));
        rack->addNode(lowPass   = new LowPassFilter(20000.0f));
        rack->addNode(overdrive = new Overdrive(0.0f));
        rack->addNode(dist      = new Distortion(1.0f));
        rack->addNode(bitcrush  = new Bitcrusher(16));
        rack->addNode(reverb    = new Reverb(0.0f));
        rack->addNode(echo      = new Echo(0.25f, 0.0f));
        rack->addNode(tremolo   = new Tremolo(5.0f, 0.0f));
        rack->addNode(gain      = new Gain(0.8f));
    }

    ~ChannelEffects()
    {
        delete highPass; delete lowPass; delete overdrive; delete dist;
        delete bitcrush; delete reverb; delete echo; delete tremolo;
        delete gain; delete rack;
    }

    void reset() { rack->resetAll(); }
    float process(float in) { return rack->processPipeline(in); }
};

// 14. FILE HANDLING: STUDIO MASTER ULTRA HI-RES (192 kHz, 32-bit Float Lossless WAV)
class WavWriter
{
    struct WavHeader
    {
        char riff[4] = {'R', 'I', 'F', 'F'};
        uint32_t overallSize;
        char wave[4] = {'W', 'A', 'V', 'E'};
        char fmt[4]  = {'f', 'm', 't', ' '};
        uint32_t lenFmt = 16;
        uint16_t format = 3;                 // 3 = 32-bit IEEE Float (Studio Master Quality)
        uint16_t channels = 1;               // Mono
        uint32_t sampleRate = SAMPLE_RATE;   // 192,000 Hz Ultra Hi-Res
        uint32_t byteRate = SAMPLE_RATE * 4; // 768,000 bytes/sec
        uint16_t blockAlign = 4;             // channels * (32 / 8) = 4
        uint16_t bitsPerSample = 32;         // 32-bit Ultra Hi-Res
        char data[4] = {'d', 'a', 't', 'a'};
        uint32_t dataSize;

        WavHeader(uint32_t samples = 0)
        {
            dataSize = samples * sizeof(float);
            overallSize = dataSize + sizeof(WavHeader) - 8;
        }
    };

public:
    static bool save(const std::string &filename, const float *buf, int n)
    {
        std::ofstream f(filename, std::ios::binary);
        if (!f.is_open()) return false;
        WavHeader h(n);
        f.write(reinterpret_cast<const char*>(&h), sizeof(WavHeader));
        int in = static_cast<int>(SAMPLE_RATE * 0.02f), out = static_cast<int>(SAMPLE_RATE * 0.04f);
        for (int i = 0; i < n; ++i)
        {
            float env = (i < in) ? ((float)i / in) : ((i > n - out) ? ((float)(n - i) / out) : 1.0f);
            float s = std::clamp(buf[i] * env, -1.0f, 1.0f);
            f.write(reinterpret_cast<const char*>(&s), sizeof(float));
        }
        return true;
    }

    // Stream rendering: exports audio in 1-second chunks (768 KB) up to 10 minutes
    static bool saveStream(const std::string &filename, Oscillator *osc1, Oscillator *osc2,
                           ChannelEffects &chan1, ChannelEffects &chan2, bool superimpose, float sec)
    {
        std::ofstream f(filename, std::ios::binary);
        if (!f.is_open()) return false;
        int total = static_cast<int>(SAMPLE_RATE * sec);
        WavHeader h(total);
        f.write(reinterpret_cast<const char*>(&h), sizeof(WavHeader));

        osc1->reset();
        if (superimpose && osc2) osc2->reset();
        chan1.reset();
        chan2.reset();

        const int CHUNK = SAMPLE_RATE;
        std::vector<float> chunk(CHUNK);
        int in = static_cast<int>(SAMPLE_RATE * 0.02f), out = static_cast<int>(SAMPLE_RATE * 0.04f);

        for (int written = 0; written < total; written += CHUNK)
        {
            int count = std::min(CHUNK, total - written);
            for (int i = 0; i < count; ++i)
            {
                int g = written + i;
                float env = (g < in) ? ((float)g / in) : ((g > total - out) ? ((float)(total - g) / out) : 1.0f);
                float s = chan1.process(osc1->process(0.0f));
                if (superimpose && osc2) s = 0.5f * (s + chan2.process(osc2->process(0.0f)));
                chunk[i] = std::clamp(s * env, -1.0f, 1.0f);
            }
            f.write(reinterpret_cast<const char*>(chunk.data()), count * sizeof(float));
        }
        return true;
    }
};