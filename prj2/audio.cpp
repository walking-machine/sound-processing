#include "audio.h"
#include <math.h>
#include "implot.h"
#include "audio_utils.h"

void audio::init(std::string filename)
{
	af.load(filename);
	tv.resize(af.getNumSamplesPerChannel());
	double period = af.getLengthInSeconds() / static_cast<double>(tv.size() - 1);
	for (uint i = 0; i < tv.size(); i++) {
		tv[i] = period * static_cast<double>(i);
	}

	windowed = std::vector<double>(af.samples[0].begin(), af.samples[0].end());
	loaded = true;

	params.resize(9);
	param_values = std::vector<double>(params.size(), -2.0);
	params[0] = std::make_unique<volume_param>();
	params[1] = std::make_unique<centroid_param>(sampling_freq() / 2.0);
	params[2] = std::make_unique<effective_bw_param>(sampling_freq() / 2.0);
	params[3] = std::make_unique<ber_param>(0);
	params[4] = std::make_unique<ber_param>(1);
	params[5] = std::make_unique<ber_param>(2);
	params[6] = std::make_unique<ber_param>(3);
	params[7] = std::make_unique<flatness_param>(0.1, 0.9);
	params[8] = std::make_unique<crest_param>(0.1, 0.9);

}

audio::~audio()
{
}

bool audio::is_loaded()
{
	return loaded;
}

void audio::apply_window(sig_window &win) {
	uint N = af.getNumSamplesPerChannel();
	double len = af.getLengthInSeconds();
	uint first_probe = floor(win.start_time * static_cast<double>(N) / len);
	uint end_probe = ceil(win.end_time * static_cast<double>(N) / len);
	end_probe = (N > end_probe) ? end_probe : N;

	std::cout << "Start: " << first_probe << ", End: " << end_probe << "\n";
	std::cout << "Size: " << N << "\n";

	windowed = std::vector<double>(af.samples[0].begin() + first_probe,
								   af.samples[0].begin() + end_probe);

	std::cout << "Applying\n";
	win.apply(windowed);
	last_win_len = win.end_time - win.start_time;
	last_win_start = win.start_time;
}

double audio::sampling_freq()
{
	return static_cast<double>(af.getNumSamplesPerChannel()) / af.getLengthInSeconds();
}

void audio::update_fft()
{
	std::valarray<dcomplex> fft(windowed.size());
	for (uint i = 0; i < windowed.size(); i++) {
		fft[i] = windowed[i];
	}

	audio_utils::fft_in_place(fft);

	double max_freq = sampling_freq() / 2.0;
	uint freq_amp_size = static_cast<uint>(round(windowed.size() * max_freq / sampling_freq()));
	freq_amp.resize(freq_amp_size);
	for (uint i = 0; i < freq_amp.size(); i++) {
		freq_amp[i] = 2 * pow(abs(fft[i]), 2) / static_cast<double>(windowed.size());
	}

	recalc_win_params();

	//	Cepstrum nie dziala :(
	audio_utils::cepstrum(fft);
	std::vector<double> cepstrum_real(fft.size());
	for (uint i = 0; i < fft.size(); i++)
		cepstrum_real[i] = fft[i].real();

	uint freq_samples = std::max_element(cepstrum_real.begin() + 20, cepstrum_real.begin() + 100)
		- cepstrum_real.begin();
	std::cout << "Cepstrum samples: " << freq_samples << ", frequency: "<<
		sampling_freq() / static_cast<double>(freq_samples) << "\n";
}

void audio::draw_full(sig_window &win) {
	if(!ImPlot::BeginPlot("Full signal in time"))
		return;
	
	win.draw();
	ImPlot::PlotLine("Signal", af.samples[0].data(), num_samples(),
					 time_length() / static_cast<double>(num_samples()), 0.0);
	ImPlot::EndPlot();

}

double audio::win_len_t() {
	return (last_win_len < 0) ? af.getNumSamplesPerChannel() : last_win_len;
}

