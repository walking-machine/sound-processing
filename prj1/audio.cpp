#include "audio.h"
#include <math.h>

void audio::init(std::string filename)
{
    af.load(filename);
    tv.resize(af.getNumSamplesPerChannel());
    double period = af.getLengthInSeconds() / static_cast<double>(tv.size() - 1);
    for (int i = 0; i < tv.size(); i++) {
        tv[i] = period * static_cast<double>(i);
    }
    ffs.clear();
    tps.clear();
    ffs.resize(5);
    ffs[0] = std::make_unique<volume_fun>();
    ffs[1] = std::make_unique<ste_fun>();
    ffs[2] = std::make_unique<zcr_fun>(af);
    ffs[3] = std::make_unique<ff_fun>(af);
    ffs[4] = std::make_unique<sr_fun>(af);

    for (auto &ff : ffs) {
        tps.emplace(ff->get_name(), time_params(af, *ff));
    }

    scalars.clear();
    scalar_vals.clear();
    scalars.resize(5);
    scalars[0] = { ffs[0]->get_name(), std::make_unique<deviation_fun>() };
    scalars[1] = { ffs[0]->get_name(), std::make_unique<dynamic_range_func>() };
    scalars[2] = { ffs[1]->get_name(), std::make_unique<low_ratio_fun>() };
    scalars[3] = { ffs[2]->get_name(), std::make_unique<deviation_fun>() };
    scalars[4] = { ffs[2]->get_name(), std::make_unique<high_ratio_fun>() };

    for (auto &sf : scalars) {
        scalar_vals.emplace(sf.second->get_name() + " (" + sf.first + ") ",
            (*(sf.second))(tps.find(sf.first)->second.vals.begin(),
            tps.find(sf.first)->second.vals.end()));
    }

    loaded = true;
}

audio::~audio()
{
}

std::vector<double> &audio::get_time_vec()
{
    return tv;
}

std::vector<double> &audio::get_main_vec()
{
    return af.samples[0];
}

bool audio::is_loaded()
{
    return loaded;
}

double volume_fun::operator () (std::vector<double> main_ts, uint offset, uint frame_size)
{
    uint N = 0;
    double sum = 0;
    for (uint i = 0; i + offset < main_ts.size() && i < frame_size; i++) {
        double val = main_ts[offset + i];
        sum += val * val;
        N++;
    }

    return sqrt(sum / static_cast<double>(N));
}

double ste_fun::operator () (std::vector<double> main_ts, uint offset, uint frame_size)
{
    uint N = 0;
    double sum = 0;
    for (uint i = 0; i + offset < main_ts.size() && i < frame_size; i++) {
        double val = main_ts[offset + i];
        sum += val * val;
        N++;
    }

    return sum / static_cast<double>(N);
}

double zcr_fun::operator () (std::vector<double> main_ts, uint offset, uint frame_size)
{
    uint sum = 0;
    uint N = 1;
    for (uint i = 0; i + offset + 1 < main_ts.size() && i + 1 < frame_size; i++) {
        sum += static_cast<uint>(((uint)signbit(main_ts[offset + i]) !=
                                  (uint)signbit(main_ts[offset + i + 1])));
        N++;
    }

    return static_cast<double>(sum) * sampling_rate / static_cast<double>(N);
}
double sr_fun::operator () (std::vector<double> main_ts, uint offset, uint frame_size)
{
    double volume = vf(main_ts, offset, frame_size);
    double zcr = zf(main_ts, offset, frame_size);

    if (volume < 0.02)
        return zcr > 50 ? 0.5 : 1;
    return 0;
}

double ff_fun::operator () (std::vector<double> main_ts, uint offset, uint frame_size)
{
    double min_rn = std::numeric_limits<double>::min();
    uint best_l = 0;
    for (uint l = 40; l < frame_size * 2 / 3; l++) {
        double rm = 0;
        uint N = 0;
        for (uint i = 0; i < frame_size - l; i++) {
            rm += main_ts[offset + i] * main_ts[offset + i + l];
        }

        if (rm > min_rn) {
            min_rn = rm;
            best_l = l;
        }
    }

    return sampling_rate / static_cast<double>(best_l);
}
double amdf_fun::operator () (std::vector<double> main_ts, uint offset, uint frame_size) {return 3;}

static double average(std::vector<double>::iterator start, std::vector<double>::iterator end)
{
    double sum = 0;
    double N = static_cast<double>(end - start);
    for (auto it = start; it < end; ++it) {
        sum += *it;
    }

    return sum / N;
}

double deviation_fun::operator () (std::vector<double>::iterator start, std::vector<double>::iterator end)
{
    double avg = average(start, end);
    double N = static_cast<double>(end - start);
    double dev = 0;

    for (auto it = start; it < end; ++it) {
        dev += pow(*it - avg, 2);
    }

    return sqrt(dev / N);
}

double dynamic_range_func::operator () (std::vector<double>::iterator start, std::vector<double>::iterator end)
{
    double max_v = *std::max_element(start, end);
    double min_v = *std::min_element(start, end);

    return (max_v - min_v) / max_v;
}

double low_ratio_fun::operator () (std::vector<double>::iterator start, std::vector<double>::iterator end)
{
    double avg = average(start, end) * 0.5;
    uint sum = 0;
    double N = static_cast<double>(end - start);

    for (auto it = start; it < end; ++it) {
        sum += signbit(*it - avg);
    }

    return static_cast<double>(sum) / N;
}

double entropy_func::operator () (std::vector<double>::iterator start, std::vector<double>::iterator end)
{}

double high_ratio_fun::operator () (std::vector<double>::iterator start, std::vector<double>::iterator end)
{
    double avg = average(start, end) * 1.5;
    uint sum = 0;
    double N = static_cast<double>(end - start);

    for (auto it = start; it < end; ++it) {
        sum += signbit(avg - *it);
    }

    return static_cast<double>(sum) / N;
}

void audio::time_params::recalc()
{
    if (overlap > frame_size)
        overlap = frame_size - 1;

    uint stride = frame_size - overlap;
    uint ns = track.getNumSamplesPerChannel();
    uint nf = ns / stride + ((ns % stride > 0) ? 1 : 0);
    double step = track.getLengthInSeconds() / static_cast<double>(nf - 1);

    vals.resize(nf);
    time_vec.resize(nf);

    for (uint i = 0; i < nf; i++) {
        vals[i] = fun(track.samples[0], i * stride, frame_size);
        time_vec[i] =  static_cast<double>(i) * step;
    }
}