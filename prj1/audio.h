#include "AudioFile.h"
#include <map>
#include <memory>
typedef unsigned int uint;

class frame_fun
{
public:
    virtual double operator () (std::vector<double> main_ts, uint offset, uint frame_size) = 0;
    virtual std::string get_name() = 0;
};

class volume_fun : public frame_fun
{
public:
    double operator () (std::vector<double> main_ts, uint offset, uint frame_size) override;
    std::string get_name() override { return "volume"; }
};

class ste_fun : public frame_fun
{
public:
    double operator () (std::vector<double> main_ts, uint offset, uint frame_size) override;
    std::string get_name() override { return "STE"; }
};

class zcr_fun : public frame_fun
{
public:
    double operator () (std::vector<double> main_ts, uint offset, uint frame_size) override;
    std::string get_name() override { return "ZCR"; }
    zcr_fun(AudioFile<double> &af) {
        sampling_rate = static_cast<double>(af.getNumSamplesPerChannel()) / af.getLengthInSeconds();
    }
private:
    double sampling_rate;
};

//silent ratio
class sr_fun : public frame_fun
{
public:
    double operator () (std::vector<double> main_ts, uint offset, uint frame_size) override;
    std::string get_name() override { return "Silence ratio"; }
    sr_fun(AudioFile<double> &af) : zf(af) {}
private:
    volume_fun vf;
    zcr_fun zf;
};

class ff_fun : public frame_fun
{
public:
    double operator () (std::vector<double> main_ts, uint offset, uint frame_size) override;
    std::string get_name() override { return "Fundamental frequency"; }
    ff_fun(AudioFile<double> &af) {
        sampling_rate = static_cast<double>(af.getNumSamplesPerChannel()) / af.getLengthInSeconds();
    }
private:
    double sampling_rate;
};

class amdf_fun : public frame_fun
{
public:
    double operator () (std::vector<double> main_ts, uint offset, uint frame_size) override;
    std::string get_name() override { return "Fundamental frequency (AMDF)"; }
    amdf_fun(AudioFile<double> &af) {
        sampling_rate = static_cast<double>(af.getNumSamplesPerChannel()) / af.getLengthInSeconds();
    }
private:
    double sampling_rate;
};

class scalar_func
{
public:
    virtual double operator () (std::vector<double>::iterator start, std::vector<double>::iterator end) = 0;
    virtual std::string get_name() = 0;
};

class deviation_fun : public scalar_func
{
public:
    double operator () (std::vector<double>::iterator start, std::vector<double>::iterator end) override;
    std::string get_name() override { return "deviation"; }
};

class dynamic_range_func : public scalar_func
{
    double operator () (std::vector<double>::iterator start, std::vector<double>::iterator end) override;
    std::string get_name() override {return "dynamic range";}
};

class low_ratio_fun : public scalar_func
{
    double operator () (std::vector<double>::iterator start, std::vector<double>::iterator end) override;
    std::string get_name() override {return "low ratio"; }
};

class entropy_func : public scalar_func
{
    double operator () (std::vector<double>::iterator start, std::vector<double>::iterator end) override;
    std::string get_name() override { return "entropy"; }
};

class high_ratio_fun : public scalar_func
{
    double operator () (std::vector<double>::iterator start, std::vector<double>::iterator end) override;
    std::string get_name() override {return "high ratio"; }
};

class audio
{
public:
    struct time_params {
        std::vector<double> vals;
        std::vector<double> time_vec;
        frame_fun& fun;
        uint frame_size;
        uint overlap;
        AudioFile<double> &track;

        time_params(AudioFile<double> &af, frame_fun &ff, uint fs = 1200, uint ol = 20)
            : fun(ff), track(af)
        {
            frame_size = fs;
            overlap = ol;
            recalc();
        }
        void recalc();
    };
    audio() {}
    void init(std::string filename);
    std::vector<double> &get_time_vec();
    std::vector<double> &get_main_vec();
    int num_samples() {return af.getNumSamplesPerChannel();}
    std::map<std::string, time_params> tps;
    std::map<std::string, double> scalar_vals;
    ~audio();
    bool is_loaded();
private:
    AudioFile<double> af;
    std::vector<double> tv;
    std::vector<std::unique_ptr<frame_fun>> ffs;
    std::vector<std::pair<std::string, std::unique_ptr<scalar_func>>> scalars;
    bool loaded = false;
};
