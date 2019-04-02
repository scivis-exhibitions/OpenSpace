/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2019                                                               *
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

#include <modules/space/rendering/elonstest.h>
#include <ghoul/logging/logmanager.h>

#include <ghoul/filesystem/filesystem.h>
#include <modules/space/spacemodule.h>
#include <modules/space/translation/keplertranslation.h>
#include <openspace/documentation/documentation.h>
#include <openspace/documentation/verifier.h>
#include <openspace/engine/globals.h>
#include <openspace/rendering/renderengine.h>


#include <fstream>
#include <vector>

namespace {
    constexpr const char* ProgramName = "ElonsTest";
    constexpr const char* _loggerCat = "SpaceDebris";

   
    static const openspace::properties::Property::PropertyInfo PathInfo = {
        "Path",
        "Path",
        "The file path to the CSV file to read"
    };
    
    static const openspace::properties::Property::PropertyInfo SegmentsInfo = {
        "Segments",
        "Segments",
        "The number of segments to use for each orbit ellipse"
    };
    
    static const openspace::properties::Property::PropertyInfo EccentricityColumnInfo = {
        "EccentricityColumn",
        "EccentricityColumn",
        "The header of the column where the eccentricity is stored"
    };
    
    static const openspace::properties::Property::PropertyInfo SemiMajorAxisColumnInfo = {
        "SemiMajorAxisColumn",
        "SemiMajorAxisColumn",
        "The header of the column where the semi-major axis is stored"
    };
    
    static const openspace::properties::Property::PropertyInfo SemiMajorAxisUnitInfo = {
        "SemiMajorAxisUnit",
        "SemiMajorAxisUnit",
        "The unit of the semi major axis. For example: If specified in km, set this to 1000."
    };
    
    static const openspace::properties::Property::PropertyInfo InclinationColumnInfo = {
        "InclinationColumn",
        "InclinationColumn",
        "The header of the column where the inclination is stored"
    };
    
    static const openspace::properties::Property::PropertyInfo AscendingNodeColumnInfo = {
        "AscendingNodeColumn",
        "AscendingNodeColumn",
        "The header of the column where the ascending node is stored"
    };
    
    static const openspace::properties::Property::PropertyInfo ArgumentOfPeriapsisColumnInfo = {
        "ArgumentOfPeriapsisColumn",
        "ArgumentOfPeriapsisColumn",
        "The header of the column where the argument of periapsis is stored"
    };
    
    static const openspace::properties::Property::PropertyInfo MeanAnomalyAtEpochColumnInfo = {
        "MeanAnomalyAtEpochColumn",
        "MeanAnomalyAtEpochColumn",
        "The header of the column where the mean anomaly at epoch is stored"
    };
    
    static const openspace::properties::Property::PropertyInfo EpochColumnInfo = {
        "EpochColumn",
        "EpochColumn",
        "The header of the column where the epoch is stored"
    };

    constexpr const char* KeyFile = "File";
    constexpr const char* KeyLineNumber = "LineNumber";

    

} // namespace

namespace openspace {

    // The list of leap years only goes until 2056 as we need to touch this file then
    // again anyway ;)
    const std::vector<int> LeapYears = {
        1956, 1960, 1964, 1968, 1972, 1976, 1980, 1984, 1988, 1992, 1996,
        2000, 2004, 2008, 2012, 2016, 2020, 2024, 2028, 2032, 2036, 2040,
        2044, 2048, 2052, 2056
    };

    // Count the number of full days since the beginning of 2000 to the beginning of
    // the parameter 'year'
    int countDays(int year) {
        // Find the position of the current year in the vector, the difference
        // between its position and the position of 2000 (for J2000) gives the
        // number of leap years
        constexpr const int Epoch = 2000;
        constexpr const int DaysRegularYear = 365;
        constexpr const int DaysLeapYear = 366;

        if (year == Epoch) {
            return 0;
        }

        // Get the position of the most recent leap year
        const auto lb = std::lower_bound(LeapYears.begin(), LeapYears.end(), year);

        // Get the position of the epoch
        const auto y2000 = std::find(LeapYears.begin(), LeapYears.end(), Epoch);

        // The distance between the two iterators gives us the number of leap years
        const int nLeapYears = static_cast<int>(std::abs(std::distance(y2000, lb)));

        const int nYears = std::abs(year - Epoch);
        const int nRegularYears = nYears - nLeapYears;

        // Get the total number of days as the sum of leap years + non leap years
        const int result = nRegularYears * DaysRegularYear + nLeapYears * DaysLeapYear;
        return result;
    }

