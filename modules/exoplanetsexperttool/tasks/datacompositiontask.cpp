/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2021                                                               *
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

#include <modules/exoplanetsexperttool/tasks/datacompositiontask.h>

#include <openspace/documentation/documentation.h>
#include <openspace/documentation/verifier.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/misc/csvreader.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/misc/dictionary.h>
#include <charconv>
#include <cmath>
#include <fstream>

namespace {
    constexpr const char _loggerCat[] = "ExoplanetsDataCompositionTask";

    constexpr const char KeyExoplanetsCsvFile[] = "InputExoplanetsCsvFile";
    constexpr const char KeyOutputLut[] = "OutputLUT";
    constexpr const char KeyOutputBin[] = "OutputBIN";

    constexpr const double EarthRadius = 6.3781e6; // meter

    // TODO: these should be moved to somewhere more general!
    float parseFloatData(const std::string& str) {
#ifdef WIN32
        float result;
        auto [p, ec] = std::from_chars(str.data(), str.data() + str.size(), result);
        if (ec == std::errc()) {
            return result;
        }
        return std::numeric_limits<float>::quiet_NaN();
#else
        // clang is missing float support for std::from_chars
        return !str.empty() ? std::stof(str.c_str(), nullptr) : NAN;
#endif
    };

    double parseDoubleData(const std::string& str) {
#ifdef WIN32
        double result;
        auto [p, ec] = std::from_chars(str.data(), str.data() + str.size(), result);
        if (ec == std::errc()) {
            return result;
        }
        return std::numeric_limits<double>::quiet_NaN();
#else
        // clang is missing double support for std::from_chars
        return !str.empty() ? std::stod(str.c_str(), nullptr) : NAN;
#endif
    };

    int parseIntegerData(const std::string& str) {
        int result;
        auto [p, ec] = std::from_chars(str.data(), str.data() + str.size(), result);
        if (ec == std::errc()) {
            return result;
        }
        return std::numeric_limits<int>::quiet_NaN();
    };
} // namespace

