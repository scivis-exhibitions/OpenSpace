#include <modules/skybrowser/include/wwtdatahandler.h>
#include <openspace/interaction/navigationhandler.h>
#include <openspace/engine/globals.h>
#include <openspace/util/camera.h>
#include <cmath> // For atan2
#include <glm/gtx/string_cast.hpp> // For printing glm data
#define _USE_MATH_DEFINES
#include <math.h>


namespace openspace {
    namespace skybrowser {
        const double SCREENSPACE_Z = -2.1;
        const double RAD_TO_DEG = 180.0 / M_PI;
        const double DEG_TO_RAD = M_PI / 180.0;
        const glm::dvec3 NORTH_POLE = { 0.0 , 0.0 , 1.0 };
        constexpr double infinity = std::numeric_limits<float>::max();

        // Conversion matrix from this paper: https://arxiv.org/abs/1010.3773v1
        const glm::dmat3 conversionMatrix = glm::dmat3({
                   -0.054875539390,  0.494109453633, -0.867666135681, // col 0
                   -0.873437104725, -0.444829594298, -0.198076389622, // col 1
                   -0.483834991775,  0.746982248696,  0.455983794523 // col 2
            });

        // J2000 to galactic conversion and vice versa
        glm::dvec2 cartesianToSpherical(glm::dvec3 cartesianCoords);
        glm::dvec3 sphericalToCartesian(glm::dvec2 sphericalCoords);
        glm::dvec3 galacticCartesianToJ2000Cartesian(glm::dvec3 rGal);
        glm::dvec2 galacticCartesianToJ2000Spherical(glm::dvec3 rGal);
        glm::dvec3 galacticCartesianToCameraLocalCartesian(glm::dvec3 galCoords);
        glm::dvec3 J2000SphericalToGalacticCartesian(glm::dvec2 coords,
            double distance = infinity);
        glm::dvec3 J2000CartesianToGalacticCartesian(glm::dvec3 coords,
            double distance = infinity);
        // Convert J2000, spherical or Cartesian, to screen space
        glm::dvec3 J2000SphericalToScreenSpace(glm::dvec2 coords);
        glm::dvec3 J2000CartesianToScreenSpace(glm::dvec3 coords);
        glm::dvec3 galacticToScreenSpace(glm::dvec3 galacticCoord);
        double calculateRoll(glm::dvec3 upWorld, glm::dvec3 forwardWorld);
    }
    namespace wwtmessage {
        // WWT messages
        ghoul::Dictionary moveCamera(const glm::dvec2 celestCoords,
            const double fov, const double roll, const bool moveInstantly = true);
        ghoul::Dictionary loadCollection(const std::string& url);
        ghoul::Dictionary setForeground(const std::string& name);
        ghoul::Dictionary createImageLayer(const std::string& imageUrl, const std::string& id);
        ghoul::Dictionary removeImageLayer(const std::string& imageId);
        ghoul::Dictionary setLayerOpacity(const std::string& imageId, double opacity);
        ghoul::Dictionary setForegroundOpacity(double val);
    }
}


    
    