    // Returns the number of leap seconds that lie between the {year, dayOfYear}
    // time point and { 2000, 1 }
    int countLeapSeconds(int year, int dayOfYear) {
        // Find the position of the current year in the vector; its position in
        // the vector gives the number of leap seconds
        struct LeapSecond {
            int year;
            int dayOfYear;
            bool operator<(const LeapSecond& rhs) const {
                return std::tie(year, dayOfYear) < std::tie(rhs.year, rhs.dayOfYear);
            }
        };

        const LeapSecond Epoch = { 2000, 1 };

        // List taken from: https://www.ietf.org/timezones/data/leap-seconds.list
        static const std::vector<LeapSecond> LeapSeconds = {
            { 1972,   1 },
            { 1972, 183 },
            { 1973,   1 },
            { 1974,   1 },
            { 1975,   1 },
            { 1976,   1 },
            { 1977,   1 },
            { 1978,   1 },
            { 1979,   1 },
            { 1980,   1 },
            { 1981, 182 },
            { 1982, 182 },
            { 1983, 182 },
            { 1985, 182 },
            { 1988,   1 },
            { 1990,   1 },
            { 1991,   1 },
            { 1992, 183 },
            { 1993, 182 },
            { 1994, 182 },
            { 1996,   1 },
            { 1997, 182 },
            { 1999,   1 },
            { 2006,   1 },
            { 2009,   1 },
            { 2012, 183 },
            { 2015, 182 },
            { 2017,   1 }
        };

        // Get the position of the last leap second before the desired date
        LeapSecond date { year, dayOfYear };
        const auto it = std::lower_bound(LeapSeconds.begin(), LeapSeconds.end(), date);

        // Get the position of the Epoch
        const auto y2000 = std::lower_bound(
            LeapSeconds.begin(),
            LeapSeconds.end(),
            Epoch
        );

        // The distance between the two iterators gives us the number of leap years
        const int nLeapSeconds = static_cast<int>(std::abs(std::distance(y2000, it)));
        return nLeapSeconds;
    }

    double epochFromSubstring(const std::string& epochString) {
        // The epochString is in the form:
        // YYDDD.DDDDDDDD
        // With YY being the last two years of the launch epoch, the first DDD the day
        // of the year and the remaning a fractional part of the day

        // The main overview of this function:
        // 1. Reconstruct the full year from the YY part
        // 2. Calculate the number of seconds since the beginning of the year
        // 2.a Get the number of full days since the beginning of the year
        // 2.b If the year is a leap year, modify the number of days
        // 3. Convert the number of days to a number of seconds
        // 4. Get the number of leap seconds since January 1st, 2000 and remove them
        // 5. Adjust for the fact the epoch starts on 1st Januaray at 12:00:00, not
        // midnight

        // According to https://celestrak.com/columns/v04n03/
        // Apparently, US Space Command sees no need to change the two-line element
        // set format yet since no artificial earth satellites existed prior to 1957.
        // By their reasoning, two-digit years from 57-99 correspond to 1957-1999 and
        // those from 00-56 correspond to 2000-2056. We'll see each other again in 2057!

        // 1. Get the full year
        std::string yearPrefix = [y = epochString.substr(0, 2)](){
            int year = std::atoi(y.c_str());
            return year >= 57 ? "19" : "20";
        }();
        const int year = std::atoi((yearPrefix + epochString.substr(0, 2)).c_str());
        const int daysSince2000 = countDays(year);

        // 2.
        // 2.a
        double daysInYear = std::atof(epochString.substr(2).c_str());

        // 2.b
        const bool isInLeapYear = std::find(
            LeapYears.begin(),
            LeapYears.end(),
            year
        ) != LeapYears.end();
        if (isInLeapYear && daysInYear >= 60) {
            // We are in a leap year, so we have an effective day more if we are
            // beyond the end of february (= 31+29 days)
            --daysInYear;
        }

        // 3
        using namespace std::chrono;
        const int SecondsPerDay = static_cast<int>(seconds(hours(24)).count());
        //Need to subtract 1 from daysInYear since it is not a zero-based count
        const double nSecondsSince2000 = (daysSince2000 + daysInYear - 1) * SecondsPerDay;

        // 4
        // We need to remove additionbal leap seconds past 2000 and add them prior to
        // 2000 to sync up the time zones
        const double nLeapSecondsOffset = -countLeapSeconds(
            year,
            static_cast<int>(std::floor(daysInYear))
        );

        // 5
        const double nSecondsEpochOffset = static_cast<double>(
            seconds(hours(12)).count()
        );

        // Combine all of the values
        const double epoch = nSecondsSince2000 + nLeapSecondsOffset - nSecondsEpochOffset;
        return epoch;
    }

