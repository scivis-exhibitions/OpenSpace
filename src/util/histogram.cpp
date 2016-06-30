/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2015-2016                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#include <openspace/util/histogram.h>

#include <ghoul/logging/logmanager.h>

#include <cmath>
#include <cassert>
namespace {
    const std::string _loggerCat = "Histogram";
}

namespace openspace {

Histogram::Histogram()
    : _minValue(0)
    , _maxValue(0)
    , _numBins(-1)
    , _numValues(0)
    , _data(nullptr) {}

Histogram::Histogram(float minValue, float maxValue, int numBins)
    : _minValue(minValue)
    , _maxValue(maxValue)
    , _numBins(numBins)
    , _numValues(0)
    , _data(nullptr) {

    _data = new float[numBins];
    for (int i = 0; i < numBins; ++i) {
        _data[i] = 0.0;
    }
}

Histogram::Histogram(float minValue, float maxValue, int numBins, float *data)
    : _minValue(minValue)
    , _maxValue(maxValue)
    , _numBins(numBins)
    , _numValues(0)
    , _data(data) {}

Histogram::Histogram(Histogram&& other) {
    _minValue = other._minValue;
    _maxValue = other._maxValue;
    _numBins = other._numBins;
    _numValues = other._numValues;
    _data = other._data;
    other._data = nullptr;
}

Histogram& Histogram::operator=(Histogram&& other) {
    _minValue = other._minValue;
    _maxValue = other._maxValue;
    _numBins = other._numBins;
    _numValues = other._numValues;
    _data = other._data;
    other._data = nullptr;
    return *this;
}


Histogram::~Histogram() {
    if (_data) {
        delete[] _data;
    }
}


int Histogram::numBins() const {
    return _numBins;
}

float Histogram::minValue() const {
    return _minValue;
}

float Histogram::maxValue() const {
    return _maxValue;
}

bool Histogram::isValid() const {
    return _numBins != -1;
}

int Histogram::numValues() const {
    return _numValues;
}


bool Histogram::add(float value, float repeat) {
    if (value < _minValue || value > _maxValue) {
        LWARNING("the value is out of range");
        return false;
    }

    float normalizedValue = (value - _minValue) / (_maxValue - _minValue);      // [0.0, 1.0]
    int binIndex = std::min( (float)floor(normalizedValue * _numBins), _numBins - 1.0f ); // [0, _numBins - 1]

    _data[binIndex] += repeat;
    _numValues += repeat;

    return true;
}

void Histogram::changeRange(float minValue, float maxValue) {
    // If both minValue and maxValue is within range of old min and max
    // do not change anything. new range must be bigger.
    if(minValue > _minValue && maxValue < _maxValue) return;

    // If old min value is smaller that the proposed new one. Then keep the old one. 
    if(minValue > _minValue)
        minValue = _minValue;

    // If old max value is smaller that the proposed new one. Then keep the old one. 
    if(maxValue < _maxValue)
        maxValue = _maxValue;

    float* oldData = _data;
    float* newData = new float[_numBins]{0.0};

    for(int i=0; i<_numBins; i++){
        float unNormalizedValue = minValue + (i+0.5)*(_maxValue-_minValue)/(float)_numBins;
        float normalizedValue = (unNormalizedValue - minValue) / (maxValue - minValue);      // [0.0, 1.0]
        int binIndex = std::min( (float)floor(normalizedValue * _numBins), _numBins - 1.0f ); // [0, _numBins - 1]

        newData[binIndex] = oldData[i];
    }

    _minValue = minValue;
    _maxValue = maxValue;

    _data = newData;
    delete oldData;
}

bool Histogram::add(const Histogram& histogram) {
    if (_minValue == histogram.minValue() && _maxValue == histogram.maxValue() && _numBins == histogram.numBins()) {

        const float* data = histogram.data();
        for (int i = 0; i < _numBins; i++) {

            _data[i] += data[i];

        }
        _numValues += histogram._numValues;
        return true;
    } else {
        LERROR("Dimension mismatch");
        return false;
    }
}

bool Histogram::addRectangle(float lowBin, float highBin, float value) {
    if (lowBin == highBin) return true;
    if (lowBin > highBin) {
        std::swap(lowBin, highBin);
    }
    if (lowBin < _minValue || highBin > _maxValue) {
        // Out of range
        return false;
    }

    float normalizedLowBin = (lowBin - _minValue) / (_maxValue - _minValue);
    float normalizedHighBin = (highBin - _minValue) / (_maxValue - _minValue);

    float lowBinIndex = normalizedLowBin * _numBins;
    float highBinIndex = normalizedHighBin * _numBins;

    int fillLow = floor(lowBinIndex);
    int fillHigh = ceil(highBinIndex);

    for (int i = fillLow; i < fillHigh; i++) {
        _data[i] += value;
    }

    if (lowBinIndex > fillLow) {
        float diff = lowBinIndex - fillLow;
        _data[fillLow] -= diff * value;
    }
    if (highBinIndex < fillHigh) {
        float diff = -highBinIndex + fillHigh;
        _data[fillHigh - 1] -= diff * value;
    }

    return true;
}


float Histogram::interpolate(float bin) const {
    float normalizedBin = (bin - _minValue) / (_maxValue - _minValue);
    float binIndex = normalizedBin * _numBins - 0.5; // Center

    float interpolator = binIndex - floor(binIndex);
    int binLow = floor(binIndex);
    int binHigh = ceil(binIndex);

    // Clamp bins
    if (binLow < 0) binLow = 0;
    if (binHigh >= _numBins) binHigh = _numBins - 1;

    return (1.0 - interpolator) * _data[binLow] + interpolator * _data[binHigh];
}

float Histogram::sample(int binIndex) const {
    assert(binIndex >= 0 && binIndex < _numBins);
    return _data[binIndex];
}


const float* Histogram::data() const {
    return _data;
}

std::vector<std::pair<float,float>> Histogram::getDecimated(int numBins) const {
    // Return a copy of _data decimated as in Ljung 2004
    return std::vector<std::pair<float,float>>();
}


void Histogram::normalize() {
    float sum = 0.0;
    for (int i = 0; i < _numBins; i++) {
        sum += _data[i];
    }
    for (int i = 0; i < _numBins; i++) {
        _data[i] /= sum;
    }
}

/*
 * Will create an internal array for histogram equalization.
 * Old histogram value is the index of the array, and the new equalized
 * value will be the value at the index.
 */
void Histogram::generateEqualizer() {

    _equalizer = std::vector<float>(_numBins, 0.0f);

    float previousCdf = 0.0f;

    for(int i = 0; i < _numBins; i++){

        float probability = _data[i] / (float)_numValues;

        float cdf  = previousCdf + probability;
        cdf = std::min(1.0f, cdf);
        _equalizer[i] = cdf * (_numBins-1);
        previousCdf = cdf;
    }
}

bool Histogram::setBin(int bin, float value) {
    if (bin < 0 || bin > _numBins) {
        LWARNING("setBin: bin is out of range");
        return false;
    }
    if(value < 0.0f){
        LWARNING("setBin: the value must not be negative");
        return false;   
    }
    _numValues -= _data[bin];
    _numValues += (int)value;
    _data[bin] = value;
    return true;
}

/*
 * Will return a equalized histogram
 */
Histogram Histogram::equalize(){
    Histogram equalizedHistogram(_minValue, _maxValue, _numBins);

    for(int i = 0; i < _numBins; i++){
        equalizedHistogram._data[(int)_equalizer[i]] += _data[i];
    }
    equalizedHistogram._numValues = _numValues;
    return std::move(equalizedHistogram);
}

/*
 * Given a value within the domain of this histogram (_minValue < value < maxValue),
 * this method will use its equalizer to return a histogram equalized result.
 */
float Histogram::equalize(float value) const {
    // if (value < _minValue || value > _maxValue) {
    //     LWARNING("Equalized value is is not within domain of histogram. min: " + std::to_string(_minValue) + " max: " + std::to_string(_maxValue) + " val: " + std::to_string(value));
    // }
    float normalizedValue = (value-_minValue)/(_maxValue-_minValue);
    int bin = floor(normalizedValue * _numBins);
    // If value == _maxValues then bin == _numBins, which is a invalid index.
    bin = std::min(_numBins-1, bin);
    bin = std::max(0 , bin);
    
    return _equalizer[bin];
}

float Histogram::entropy() const {
    float entropy = 0.f;

    for(int i = 0; i < _numBins; i++){
        if(_data[i] != 0)
            entropy -= ((float)_data[i]/_numValues) * log2((float)_data[i]/_numValues);
    }
    return entropy;
}

void Histogram::print() const {
    std::cout << "number of bins: " << _numBins << std::endl
              << "range: " << _minValue << " - " << _maxValue << std::endl << std::endl;
    for (int i = 0; i < _numBins; i++) {
        float low = _minValue + float(i) / _numBins * (_maxValue - _minValue);
        float high = low + (_maxValue - _minValue) / float(_numBins);
        std::cout << i << " [" << low << ", " << high << "]"
                  << "   " << _data[i] << std::endl;
        // std::cout << _data[i]/(float)_numValues << ", ";
    }
    std::cout << std::endl << std::endl << std::endl<< "==============" << std::endl;
}

int Histogram::highestBin() const {
    int highestBin = 0;
    float highestValue = 0.0f;
    for(int i=0; i < _numBins; i++){
        float value = _data[i];

        if(value > highestValue){
            highestBin = i;
            highestValue = value;
        }
    }

    return highestBin;
}

float Histogram::realBinValue(int bin) const {
    if (bin > _numBins || bin < 0) {
        LWARNING("realBinValue: bin value should be between 0 - number of bin");
    }
    float low = _minValue + (float(bin) / _numBins) * (_maxValue - _minValue);
    float high = low + (_maxValue - _minValue) / float(_numBins);
    return (high+low)/2.0;
}

float Histogram::binWidth(){
    return (_maxValue-_minValue) / float(_numBins);
}


std::ostream& operator<<(std::ostream& os, const Histogram& hist){
    int numBins = hist.numBins();
    int numValues = hist.numValues();
    const float* data = hist.data();

    for (int i = 0; i < numBins; i++) {
        // float low = _minValue + float(i) / _numBins * (_maxValue - _minValue);
        // float high = low + (_maxValue - _minValue) / float(_numBins);
        // std::cout << i << " [" << low << ", " << high << "]"
        //           << "   " << _data[i] << std::endl;
        os << data[i]/(float)numValues << ", ";
    }
    return os;
}
}
