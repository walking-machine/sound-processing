#include <complex>
#include <valarray>

typedef std::complex<double> dcomplex;
namespace audio_utils
{

static void fft_in_place(std::valarray<dcomplex> &time_series)
{
    const size_t N = time_series.size();
    if (N <= 1) return;

    std::valarray<dcomplex> even = time_series[std::slice(0, N/2, 2)];
    std::valarray<dcomplex> odd = time_series[std::slice(1, N/2, 2)];

    fft_in_place(even);
    fft_in_place(odd);

    for (uint i = 0; i < N/2; i++)
    {
        dcomplex t = std::polar(1.0, -2 * M_PI * i / N) * odd[i];
        time_series[i] = even[i] + t;
        time_series[i+N/2] = even[i] - t;
    }
}

static void fft_inverse(std::valarray<dcomplex> &freq_series)
{
    const size_t N = freq_series.size();
    if (N <= 1) return;

    freq_series = freq_series.apply(std::conj);
    
    fft_in_place(freq_series);

    freq_series = freq_series.apply(std::conj);
    freq_series /= N;
}

static void cepstrum(std::valarray<dcomplex> &fft)
{
    for (auto& c : fft)
        c = log(c);
    
    fft_inverse(fft);
}

};