    double calculateSemiMajorAxis(double meanMotion) {
        constexpr const double GravitationalConstant = 6.6740831e-11;
        constexpr const double MassEarth = 5.9721986e24;
        constexpr const double muEarth = GravitationalConstant * MassEarth;

        // Use Kepler's 3rd law to calculate semimajor axis
        // a^3 / P^2 = mu / (2pi)^2
        // <=> a = ((mu * P^2) / (2pi^2))^(1/3)
        // with a = semimajor axis
        // P = period in seconds
        // mu = G*M_earth
        double period = std::chrono::seconds(std::chrono::hours(24)).count() / meanMotion;

        const double pisq = glm::pi<double>() * glm::pi<double>();
        double semiMajorAxis = pow((muEarth * period*period) / (4 * pisq), 1.0 / 3.0);

        // We need the semi major axis in km instead of m
        return semiMajorAxis / 1000.0;
    }

    documentation::Documentation ElonsTest::Documentation() {
        using namespace documentation;
        return {
            "ElonsTest",
            "space_elons_test",
            {
                {
                    SegmentsInfo.identifier,
                    new DoubleVerifier,
                    Optional::No,
                    SegmentsInfo.description
                },
                {
                    PathInfo.identifier,
                    new StringVerifier,
                    Optional::No,
                    PathInfo.description
                },
                {
                    EccentricityColumnInfo.identifier,
                    new StringVerifier,
                    Optional::No,
                    EccentricityColumnInfo.description
                },
                {
                    SemiMajorAxisColumnInfo.identifier,
                    new StringVerifier,
                    Optional::No,
                    SemiMajorAxisColumnInfo.description
                },
                {
                    SemiMajorAxisUnitInfo.identifier,
                    new DoubleVerifier,
                    Optional::No,
                    SemiMajorAxisUnitInfo.description
                },
                {
                    InclinationColumnInfo.identifier,
                    new StringVerifier,
                    Optional::No,
                    InclinationColumnInfo.description
                },
                {
                    AscendingNodeColumnInfo.identifier,
                    new StringVerifier,
                    Optional::No,
                    AscendingNodeColumnInfo.description
                },
                {
                    ArgumentOfPeriapsisColumnInfo.identifier,
                    new StringVerifier,
                    Optional::No,
                    ArgumentOfPeriapsisColumnInfo.description
                },
                {
                    MeanAnomalyAtEpochColumnInfo.identifier,
                    new StringVerifier,
                    Optional::No,
                    MeanAnomalyAtEpochColumnInfo.description
                },
                {
                    EpochColumnInfo.identifier,
                    new StringVerifier,
                    Optional::No,
                    EpochColumnInfo.description
                }
            }
        };
    }    

