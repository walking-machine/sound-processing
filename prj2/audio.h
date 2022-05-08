#include "AudioFile.h"
#include <map>
#include <memory>
#include <complex>
#include <valarray>
typedef unsigned int uint;
typedef std::complex<double> dcomplex;

class freq_param
{
public:
    virtual double operator () (std::vector<double>::iterator first_sample,
                      std::vector<double>::iterator end_sample) = 0;
    virtual std::string name() = 0;
    virtual void draw_controls() = 0;
};

class audio
{
public:
    audio() {}
    void init(std::string filename);
    int num_samples() {return af.getNumSamplesPerChannel();}
    ~audio();
    bool is_loaded();
    double time_length() { return af.getLengthInSeconds(); }
    struct sig_window {
        bool rect = true;
        float start_time = 0.0;
        float end_time = 1.0;
        float a0 = 0.0;
        double win_fun[100];
        double win_tv[100];
        void update_fun();
        void apply(std::vector<double> &values);
        sig_window(bool is_rect, double start_s, double end_s, double a0);
        sig_window(bool is_rect, double a0, audio &a) {
            sig_window(is_rect, 0.0, a.time_length(), a0);
        }
        sig_window(audio &a) { sig_window(true, 0.0, a); }
        sig_window() {}
        void draw();
        void draw_controls(audio& a);
    };
    void apply_window(sig_window &win);
    void update_fft();
    void draw_full(sig_window &win);
    void draw_windowed();
    void draw_fft();
    double sampling_freq();
private:
    AudioFile<double> af;
    std::vector<double> windowed;
    std::vector<double> tv;
    std::vector<double> win_tv;
    std::vector<double> freq_amp;
    std::vector<double> freq_vec;
    bool loaded = false;
    double last_win_len = -1.0;
    double last_win_start = -1.0;
    double win_start_t();
    double win_len_t();
    std::vector<std::unique_ptr<freq_param>> params;
    std::vector<double> param_values;
    void recalc_win_params();
    void show_win_params();
};

class volume_param : public freq_param
{
public:
    double operator () (std::vector<double>::iterator first_sample,
                        std::vector<double>::iterator end_sample) override;
    std::string name() override;
    void draw_controls() override {}
};

class centroid_param : public freq_param
{
public:
    centroid_param(double freq_threshold) : max_freq(freq_threshold) {}
    double operator () (std::vector<double>::iterator first_sample,
                        std::vector<double>::iterator end_sample) override;
    std::string name() override;
    void draw_controls() override {}
private:
    double max_freq;
};

class effective_bw_param : public freq_param
{
public:
    effective_bw_param(double freq_threshold) : max_freq(freq_threshold), cp(freq_threshold) {}
    double operator () (std::vector<double>::iterator first_sample,
                        std::vector<double>::iterator end_sample) override;
    std::string name() override;
    void draw_controls() override {}
private:
    double max_freq;
    centroid_param cp;
};

class ber_param : public freq_param
{
public:
    ber_param(uint band_num) : band_num(band_num) {}
    double operator () (std::vector<double>::iterator first_sample,
                        std::vector<double>::iterator end_sample) override;
    std::string name() override;
    void draw_controls() override {}
private:
    uint band_num;
};

class flatness_param : public freq_param
{
public:
    flatness_param(double max_freq, double low_freq, double high_freq) :
        low(low_freq / max_freq), high(high_freq / max_freq) {}
    flatness_param(double low_aspect, double high_aspect) :
        low(low_aspect), high(high_aspect) {}
    double operator () (std::vector<double>::iterator first_sample,
                        std::vector<double>::iterator end_sample) override;
    std::string name() override;
    void draw_controls() override;
private:
    float low;
    float high;
};

class crest_param : public freq_param
{
public:
    crest_param(double max_freq, double low_freq, double high_freq) :
        low(low_freq / max_freq), high(high_freq / max_freq) {}
    crest_param(double low_aspect, double high_aspect) :
        low(low_aspect), high(high_aspect) {}
    double operator () (std::vector<double>::iterator first_sample,
                        std::vector<double>::iterator end_sample) override;
    std::string name() override;
    void draw_controls() override;
private:
    float low;
    float high;
};