double audio::win_start_t() {
	return (last_win_start < 0) ? 0.0 : last_win_start;
}

void audio::draw_windowed() {
	if(!ImPlot::BeginPlot("Windowed signal in time"))
		return;

	ImPlot::PlotLine("Signal", windowed.data(), windowed.size(),
					 last_win_len / static_cast<double>(windowed.size()), last_win_start);
	
	ImPlot::EndPlot();
}

void audio::draw_fft() {
	if(!ImPlot::BeginPlot("Frequency"))
		return;

	double fs = sampling_freq();

	ImPlot::PlotLine("Widmo", freq_amp.data(), freq_amp.size(),
					 fs / windowed.size(), 0);
	ImPlot::EndPlot();
	show_win_params();
}

void audio::recalc_win_params()
{
	for (uint i = 0; i < params.size(); i++)
		param_values[i] = (*params[i])(freq_amp.begin(), freq_amp.end());
}

void audio::show_win_params()
{
	for (uint i = 0; i < params.size(); i++) {
		ImGui::Text("%s: %lf", params[i]->name().c_str(), param_values[i]);
		params[i]->draw_controls();
	}

	if (ImGui::Button("Recalc"))
		recalc_win_params();
}

audio::sig_window::sig_window(bool is_rect, double start_s, double end_s, double a0)
            : rect(is_rect), start_time(start_s), end_time(end_s), a0(a0)
{
	std::cout << "start: " << start_time << ", end: " << end_time << "\n";
	update_fun();
}

void audio::sig_window::draw() {
	ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.5f);
	ImPlot::PlotShaded("Window", win_fun, 100, 0.0, (end_time - start_time) / 99.0,
					   start_time);
	ImPlot::PlotLine("win", win_fun, 100, (end_time - start_time) / 99.0,
					 start_time);
}

void audio::sig_window::draw_controls(audio &a) {
	ImGui::DragFloat("start", &start_time, 0.01f, 0.f,
					 static_cast<float>(a.time_length() - 0.01));
	ImGui::DragFloat("end", &end_time, 0.01f, 0.01f, static_cast<float>(a.time_length()));

	ImGui::Checkbox("Rectangular", &rect);

	ImGui::DragFloat("a0", &a0, 0.001f, 0.f, 1.f); ImGui::SameLine();
	if (ImGui::Button("Hann"))
		a0 = 0.5f;
	ImGui::SameLine();
	if (ImGui::Button("Hamming"))
		a0 = 0.53836f;

	if (ImGui::Button("Show"))
		update_fun();
}

void audio::sig_window::update_fun() {
	uint N = 100;
	std::cout << (rect ? "Setting all to 1" : "Calculating stuff") << "\n";
	for (uint i = 0; i < N; i++)
		win_fun[i] = rect ? 1.0 : (a0 - (1 - a0) *
					 cos(2.0 * M_PI * static_cast<double>(i) / static_cast<double>(N)));
}

void audio::sig_window::apply(std::vector<double> &values) {
	if (rect)
		return;

	uint N = values.size();

	for (uint i = 0; i < N; i++) {
		values[i] = values[i] * (a0 - (1 - a0) *
					cos(2.0 * M_PI * static_cast<double>(i) / static_cast<double>(N)));
	}
}

double volume_param::operator () (std::vector<double>::iterator first_sample,
                        std::vector<double>::iterator end_sample)
{
	double volume = 0.0;
	uint N = end_sample - first_sample;
	if (N < 1)
		return volume;

	for (auto it = first_sample; it < end_sample; ++it)
		volume += abs(*it);

	return volume;
}

std::string volume_param::name() {return "Volume"; }