namespace openspace::exoplanets {

ExoplanetsDataCompositionTask::ExoplanetsDataCompositionTask(
                                                     const ghoul::Dictionary& dictionary)
{
    openspace::documentation::testSpecificationAndThrow(
        documentation(),
        dictionary,
        "ExoplanetsDataCompositionTask"
    );

    _inExoplanetsCsvPath = absPath(dictionary.value<std::string>(KeyExoplanetsCsvFile));
    _outputBinPath = absPath(dictionary.value<std::string>(KeyOutputBin));
    _outputLutPath = absPath(dictionary.value<std::string>(KeyOutputLut));
}

std::string ExoplanetsDataCompositionTask::description() {
    return ""; // @TODO
}

void ExoplanetsDataCompositionTask::perform(
                                          const Task::ProgressCallback& progressCallback)
{
    std::ifstream exoplanetsCsvFile(_inExoplanetsCsvPath);
    if (!exoplanetsCsvFile.good()) {
        LERROR(fmt::format("Failed to open input file '{}'", _inExoplanetsCsvPath));
        return;
    }

    std::ofstream binFile(_outputBinPath, std::ios::out | std::ios::binary);
    std::ofstream lutFile(_outputLutPath);

    int version = 1;
    binFile.write(reinterpret_cast<char*>(&version), sizeof(int));

    LINFO("Reading Exoplanets CSV");

    bool includeFirstLine = true;
    std::vector<std::vector<std::string>> csvContent = ghoul::loadCSVFile(
        _inExoplanetsCsvPath,
        includeFirstLine
    );

    if (csvContent.empty()) {
        LERROR(
            fmt::format("Could not read CSV data from file '{}'", _inExoplanetsCsvPath)
        );
        return;
    }

    // Write exoplanet records to file
   std::vector<std::string> columns = csvContent[0];

   int rowCounter = 0;
   const int nRows = csvContent.size();

   // TODO: keep track of already found planets
   std::vector<std::string> planets;

   for (int row = 1; row < nRows; row++) {
       ++rowCounter;
       progressCallback(static_cast<float>(rowCounter) / static_cast<float>(nRows));

       ExoplanetRecord p;
       std::string name;
       std::string component;
       std::string hostStar;

       for (int col = 0; col < columns.size(); col++) {
           const std::string& column = columns[col];
           const std::string& data = csvContent[row][col];

           if (column == "pl_name") {
               name = data;
               // TODO: create identifier matching exoplanets module?
           }
           else if (column == "hostname") {
               hostStar = data;
               // TODO: create identifier matching exoplanets module?
           }
           else if (column == "pl_letter") {
               component = data;
           }
           // System properties
           else if (column == "sy_snum") {
               p.nStars = parseIntegerData(data);
           }
           else if (column == "sy_pnum") {
               p.nPlanets = parseIntegerData(data);
           }
           else if (column == "disc_year") {
               p.discoveryYear = parseIntegerData(data);
           }
           // Planet properties
           else if (column == "pl_rade") {
               p.radius.value = parseDoubleData(data);
           }
           else if (column == "pl_masse") {
               p.mass.value = parseDoubleData(data);
           }
           else if (column == "pl_orbsmax") {
               p.semiMajorAxis.value = parseDoubleData(data);
           }
           // Star properties
           else if (column == "st_teff") {
               p.starEffectiveTemp.value = parseDoubleData(data);
           }
           else if (column == "st_rad") {
               p.starRadius.value = parseDoubleData(data);
           }
           else if (column == "sy_jmag") {
               p.magnitudeJ.value = parseDoubleData(data);
           }
           else if (column == "sy_kmag") {
               p.magnitudeK.value = parseDoubleData(data);
           }
       }

       p.multiSystemFlag = (p.nPlanets > 1);

       // If uknown, compute planet mass
       if ((!p.mass.hasValue()) && p.radius.hasValue()) {
           double r = p.radius.value;

           // Mass radius relationship from Chen & Kipping (2017)
           // See eq. (2) in https://arxiv.org/pdf/1805.03671.pdf
           if (r < 1.23) {
               p.mass.value = 0.9718 * glm::pow(r, 3.58);
           }
           else if (r < 14.26) {
               p.mass.value = 1.436 * glm::pow(r, 1.70);
           }
       }

       // Compute planet equilibrium temperature according to eq. (3) in
       // https://arxiv.org/pdf/1805.03671.pdf
       bool hasStarTempInfo = p.starEffectiveTemp.hasValue() && p.starRadius.hasValue();
       if (hasStarTempInfo && p.semiMajorAxis.hasValue()) {
           double tempStar = p.starEffectiveTemp.value;
           double rStar = p.starRadius.value;
           double a = p.semiMajorAxis.value;
           a *= 214.93946938362; // convert to Solar radii (same as star)

           const double c = glm::pow(0.25, 0.25);
           p.eqilibriumTemp.value = c * tempStar * glm::sqrt((rStar / a));
       }

       // @TODO: compute transmission and emission metrics (TSM and ESM)
       // (eq. 1 and 4 in https://arxiv.org/pdf/1805.03671.pdf)
       p.tsm = computeTSM(p);
       p.esm = computeESM(p);

       // For now, only include the planets with TSM values
       if (std::isnan(p.tsm)) {
           continue;
       }

       // @TODO: also compute planet surface gravity?

       // @OBS! For now, don't add duplicates. But @TODO: ask about whether we are
       // actually interested in multiple entries on the same planet. If we are not,
       // then we should use the composite dataset instead
       if (std::find(planets.begin(), planets.end(), name) != planets.end()) {
           continue;
       }

       planets.push_back(name);

       // Create look-up table and write data to file
       long pos = static_cast<long>(binFile.tellp());
       lutFile << name << "," << pos;
       lutFile << "," << hostStar << "," << component; // Also write star name and component in lookuptable
       lutFile << std::endl;
       // TODO: separate data file and lookup table for stars? Depends on whether we want
       // to be able to filter stars, I guess

       binFile.write(reinterpret_cast<char*>(&p), sizeof(ExoplanetRecord));
   }

    progressCallback(1.f);
}

documentation::Documentation ExoplanetsDataCompositionTask::documentation() {
    using namespace documentation;
    return {
        "ExoplanetsDataCompositionTask",
        "exoplanets_data_composition_task",
        {
            {
                KeyExoplanetsCsvFile,
                new FileVerifier,
                Optional::No,
                "The NASA Exoplanets Archive CSV file"
            },
            {
                KeyOutputLut,
                // File verifier cannot be used here. Maybe implement a variant?
                new StringAnnotationVerifier("A valid filepath"),
                Optional::No,
                "The txt file to write the output look-up table into"
            },
            {
                KeyOutputBin,
                // Same here
                new StringAnnotationVerifier("A valid filepath"),
                Optional::No,
                "The bin file to export the planet data into"
            }
        }
    };
}

float ExoplanetsDataCompositionTask::computeTSM(const ExoplanetRecord& p) {
    bool hasMissingValue = !(p.radius.hasValue()
        && p.mass.hasValue() && p.eqilibriumTemp.hasValue()
        && p.starRadius.hasValue() && p.magnitudeJ.hasValue());

    if (hasMissingValue) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    // Scale factor based on table 1 in Kempton et al. (2018)
    // planetRadius is in Earth radii
    auto scaleFactor = [](double planetRadius) {
        if (planetRadius < 1.5) {
            return 0.19;
        }
        else if (planetRadius < 2.75) {
            return 1.26;
        }
        else if (planetRadius < 4.0) {
            return 1.28;
        }
        else { // 4.0 < r  < 10 EarthRadius
            return 1.15;
        }
        // @TODO: OBS! What about larger planets??
    };

    const double rPlanet = p.radius.value;
    const double mass = p.mass.value;
    const double temp = p.eqilibriumTemp.value;
    const double rStar = p.starRadius.value;
    const double mJ = p.magnitudeJ.value;

    const double rPlanet3 = rPlanet * rPlanet * rPlanet;
    const double rStar2 = rStar * rStar;

    double tsm = (rPlanet3 * temp) / (mass * rStar2);
    tsm *= glm::pow(10.0, -mJ / 5.0);
    tsm *= scaleFactor(rPlanet);

    return static_cast<float>(tsm);
}

float ExoplanetsDataCompositionTask::computeESM(const ExoplanetRecord& p) {
    bool hasMissingValue = !(p.radius.hasValue()
        && p.eqilibriumTemp.hasValue() && p.starEffectiveTemp.hasValue()
        && p.starRadius.hasValue() && p.magnitudeK.hasValue());

    if (hasMissingValue) {
        return std::numeric_limits<float>::quiet_NaN();
    }


    const double rPlanet = p.radius.value;
    const double tempPlanetDay = 1.10 * p.eqilibriumTemp.value;
    const double rStar = p.starRadius.value;
    const double teffStar = p.starEffectiveTemp.value;
    const double mK = p.magnitudeK.value;

    constexpr const double earthToSolar = 0.0091577;
    const double normalizedPlanetRadius = (rPlanet * earthToSolar) / rStar;

    // Plack's function computes the energy emitted per second per unit lambda wavelength
    // per steradian from one square meter of a perfect blackbody at temperature T
    // http://spiff.rit.edu/classes/phys317/lectures/planck.html
    auto plancksFunction = [](double T, double lambda) {
        constexpr const double h = 6.62607015e-34; // Planck's constant
        constexpr const double c = 299792458; // Speed of light
        constexpr const double k = 1.38064852e-23; // Boltzmann's constant

        const double nom = 2.0 * h * c * c / std::pow(lambda, 5.0);
        const double denom = std::exp((h * c) / (lambda * k * T)) - 1.0;
        return nom / denom;
    };

    const double lambda = 7.5 * 10e-6; // micrometer

    double esm = 4.29 * 10e6;
    esm *= plancksFunction(tempPlanetDay, lambda) / plancksFunction(teffStar, lambda);
    esm *= std::pow(normalizedPlanetRadius, 2.0);
    esm *= std::pow(10.0, -mK / 5.0);
    return static_cast<float>(esm);
}

} // namespace openspace::exoplanets