    ElonsTest::ElonsTest(const ghoul::Dictionary& dictionary)
        : Renderable(dictionary)
        , _path(PathInfo)
        , _nSegments(SegmentsInfo)
        , _eccentricityColumnName(EccentricityColumnInfo)
        , _semiMajorAxisColumnName(SemiMajorAxisColumnInfo)
        , _semiMajorAxisUnit(SemiMajorAxisUnitInfo)
        , _inclinationColumnName(InclinationColumnInfo)
        , _ascendingNodeColumnName(AscendingNodeColumnInfo)
        , _argumentOfPeriapsisColumnName(ArgumentOfPeriapsisColumnInfo)
        , _meanAnomalyAtEpochColumnName(MeanAnomalyAtEpochColumnInfo)
        , _epochColumnName(EpochColumnInfo)
    {
        documentation::testSpecificationAndThrow(
            Documentation(),
            dictionary,
            "ElonsTest"
        );

        _path =
            dictionary.value<std::string>(PathInfo.identifier);
        _nSegments =
            static_cast<int>(dictionary.value<double>(SegmentsInfo.identifier));
        _eccentricityColumnName =
            dictionary.value<std::string>(EccentricityColumnInfo.identifier);
        _semiMajorAxisColumnName =
            dictionary.value<std::string>(SemiMajorAxisColumnInfo.identifier);
        _semiMajorAxisUnit =
            dictionary.value<double>(SemiMajorAxisUnitInfo.identifier);
        _inclinationColumnName =
            dictionary.value<std::string>(InclinationColumnInfo.identifier);
        _ascendingNodeColumnName =
            dictionary.value<std::string>(AscendingNodeColumnInfo.identifier);
        _argumentOfPeriapsisColumnName =
            dictionary.value<std::string>(ArgumentOfPeriapsisColumnInfo.identifier);
        _meanAnomalyAtEpochColumnName =
            dictionary.value<std::string>(MeanAnomalyAtEpochColumnInfo.identifier);
        _epochColumnName =
            dictionary.value<std::string>(EpochColumnInfo.identifier);

        addProperty(_path);
        addProperty(_nSegments);
        // addProperty(_semiMajorAxisUnit);

     
        const std::string& file = dictionary.value<std::string>(KeyFile);
        readTLEFile(file);

        // !TLE
    } // !constructor
    // uses Renderables destructor?