double centroid_param::operator () (std::vector<double>::iterator first_sample,
                        std::vector<double>::iterator end_sample)
{
	double amp_sum = 0.0;
	double freq_amp_sum = 0.0;
	uint N = end_sample - first_sample;
	if (N < 1)
		return -1.0;

	double df = max_freq / static_cast<double>(N);

	for (auto it = first_sample; it < end_sample; ++it) {
		amp_sum += sqrt(*it);
		freq_amp_sum += sqrt(*it) * df * static_cast<double>(it - first_sample);
	}

	return freq_amp_sum / amp_sum;
}

std::string centroid_param::name() {return "Frequency centroid"; }

double effective_bw_param::operator () (std::vector<double>::iterator first_sample,
                        std::vector<double>::iterator end_sample)
{
	double amp_sum = 0.0;
	double freq_amp_sum = 0.0;
	uint N = end_sample - first_sample;
	if (N < 1)
		return -1.0;

	double fc = cp(first_sample, end_sample);
	double df = max_freq / static_cast<double>(N);

	for (auto it = first_sample; it < end_sample; ++it) {
		amp_sum += *it;
		freq_amp_sum += (*it) * pow(df * static_cast<double>(it - first_sample) - fc, 2);
	}

	return sqrt(freq_amp_sum / amp_sum);
}

std::string effective_bw_param::name() {return "Effective bandwidth"; }

double ber_param::operator () (std::vector<double>::iterator first_sample,
                        std::vector<double>::iterator end_sample)
{
	double sub_sum = 0.0;
	double full_sum = 0.0;
	uint N = end_sample - first_sample;
	if (N < 8)
		return -1.0;
	
	uint start_id = 0;
	uint end_id = N / 8;

	switch (band_num)
	{
	case 1:
		start_id = N / 8;
		end_id = N / 4;
		break;
	case 2:
		start_id = N / 4;
		end_id = N / 2;
		break;
	case 3:
		start_id = N / 2;
		end_id = N;
		break;
	default:
		break;
	}

	for (auto it = first_sample; it < end_sample; ++it)
		full_sum += *it;
	
	for (auto it = first_sample + start_id; it < first_sample + end_id; ++it)
		sub_sum += *it;

	return sub_sum / full_sum;
}

std::string ber_param::name() {return "Sub-band " + std::to_string(band_num) + " ratio:"; }

double flatness_param::operator () (std::vector<double>::iterator first_sample,
                        std::vector<double>::iterator end_sample)
{
	double geom_avg = 1.0;
	double arith_avg = 0.0;
	uint N = end_sample - first_sample;
	if (N < 1)
		return -1.0;

	uint start_id = static_cast<uint>(round(N * low));
	uint end_id = static_cast<uint>(round(N * high));

	for (auto it = first_sample + start_id; it < first_sample + end_id; ++it) {
		geom_avg *= *it;
		arith_avg += *it;
	}

	return pow(geom_avg, 1.0 / (end_id - start_id)) * (end_id - start_id) / arith_avg;
}

std::string flatness_param::name() {return "Spectral Flatness Measure"; }
void flatness_param::draw_controls()
{
	ImGui::DragFloat("flatness - low", &low, 0.001, 0.0, high);
	ImGui::DragFloat("flatness - high", &high, 0.001, low, 1.0);
}

double crest_param::operator () (std::vector<double>::iterator first_sample,
                        std::vector<double>::iterator end_sample)
{
	double arith_avg = 0.0;
	uint N = end_sample - first_sample;
	if (N < 1)
		return -1.0;

	uint start_id = static_cast<uint>(round(N * low));
	uint end_id = static_cast<uint>(round(N * high));

	for (auto it = first_sample + start_id; it < first_sample + end_id; ++it) {
		arith_avg += *it;
	}

	return (*std::max_element(first_sample + start_id, first_sample + end_id))
		* (end_id - start_id) / arith_avg;
}

std::string crest_param::name() {return "Spectral Flatness Crest"; }
void crest_param::draw_controls()
{
	ImGui::DragFloat("crest - low", &low, 0.001, 0.0, high);
	ImGui::DragFloat("crest - high", &high, 0.001, low, 1.0);
}