    void ElonsTest::readTLEFile(const std::string& filename) {
        ghoul_assert(FileSys.fileExists(filename), "The filename must exist");

        std::ifstream file;
        file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
        file.open(filename);
    
        // All of the Kepler element information
        struct KeplerParameters{
            double inclination = 0.0;
            double semiMajorAxis = 0.0;
            double ascendingNode = 0.0;
            double eccentricity = 0.0;
            double argumentOfPeriapsis = 0.0;
            double meanAnomaly = 0.0;
            double meanMotion = 0.0;
            double epoch = 0.0;
        };

        std::vector<KeplerParameters> TLEData;

        // int numberOfLines = std::count(std::istreambuf_iterator<char>(file), 
        //                               std::istreambuf_iterator<char>(), '\n' );
        // 3 because a TLE has 3 lines per element/ object.
        // int numberOfObjects = numberOfLines/3;
        // LINFO("Number of data elements: " + numberOfObjects);

        // for(int i=0 ; i<numberOfObjects; ++i){
            

        //     TLEData.push_back();

        // }


        std::string line;
        while(std::getline(file, line)) {

            KeplerParameters keplerElements;
            
            std::getline(file, line);
            if (line[0] == '1') {
                // First line
                // Field Columns   Content
                //     1   01-01   Line number
                //     2   03-07   Satellite number
                //     3   08-08   Classification (U = Unclassified)
                //     4   10-11   International Designator (Last two digits of launch year)
                //     5   12-14   International Designator (Launch number of the year)
                //     6   15-17   International Designator(piece of the launch)    A
                //     7   19-20   Epoch Year(last two digits of year)
                //     8   21-32   Epoch(day of the year and fractional portion of the day)
                //     9   34-43   First Time Derivative of the Mean Motion divided by two
                //    10   45-52   Second Time Derivative of Mean Motion divided by six
                //    11   54-61   BSTAR drag term(decimal point assumed)[10] - 11606 - 4
                //    12   63-63   The "Ephemeris type"
                //    13   65-68   Element set  number.Incremented when a new TLE is generated
                //    14   69-69   Checksum (modulo 10)
                keplerElements.epoch = epochFromSubstring(line.substr(18, 14));
            } else {
                throw ghoul::RuntimeError(fmt::format(
                    "File {} @ line {} does not have '1' header", filename // linNum + 1
                ));
            }
            std::getline(file, line);
            if (line[0] == '2') {
                // Second line
                // Field    Columns   Content
                //     1      01-01   Line number
                //     2      03-07   Satellite number
                //     3      09-16   Inclination (degrees)
                //     4      18-25   Right ascension of the ascending node (degrees)
                //     5      27-33   Eccentricity (decimal point assumed)
                //     6      35-42   Argument of perigee (degrees)
                //     7      44-51   Mean Anomaly (degrees)
                //     8      53-63   Mean Motion (revolutions per day)
                //     9      64-68   Revolution number at epoch (revolutions)
                //    10      69-69   Checksum (modulo 10)

                std::stringstream stream;
                stream.exceptions(std::ios::failbit);

                // Get inclination
                stream.str(line.substr(8, 8));
                stream >> keplerElements.inclination;
                stream.clear();

                // Get Right ascension of the ascending node
                stream.str(line.substr(17, 8));
                stream >> keplerElements.ascendingNode;
                stream.clear();

                // Get Eccentricity
                stream.str("0." + line.substr(26, 7));
                stream >> keplerElements.eccentricity;
                stream.clear();

                // Get argument of periapsis
                stream.str(line.substr(34, 8));
                stream >> keplerElements.argumentOfPeriapsis;
                stream.clear();

                // Get mean anomaly
                stream.str(line.substr(43, 8));
                stream >> keplerElements.meanAnomaly;
                stream.clear();

                // Get mean motion
                stream.str(line.substr(52, 11));
                stream >> keplerElements.meanMotion;
            } else {
                throw ghoul::RuntimeError(fmt::format(
                    "File {} @ line {} does not have '2' header", filename  // , lineNum + 2
                ));
            }

            // Calculate the semi major axis based on the mean motion using kepler's laws
            keplerElements.semiMajorAxis = calculateSemiMajorAxis(keplerElements.meanMotion);
        
            // Converting the mean motion (revolutions per day) to period (seconds per revolution)
            using namespace std::chrono;
            double period = seconds(hours(24)).count() / keplerElements.meanMotion;

            _keplerTranslator.setKeplerElements(
                keplerElements.eccentricity,
                keplerElements.semiMajorAxis,
                keplerElements.inclination,
                keplerElements.ascendingNode,
                keplerElements.argumentOfPeriapsis,
                keplerElements.meanAnomaly,
                period,
                keplerElements.epoch
            );

            TLEData.push_back(keplerElements); 
        } // !while loop
        
        file.close();
    }

    void ElonsTest::initialize(){
            //Fyll _vertexArray i init och 
            // rendera bara orbits, inga rörliga delar.









    }

    void ElonsTest::initializeGL() {
        _programObject = SpaceModule::ProgramObjectManager.request(
            ProgramName,
            []() -> std::unique_ptr<ghoul::opengl::ProgramObject> {
                return global::renderEngine.buildRenderProgram(
                    ProgramName,
                    absPath("${MODULE_SPACE}/shaders/renderablekeplerorbits_vs.glsl"),
                    absPath("${MODULE_SPACE}/shaders/renderablekeplerorbits_fs.glsl")
                );
            }
        );

    }

    void ElonsTest::deinitializeGL() {




    }

    void ElonsTest::render(const RenderData& data, RendererTasks& rendererTask)  {
        _programObject->activate();
        LINFO("render data: ", data);

    }

    void ElonsTest::update(const UpdateData& data) {

    }

    bool ElonsTest::isReady() const {
        return true;
    }

}