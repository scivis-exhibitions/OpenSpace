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

#include <modules/globebrowsing/src/tileprovider.h>

#include <modules/globebrowsing/globebrowsingmodule.h>
#include <modules/globebrowsing/src/asynctiledataprovider.h>
#include <modules/globebrowsing/src/geodeticpatch.h>
#include <modules/globebrowsing/src/layermanager.h>
#include <modules/globebrowsing/src/memoryawaretilecache.h>
#include <modules/globebrowsing/src/rawtiledatareader.h>
#include <openspace/engine/globals.h>
#include <openspace/engine/moduleengine.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/util/factorymanager.h>
#include <openspace/util/timemanager.h>
#include <openspace/util/spicemanager.h>
#include <ghoul/filesystem/file.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/font/fontmanager.h>
#include <ghoul/font/fontrenderer.h>
#include <ghoul/io/texture/texturereader.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/misc/profiling.h>
#include <ghoul/opengl/openglstatecache.h>
#include <filesystem>
#include <fstream>
#include "cpl_minixml.h"

namespace ghoul {
    template <>
    constexpr openspace::globebrowsing::tileprovider::TemporalTileProvider::TimeFormatType
        from_string(std::string_view string)
    {
        using namespace openspace::globebrowsing::tileprovider;
        if (string == "YYYY-MM-DD") {
            return TemporalTileProvider::TimeFormatType::YYYY_MM_DD;
        }
        else if (string == "YYYY-MM-DDThh:mm:ssZ") {
            return TemporalTileProvider::TimeFormatType::YYYY_MM_DDThhColonmmColonssZ;
        }
        else if (string == "YYYY-MM-DDThh_mm_ssZ") {
            return TemporalTileProvider::TimeFormatType::YYYY_MM_DDThh_mm_ssZ;
        }
        else if (string == "YYYYMMDD_hhmmss") {
            return TemporalTileProvider::TimeFormatType::YYYYMMDD_hhmmss;
        }
        else if (string == "YYYYMMDD_hhmm") {
            return TemporalTileProvider::TimeFormatType::YYYYMMDD_hhmm;
        }
        else {
            throw ghoul::RuntimeError("Unknown timeformat '" + std::string(string) + "'");
        }
    }
} // namespace ghoul


namespace openspace::globebrowsing::tileprovider {

namespace {

std::unique_ptr<ghoul::opengl::Texture> DefaultTileTexture;
Tile DefaultTile = Tile { nullptr, std::nullopt, Tile::Status::Unavailable };

constexpr const char* KeyFilePath = "FilePath";

namespace defaultprovider {
    constexpr const char* KeyPerformPreProcessing = "PerformPreProcessing";
    constexpr const char* KeyTilePixelSize = "TilePixelSize";
    constexpr const char* KeyPadTiles = "PadTiles";

    constexpr openspace::properties::Property::PropertyInfo FilePathInfo = {
        "FilePath",
        "File Path",
        "The path of the GDAL file or the image file that is to be used in this tile "
        "provider."
    };

    constexpr openspace::properties::Property::PropertyInfo TilePixelSizeInfo = {
        "TilePixelSize",
        "Tile Pixel Size",
        "This value is the preferred size (in pixels) for each tile. Choosing the right "
        "value is a tradeoff between more efficiency (larger images) and better quality "
        "(smaller images). The tile pixel size has to be smaller than the size of the "
        "complete image if a single image is used."
    };
} // namespace defaultprovider

namespace singleimageprovider {
    constexpr openspace::properties::Property::PropertyInfo FilePathInfo = {
        "FilePath",
        "File Path",
        "The file path that is used for this image provider. The file must point to an "
        "image that is then loaded and used for all tiles."
    };
} // namespace singleimageprovider

namespace sizereferenceprovider {
    constexpr const char* KeyRadii = "Radii";
} // namespace sizereferenceprovider

namespace byindexprovider {
    constexpr const char* KeyDefaultProvider = "DefaultProvider";
    constexpr const char* KeyProviders = "IndexTileProviders";
    constexpr const char* KeyTileIndex = "TileIndex";
    constexpr const char* KeyTileProvider = "TileProvider";
} // namespace byindexprovider

namespace bylevelprovider {
    constexpr const char* KeyProviders = "LevelTileProviders";
    constexpr const char* KeyMaxLevel = "MaxLevel";
    constexpr const char* KeyTileProvider = "TileProvider";
    constexpr const char* KeyLayerGroupID = "LayerGroupID";
} // namespace bylevelprovider

namespace temporal {
    constexpr const char* KeyBasePath = "BasePath";

    constexpr const char* UrlTimePlaceholder = "${OpenSpaceTimeId}";
    constexpr const char* TimeStart = "OpenSpaceTimeStart";
    constexpr const char* TimeEnd = "OpenSpaceTimeEnd";
    constexpr const char* TimeResolution = "OpenSpaceTimeResolution";
    constexpr const char* TimeFormat = "OpenSpaceTimeIdFormat";

    constexpr openspace::properties::Property::PropertyInfo FilePathInfo = {
        "FilePath",
        "File Path",
        "This is the path to the XML configuration file that describes the temporal tile "
        "information."
    };

    constexpr openspace::properties::Property::PropertyInfo UseFixedTimeInfo = {
        "UseFixedTime",
        "Use Fixed Time",
        "If this value is enabled, the time-varying timevarying dataset will always use "
        "the time that is specified in the 'FixedTime' property, rather than using the "
        "actual time from OpenSpace"
    };

    constexpr openspace::properties::Property::PropertyInfo FixedTimeInfo = {
        "FixedTime",
        "Fixed Time",
        "If the 'UseFixedTime' is enabled, this time will be used instead of the actual "
        "time taken from OpenSpace for the displayed tiles."
    };
} // namespace temporal


//
// DefaultTileProvider
//

void initAsyncTileDataReader(DefaultTileProvider& t, TileTextureInitData initData) {
    ZoneScoped

    t.asyncTextureDataProvider = std::make_unique<AsyncTileDataProvider>(
        t.name,
        std::make_unique<RawTileDataReader>(
            t.filePath,
            initData,
            RawTileDataReader::PerformPreprocessing(t.performPreProcessing)
        )
    );
}

bool initTexturesFromLoadedData(DefaultTileProvider& t) {
    ZoneScoped

    if (t.asyncTextureDataProvider) {
        std::optional<RawTile> tile = t.asyncTextureDataProvider->popFinishedRawTile();
        if (tile) {
            const cache::ProviderTileKey key = { tile->tileIndex, t.uniqueIdentifier };
            ghoul_assert(!t.tileCache->exist(key), "Tile must not be existing in cache");
            t.tileCache->createTileAndPut(key, std::move(*tile));
            return true;
        }
    }
    return false;
}


//
// TextTileProvider
//

void initialize(TextTileProvider& t) {
    ZoneScoped

    t.font = global::fontManager->font("Mono", static_cast<float>(t.fontSize));
    t.fontRenderer = ghoul::fontrendering::FontRenderer::createDefault();
    t.fontRenderer->setFramebufferSize(glm::vec2(t.initData.dimensions));
    glGenFramebuffers(1, &t.fbo);
}

void deinitialize(TextTileProvider& t) {
    glDeleteFramebuffers(1, &t.fbo);
}

Tile tile(TextTileProvider& t, const TileIndex& tileIndex) {
    ZoneScoped
    TracyGpuZone("tile")

    cache::ProviderTileKey key = { tileIndex, t.uniqueIdentifier };
    Tile tile = t.tileCache->get(key);
    if (!tile.texture) {
        ghoul::opengl::Texture* texture = t.tileCache->texture(t.initData);

        // Keep track of defaultFBO and viewport to be able to reset state when done
        GLint defaultFBO;
        defaultFBO = global::renderEngine->openglStateCache().defaultFramebuffer();

        // Render to texture
        glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            *texture,
            0
        );

        GLsizei w = static_cast<GLsizei>(texture->width());
        GLsizei h = static_cast<GLsizei>(texture->height());
        glViewport(0, 0, w, h);
        glClearColor(0.f, 0.f, 0.f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT);

        t.fontRenderer->render(*t.font, t.textPosition, t.text, t.textColor);

        glBindFramebuffer(GL_FRAMEBUFFER, defaultFBO);
        global::renderEngine->openglStateCache().resetViewportState();

        tile = Tile{ texture, std::nullopt, Tile::Status::OK };
        t.tileCache->put(key, t.initData.hashKey, tile);
    }
    return tile;
}

void reset(TextTileProvider& t) {
    ZoneScoped

    t.tileCache->clear();
}


//
// TileProviderByLevel
//

TileProvider* levelProvider(TileProviderByLevel& t, int level) {
    ZoneScoped

    if (!t.levelTileProviders.empty()) {
        int clampedLevel = glm::clamp(
            level,
            0,
            static_cast<int>(t.providerIndices.size() - 1)
        );
        int idx = t.providerIndices[clampedLevel];
        return t.levelTileProviders[idx].get();
    }
    else {
        return nullptr;
    }
}


//
// TemporalTileProvider
//

// Buffer needs at least 22 characters space
int timeStringify(TemporalTileProvider::TimeFormatType type, const Time& t, char* buffer)
{
    ZoneScoped

    std::memset(buffer, 0, 22);
    const double time = t.j2000Seconds();

    switch (type) {
        case TemporalTileProvider::TimeFormatType::YYYY_MM_DD:
        {
            constexpr const char Format[] = "YYYY-MM-DD";
            constexpr const int Size = sizeof(Format);
            SpiceManager::ref().dateFromEphemerisTime(time, buffer, Size, Format);
            return Size - 1;
        }
        case TemporalTileProvider::TimeFormatType::YYYYMMDD_hhmmss: {
            constexpr const char Format[] = "YYYYMMDD_HRMNSC";
            constexpr const int Size = sizeof(Format);
            SpiceManager::ref().dateFromEphemerisTime(time, buffer, Size, Format);
            return Size - 1;
        }
        case TemporalTileProvider::TimeFormatType::YYYYMMDD_hhmm: {
            constexpr const char Format[] = "YYYYMMDD_HRMN";
            constexpr const int Size = sizeof(Format);
            SpiceManager::ref().dateFromEphemerisTime(time, buffer, Size, Format);
            return Size - 1;
        }
        case TemporalTileProvider::TimeFormatType::YYYY_MM_DDThhColonmmColonssZ:
        {
            constexpr const char Format[] = "YYYY-MM-DDTHR:MN:SCZ";
            constexpr const int Size = sizeof(Format);
            SpiceManager::ref().dateFromEphemerisTime(time, buffer, Size, Format);
            return Size - 1;
        }
        case TemporalTileProvider::TimeFormatType::YYYY_MM_DDThh_mm_ssZ: {
            constexpr const char Format[] = "YYYY-MM-DDTHR_MN_SCZ";
            constexpr const int Size = sizeof(Format);
            SpiceManager::ref().dateFromEphemerisTime(time, buffer, Size, Format);
            return Size - 1;
        }
        default:
            throw ghoul::MissingCaseException();
    }
}

std::unique_ptr<TileProvider> initTileProvider(TemporalTileProvider& t,
                                               std::string_view timekey)
{
    ZoneScoped

    static const std::vector<std::string> IgnoredTokens = {
        // From: http://www.gdal.org/frmt_wms.html
        "${x}",
        "${y}",
        "${z}",
        "${version}",
        "${format}",
        "${layer}"
    };


    std::string xmlTemplate(t.gdalXmlTemplate);
    const size_t pos = xmlTemplate.find(temporal::UrlTimePlaceholder);
    const size_t numChars = strlen(temporal::UrlTimePlaceholder);
    // @FRAGILE:  This will only find the first instance. Dangerous if that instance is
    // commented out ---abock
    std::string xml = xmlTemplate.replace(pos, numChars, timekey);

    xml = FileSys.expandPathTokens(std::move(xml), IgnoredTokens).string();

    t.initDict.setValue(KeyFilePath, xml);
    return std::make_unique<DefaultTileProvider>(t.initDict);
}

TileProvider* getTileProvider(TemporalTileProvider& t, std::string_view timekey) {
    ZoneScoped

    // @TODO (abock, 2020-08-20) This std::string creation can be removed once we switch
    // to C++20 thanks to P0919R2
    const auto it = t.tileProviderMap.find(std::string(timekey));
    if (it != t.tileProviderMap.end()) {
        return it->second.get();
    }
    else {
        std::unique_ptr<TileProvider> tileProvider = initTileProvider(t, timekey);
        initialize(*tileProvider);

        TileProvider* res = tileProvider.get();
        t.tileProviderMap[std::string(timekey)] = std::move(tileProvider);
        return res;
    }
}

TileProvider* getTileProvider(TemporalTileProvider& t, const Time& time) {
    ZoneScoped

    if (t.useFixedTime && !t.fixedTime.value().empty()) {
        try {
            return getTileProvider(t, t.fixedTime.value());
        }
        catch (const ghoul::RuntimeError& e) {
            LERRORC("TemporalTileProvider", e.message);
            return nullptr;
        }
    }
    else {
        Time tCopy(time);
        if (t.timeQuantizer.quantize(tCopy, true)) {
            char Buffer[22];
            const int size = timeStringify(t.timeFormat, tCopy, Buffer);
            try {
                return getTileProvider(t, std::string_view(Buffer, size));
            }
            catch (const ghoul::RuntimeError& e) {
                LERRORC("TemporalTileProvider", e.message);
                return nullptr;
            }
        }
    }
    return nullptr;
}

void ensureUpdated(TemporalTileProvider& t) {
    ZoneScoped

    if (!t.currentTileProvider) {
        update(t);
    }
}

std::string xmlValue(TemporalTileProvider& t, CPLXMLNode* node, const std::string& key,
                        const std::string& defaultVal)
{
    CPLXMLNode* n = CPLSearchXMLNode(node, key.c_str());
    if (!n) {
        throw ghoul::RuntimeError(
            fmt::format("Unable to parse file {}. {} missing", t.filePath.value(), key)
        );
    }

    const bool hasValue = n && n->psChild && n->psChild->pszValue;
    return hasValue ? n->psChild->pszValue : defaultVal;
}

std::string consumeTemporalMetaData(TemporalTileProvider& t, const std::string& xml) {
    ZoneScoped

    CPLXMLNode* node = CPLParseXMLString(xml.c_str());

    std::string timeStart = xmlValue(t, node, temporal::TimeStart, "2000 Jan 1");
    std::string timeResolution = xmlValue(t, node, temporal::TimeResolution, "2d");
    std::string timeEnd = xmlValue(t, node, temporal::TimeEnd, "Today");
    std::string timeIdFormat = xmlValue(
        t,
        node,
        temporal::TimeFormat,
        "YYYY-MM-DDThh:mm:ssZ"
    );

    Time start;
    start.setTime(std::move(timeStart));
    Time end(Time::now());
    if (timeEnd == "Yesterday") {
        end.advanceTime(-60.0 * 60.0 * 24.0); // Go back one day
    }
    else if (timeEnd != "Today") {
        end.setTime(std::move(timeEnd));
    }

    try {
        t.timeQuantizer.setStartEndRange(
            std::string(start.ISO8601()),
            std::string(end.ISO8601())
        );
        t.timeQuantizer.setResolution(timeResolution);
    }
    catch (const ghoul::RuntimeError& e) {
        throw ghoul::RuntimeError(fmt::format(
            "Could not create time quantizer for Temporal GDAL dataset '{}'. {}",
            t.filePath.value(), e.message
        ));
    }
    t.timeFormat = ghoul::from_string<TemporalTileProvider::TimeFormatType>(timeIdFormat);

    CPLXMLNode* gdalNode = CPLSearchXMLNode(node, "GDAL_WMS");
    if (gdalNode) {
        std::string gdalDescription = CPLSerializeXMLTree(gdalNode);
        return gdalDescription;
    }
    else {
        gdalNode = CPLSearchXMLNode(node, "FilePath");
        if (gdalNode) {
            std::string gdalDescription = std::string(gdalNode->psChild->pszValue);
            return gdalDescription;
        }
        else {
            return "";
        }
    }
}

bool readFilePath(TemporalTileProvider& t) {
    ZoneScoped

    std::ifstream in(t.filePath.value().c_str());
    std::string xml;
    if (in.is_open()) {
        // read file
        xml = std::string(
            std::istreambuf_iterator<char>(in),
            (std::istreambuf_iterator<char>())
        );
    }
    else {
        // Assume that it is already an xml
        xml = t.filePath;
    }

    // File path was not a path to a file but a GDAL config or empty
    std::filesystem::path f(t.filePath.value());
    if (std::filesystem::is_regular_file(f)) {
        t.initDict.setValue(temporal::KeyBasePath, f.parent_path().string());
    }

    t.gdalXmlTemplate = consumeTemporalMetaData(t, xml);
    return true;
}

} // namespace

unsigned int TileProvider::NumTileProviders = 0;


//
// General functions
//
void initializeDefaultTile() {
    ZoneScoped

    ghoul_assert(!DefaultTile.texture, "Default tile should not have been created");
    using namespace ghoul::opengl;

    // Create pixel data
    TileTextureInitData initData(
        8,
        8,
        GL_UNSIGNED_BYTE,
        Texture::Format::RGBA,
        TileTextureInitData::PadTiles::No,
        TileTextureInitData::ShouldAllocateDataOnCPU::Yes
    );
    char* pixels = new char[initData.totalNumBytes];
    memset(pixels, 0, initData.totalNumBytes * sizeof(char));

    // Create ghoul texture
    DefaultTileTexture = std::make_unique<Texture>(initData.dimensions);
    DefaultTileTexture->setDataOwnership(Texture::TakeOwnership::Yes);
    DefaultTileTexture->setPixelData(pixels);
    DefaultTileTexture->uploadTexture();
    DefaultTileTexture->setFilter(ghoul::opengl::Texture::FilterMode::LinearMipMap);

    // Create tile
    DefaultTile = Tile{ DefaultTileTexture.get(), std::nullopt, Tile::Status::OK };
}

void deinitializeDefaultTile() {
    DefaultTileTexture = nullptr;
}

std::unique_ptr<TileProvider> createFromDictionary(layergroupid::TypeID layerTypeID,
                                                   const ghoul::Dictionary& dictionary)
{
    ZoneScoped

    const char* type = layergroupid::LAYER_TYPE_NAMES[static_cast<int>(layerTypeID)];
    auto factory = FactoryManager::ref().factory<TileProvider>();
    TileProvider* result = factory->create(type, dictionary);
    return std::unique_ptr<TileProvider>(result);
}

TileProvider::TileProvider() : properties::PropertyOwner({ "tileProvider" }) {}

DefaultTileProvider::DefaultTileProvider(const ghoul::Dictionary& dictionary)
    : filePath(defaultprovider::FilePathInfo, "")
    , tilePixelSize(defaultprovider::TilePixelSizeInfo, 32, 32, 2048)
{
    ZoneScoped

    type = Type::DefaultTileProvider;

    tileCache = global::moduleEngine->module<GlobeBrowsingModule>()->tileCache();
    name = "Name unspecified";
    if (dictionary.hasValue<std::string>("Name")) {
        name = dictionary.value<std::string>("Name");
    }
    std::string _loggerCat = "DefaultTileProvider (" + name + ")";

    // 1. Get required Keys
    filePath = dictionary.value<std::string>(KeyFilePath);
    layerGroupID =
        static_cast<layergroupid::GroupID>(dictionary.value<int>("LayerGroupID"));

    // 2. Initialize default values for any optional Keys
    // getValue does not work for integers
    int pixelSize = 0;
    if (dictionary.hasValue<double>(defaultprovider::KeyTilePixelSize)) {
        pixelSize = static_cast<int>(
            dictionary.value<double>(defaultprovider::KeyTilePixelSize)
        );
        LDEBUG(fmt::format("Default pixel size overridden: {}", pixelSize));
    }

    if (dictionary.hasValue<bool>(defaultprovider::KeyPadTiles)) {
        padTiles = dictionary.value<bool>(defaultprovider::KeyPadTiles);
    }

    TileTextureInitData initData(
        tileTextureInitData(layerGroupID, padTiles, pixelSize)
    );
    tilePixelSize = initData.dimensions.x;


    // Only preprocess height layers by default
    switch (layerGroupID) {
        case layergroupid::GroupID::HeightLayers: performPreProcessing = true; break;
        default:                                  performPreProcessing = false; break;
    }

    if (dictionary.hasValue<bool>(defaultprovider::KeyPerformPreProcessing)) {
        performPreProcessing = dictionary.value<bool>(
            defaultprovider::KeyPerformPreProcessing
        );
        LDEBUG(fmt::format(
            "Default PerformPreProcessing overridden: {}", performPreProcessing
        ));
    }

    initAsyncTileDataReader(*this, initData);

    addProperty(filePath);
    addProperty(tilePixelSize);
}





SingleImageProvider::SingleImageProvider(const ghoul::Dictionary& dictionary)
    : filePath(singleimageprovider::FilePathInfo)
{
    ZoneScoped

    type = Type::SingleImageTileProvider;

    filePath = dictionary.value<std::string>(KeyFilePath);
    addProperty(filePath);

    reset(*this);
}





TextTileProvider::TextTileProvider(TileTextureInitData initData_, size_t fontSize_)
    : initData(std::move(initData_))
    , fontSize(fontSize_)
{
    ZoneScoped

    tileCache = global::moduleEngine->module<GlobeBrowsingModule>()->tileCache();
}





SizeReferenceTileProvider::SizeReferenceTileProvider(const ghoul::Dictionary& dictionary)
    : TextTileProvider(tileTextureInitData(layergroupid::GroupID::ColorLayers, false))
{
    ZoneScoped

    type = Type::SizeReferenceTileProvider;

    font = global::fontManager->font("Mono", static_cast<float>(fontSize));

    if (dictionary.hasValue<glm::dvec3>(sizereferenceprovider::KeyRadii)) {
        ellipsoid = dictionary.value<glm::dvec3>(sizereferenceprovider::KeyRadii);
    }
    else if (dictionary.hasValue<double>(sizereferenceprovider::KeyRadii)) {
        const double r = dictionary.value<double>(sizereferenceprovider::KeyRadii);
        ellipsoid = glm::dvec3(r, r, r);
    }
}





TileIndexTileProvider::TileIndexTileProvider(const ghoul::Dictionary&)
    : TextTileProvider(tileTextureInitData(layergroupid::GroupID::ColorLayers, false))
{
    ZoneScoped

    type = Type::TileIndexTileProvider;
}





TileProviderByIndex::TileProviderByIndex(const ghoul::Dictionary& dictionary) {
    ZoneScoped

    type = Type::ByIndexTileProvider;

    const ghoul::Dictionary& defaultProviderDict = dictionary.value<ghoul::Dictionary>(
        byindexprovider::KeyDefaultProvider
    );

    layergroupid::TypeID typeID;
    if (defaultProviderDict.hasValue<std::string>("Type")) {
        const std::string& t = defaultProviderDict.value<std::string>("Type");
        typeID = ghoul::from_string<layergroupid::TypeID>(t);

        if (typeID == layergroupid::TypeID::Unknown) {
            throw ghoul::RuntimeError("Unknown layer type: " + t);
        }
    }
    else {
        typeID = layergroupid::TypeID::DefaultTileLayer;
    }

    defaultTileProvider = createFromDictionary(typeID, defaultProviderDict);

    const ghoul::Dictionary& indexProvidersDict = dictionary.value<ghoul::Dictionary>(
        byindexprovider::KeyProviders
    );
    for (size_t i = 1; i <= indexProvidersDict.size(); i++) {
        ghoul::Dictionary indexProviderDict = indexProvidersDict.value<ghoul::Dictionary>(
            std::to_string(i)
        );
        ghoul::Dictionary tileIndexDict = indexProviderDict.value<ghoul::Dictionary>(
            byindexprovider::KeyTileIndex
        );
        ghoul::Dictionary providerDict = indexProviderDict.value<ghoul::Dictionary>(
            byindexprovider::KeyTileProvider
        );

        constexpr const char* KeyLevel = "Level";
        constexpr const char* KeyX = "X";
        constexpr const char* KeyY = "Y";

        int level = static_cast<int>(tileIndexDict.value<double>(KeyLevel));
        ghoul_assert(level < std::numeric_limits<uint8_t>::max(), "Level too large");
        int x = static_cast<int>(tileIndexDict.value<double>(KeyX));
        int y = static_cast<int>(tileIndexDict.value<double>(KeyY));

        const TileIndex tileIndex(x, y, static_cast<uint8_t>(level));

        layergroupid::TypeID providerTypeID = layergroupid::TypeID::DefaultTileLayer;
        if (defaultProviderDict.hasValue<std::string>("Type")) {
            const std::string& t = defaultProviderDict.value<std::string>("Type");
            providerTypeID = ghoul::from_string<layergroupid::TypeID>(t);

            if (providerTypeID == layergroupid::TypeID::Unknown) {
                throw ghoul::RuntimeError("Unknown layer type: " + t);
            }
        }

        std::unique_ptr<TileProvider> stp = createFromDictionary(
            providerTypeID,
            providerDict
        );
        TileIndex::TileHashKey key = tileIndex.hashKey();
        tileProviderMap.insert(std::make_pair(key, std::move(stp)));
    }
}





TileProviderByLevel::TileProviderByLevel(const ghoul::Dictionary& dictionary) {
    ZoneScoped

    type = Type::ByLevelTileProvider;

    layergroupid::GroupID layerGroupID = static_cast<layergroupid::GroupID>(
        dictionary.value<int>(bylevelprovider::KeyLayerGroupID)
    );

    if (dictionary.hasValue<ghoul::Dictionary>(bylevelprovider::KeyProviders)) {
        ghoul::Dictionary providers = dictionary.value<ghoul::Dictionary>(
            bylevelprovider::KeyProviders
        );

        for (size_t i = 1; i <= providers.size(); i++) {
            ghoul::Dictionary levelProviderDict = providers.value<ghoul::Dictionary>(
                std::to_string(i)
            );
            double floatMaxLevel = levelProviderDict.value<double>(
                bylevelprovider::KeyMaxLevel
            );
            int maxLevel = static_cast<int>(std::round(floatMaxLevel));

            ghoul::Dictionary providerDict = levelProviderDict.value<ghoul::Dictionary>(
                bylevelprovider::KeyTileProvider
            );
            providerDict.setValue(
                bylevelprovider::KeyLayerGroupID,
                static_cast<int>(layerGroupID)
            );

            layergroupid::TypeID typeID;
            if (providerDict.hasValue<std::string>("Type"))
            {
                const std::string& typeString = providerDict.value<std::string>("Type");
                typeID = ghoul::from_string<layergroupid::TypeID>(typeString);

                if (typeID == layergroupid::TypeID::Unknown) {
                    throw ghoul::RuntimeError("Unknown layer type: " + typeString);
                }
            }
            else {
                typeID = layergroupid::TypeID::DefaultTileLayer;
            }

            std::unique_ptr<TileProvider> tp = createFromDictionary(typeID, providerDict);

            std::string provId = providerDict.value<std::string>("Identifier");
            tp->setIdentifier(provId);
            std::string providerName = providerDict.value<std::string>("Name");
            tp->setGuiName(providerName);
            addPropertySubOwner(tp.get());

            levelTileProviders.push_back(std::move(tp));

            // Ensure we can represent the max level
            if (static_cast<int>(providerIndices.size()) < maxLevel) {
                providerIndices.resize(maxLevel + 1, -1);
            }

            // map this level to the tile provider index
            providerIndices[maxLevel] = static_cast<int>(levelTileProviders.size()) - 1;
        }
    }

    // Fill in the gaps (value -1 ) in provider indices, from back to end
    for (int i = static_cast<int>(providerIndices.size()) - 2; i >= 0; --i) {
        if (providerIndices[i] == -1) {
            providerIndices[i] = providerIndices[i + 1];
        }
    }
}





TemporalTileProvider::TemporalTileProvider(const ghoul::Dictionary& dictionary)
    : initDict(dictionary)
    , filePath(temporal::FilePathInfo)
    , useFixedTime(temporal::UseFixedTimeInfo, false)
    , fixedTime(temporal::FixedTimeInfo)
{
    ZoneScoped

    type = Type::TemporalTileProvider;

    filePath = dictionary.value<std::string>(KeyFilePath);
    addProperty(filePath);

    if (dictionary.hasValue<bool>(temporal::UseFixedTimeInfo.identifier)) {
        useFixedTime = dictionary.value<bool>(temporal::UseFixedTimeInfo.identifier);
    }
    addProperty(useFixedTime);
    
    if (dictionary.hasValue<std::string>(temporal::FixedTimeInfo.identifier)) {
        fixedTime = dictionary.value<std::string>(temporal::FixedTimeInfo.identifier);
    }
    addProperty(fixedTime);

    successfulInitialization = readFilePath(*this);

    if (!successfulInitialization) {
        LERRORC("TemporalTileProvider", "Unable to read file " + filePath.value());
    }
}






bool initialize(TileProvider& tp) {
    ZoneScoped

    ghoul_assert(!tp.isInitialized, "TileProvider can only be initialized once.");

    if (TileProvider::NumTileProviders > std::numeric_limits<uint16_t>::max() - 1) {
        LERRORC(
            "TileProvider",
            "Number of tile providers exceeds 65535. Something will break soon"
        );
        TileProvider::NumTileProviders = 0;
    }
    tp.uniqueIdentifier = static_cast<uint16_t>(TileProvider::NumTileProviders++);
    if (TileProvider::NumTileProviders == std::numeric_limits<unsigned int>::max()) {
        --TileProvider::NumTileProviders;
        return false;
    }

    tp.isInitialized = true;

    switch (tp.type) {
        case Type::DefaultTileProvider:
            break;
        case Type::SingleImageTileProvider:
            break;
        case Type::SizeReferenceTileProvider: {
            SizeReferenceTileProvider& t = static_cast<SizeReferenceTileProvider&>(tp);
            initialize(t);
            break;
        }
        case Type::TileIndexTileProvider: {
            TileIndexTileProvider& t = static_cast<TileIndexTileProvider&>(tp);
            initialize(t);
            break;
        }
        case Type::ByIndexTileProvider:
            break;
        case Type::ByLevelTileProvider: {
            TileProviderByLevel& t = static_cast<TileProviderByLevel&>(tp);
            bool success = true;
            for (const std::unique_ptr<TileProvider>& prov : t.levelTileProviders) {
                success &= initialize(*prov);
            }
            return success;
        }
        case Type::TemporalTileProvider:
            break;
        default:
            throw ghoul::MissingCaseException();
    }

    return true;
}






bool deinitialize(TileProvider& tp) {
    ZoneScoped

    switch (tp.type) {
        case Type::DefaultTileProvider:
            break;
        case Type::SingleImageTileProvider:
            break;
        case Type::SizeReferenceTileProvider: {
            SizeReferenceTileProvider& t = static_cast<SizeReferenceTileProvider&>(tp);
            deinitialize(t);
            break;
        }
        case Type::TileIndexTileProvider: {
            TileIndexTileProvider& t = static_cast<TileIndexTileProvider&>(tp);
            deinitialize(t);
            break;
        }
        case Type::ByIndexTileProvider:
            break;
        case Type::ByLevelTileProvider: {
            TileProviderByLevel& t = static_cast<TileProviderByLevel&>(tp);
            bool success = true;
            for (const std::unique_ptr<TileProvider>& prov : t.levelTileProviders) {
                success &= deinitialize(*prov);
            }
            return success;
        }
        case Type::TemporalTileProvider:
            break;
        default:
            throw ghoul::MissingCaseException();
    }
    return true;
}






Tile tile(TileProvider& tp, const TileIndex& tileIndex) {
    ZoneScoped

    switch (tp.type) {
        case Type::DefaultTileProvider: {
            ZoneScopedN("Type::DefaultTileProvider")
            DefaultTileProvider& t = static_cast<DefaultTileProvider&>(tp);
            if (t.asyncTextureDataProvider) {
                if (tileIndex.level > maxLevel(t)) {
                    return Tile { nullptr, std::nullopt, Tile::Status::OutOfRange };
                }
                const cache::ProviderTileKey key = { tileIndex, t.uniqueIdentifier };
                Tile tile = t.tileCache->get(key);
                if (!tile.texture) {
                    //TracyMessage("Enqueuing tile", 32);
                    t.asyncTextureDataProvider->enqueueTileIO(tileIndex);
                }

                return tile;
            }
            else {
                return Tile { nullptr, std::nullopt, Tile::Status::Unavailable };
            }
        }
        case Type::SingleImageTileProvider: {
            ZoneScopedN("Type::SingleImageTileProvider")
            SingleImageProvider& t = static_cast<SingleImageProvider&>(tp);
            return t.tile;
        }
        case Type::SizeReferenceTileProvider: {
            ZoneScopedN("Type::SizeReferenceTileProvider")
            SizeReferenceTileProvider& t = static_cast<SizeReferenceTileProvider&>(tp);

            const GeodeticPatch patch(tileIndex);
            const bool aboveEquator = patch.isNorthern();
            const double lat = aboveEquator ? patch.minLat() : patch.maxLat();
            const double lon1 = patch.minLon();
            const double lon2 = patch.maxLon();
            int l = static_cast<int>(t.ellipsoid.longitudalDistance(lat, lon1, lon2));

            const bool useKm = l > 9999;
            if (useKm) {
                l /= 1000;
            }
            l = static_cast<int>(std::round(l));
            if (useKm) {
                l *= 1000;
            }
            double tileLongitudalLength = l;

            const char* unit;
            if (tileLongitudalLength > 9999) {
                tileLongitudalLength *= 0.001;
                unit = "km";
            }
            else {
                unit = "m";
            }

            t.text = fmt::format(" {:.0f} {:s}", tileLongitudalLength, unit);
            t.textPosition = {
                0.f,
                aboveEquator ?
                    t.fontSize / 2.f :
                    t.initData.dimensions.y - 3.f * t.fontSize / 2.f
            };
            t.textColor = glm::vec4(1.f);

            return tile(t, tileIndex);
        }
        case Type::TileIndexTileProvider: {
            ZoneScopedN("Type::TileIndexTileProvider")
            TileIndexTileProvider& t = static_cast<TileIndexTileProvider&>(tp);
            t.text = fmt::format(
                "level: {}\nx: {}\ny: {}", tileIndex.level, tileIndex.x, tileIndex.y
            );
            t.textPosition = glm::vec2(
                t.initData.dimensions.x / 4 -
                (t.initData.dimensions.x / 32) * log10(1 << tileIndex.level),
                t.initData.dimensions.y / 2 + t.fontSize
            );
            t.textColor = glm::vec4(1.f);

            return tile(t, tileIndex);
        }
        case Type::ByIndexTileProvider: {
            ZoneScopedN("Type::ByIndexTileProvider")
            TileProviderByIndex& t = static_cast<TileProviderByIndex&>(tp);
            const auto it = t.tileProviderMap.find(tileIndex.hashKey());
            const bool hasProvider = it != t.tileProviderMap.end();
            return hasProvider ? tile(*it->second, tileIndex) : Tile();
        }
        case Type::ByLevelTileProvider: {
            ZoneScopedN("Type::ByLevelTileProvider")
            TileProviderByLevel& t = static_cast<TileProviderByLevel&>(tp);
            TileProvider* provider = levelProvider(t, tileIndex.level);
            if (provider) {
                return tile(*provider, tileIndex);
            }
            else {
                return Tile();
            }
        }
        case Type::TemporalTileProvider: {
            ZoneScopedN("Type::TemporalTileProvider")
            TemporalTileProvider& t = static_cast<TemporalTileProvider&>(tp);
            if (t.successfulInitialization) {
                ensureUpdated(t);
                return tile(*t.currentTileProvider, tileIndex);
            }
            else {
                return Tile();
            }
        }
        default:
            throw ghoul::MissingCaseException();
    }
}




Tile::Status tileStatus(TileProvider& tp, const TileIndex& index) {
    ZoneScoped

    switch (tp.type) {
        case Type::DefaultTileProvider: {
            DefaultTileProvider& t = static_cast<DefaultTileProvider&>(tp);
            if (t.asyncTextureDataProvider) {
                const RawTileDataReader& rawTileDataReader =
                    t.asyncTextureDataProvider->rawTileDataReader();

                if (index.level > rawTileDataReader.maxChunkLevel()) {
                    return Tile::Status::OutOfRange;
                }

                const cache::ProviderTileKey key = { index, t.uniqueIdentifier };
                return t.tileCache->get(key).status;
            }
            else {
                return Tile::Status::Unavailable;
            }
        }
        case Type::SingleImageTileProvider: {
            SingleImageProvider& t = static_cast<SingleImageProvider&>(tp);
            return t.tile.status;
        }
        case Type::SizeReferenceTileProvider:
            return Tile::Status::OK;
        case Type::TileIndexTileProvider:
            return Tile::Status::OK;
        case Type::ByIndexTileProvider: {
            TileProviderByIndex& t = static_cast<TileProviderByIndex&>(tp);
            const auto it = t.tileProviderMap.find(index.hashKey());
            const bool hasProvider = it != t.tileProviderMap.end();
            return hasProvider ?
                tileStatus(*it->second, index) :
                Tile::Status::Unavailable;
        }
        case Type::ByLevelTileProvider: {
            TileProviderByLevel& t = static_cast<TileProviderByLevel&>(tp);
            TileProvider* provider = levelProvider(t, index.level);
            return provider ? tileStatus(*provider, index) : Tile::Status::Unavailable;
        }
        case Type::TemporalTileProvider: {
            TemporalTileProvider& t = static_cast<TemporalTileProvider&>(tp);
            if (t.successfulInitialization) {
                ensureUpdated(t);
                return tileStatus(*t.currentTileProvider, index);
            }
            else {
                return Tile::Status::Unavailable;
            }
        }
        default:
            throw ghoul::MissingCaseException();
    }
}






TileDepthTransform depthTransform(TileProvider& tp) {
    ZoneScoped

    switch (tp.type) {
        case Type::DefaultTileProvider: {
            DefaultTileProvider& t = static_cast<DefaultTileProvider&>(tp);
            if (t.asyncTextureDataProvider) {
                return t.asyncTextureDataProvider->rawTileDataReader().depthTransform();
            }
            else {
                return { 1.f, 0.f };
            }
        }
        case Type::SingleImageTileProvider:
            return { 0.f, 1.f };
        case Type::SizeReferenceTileProvider:
            return { 0.f, 1.f };
        case Type::TileIndexTileProvider:
            return { 0.f, 1.f };
        case Type::ByIndexTileProvider: {
            TileProviderByIndex& t = static_cast<TileProviderByIndex&>(tp);
            return depthTransform(*t.defaultTileProvider);
        }
        case Type::ByLevelTileProvider:
            return { 0.f, 1.f };
        case Type::TemporalTileProvider: {
            TemporalTileProvider& t = static_cast<TemporalTileProvider&>(tp);
            if (t.successfulInitialization) {
                ensureUpdated(t);
                return depthTransform(*t.currentTileProvider);
            }
            else {
                return { 1.f, 0.f };
            }
        }
        default:
            throw ghoul::MissingCaseException();
    }
}






int update(TileProvider& tp) {
    ZoneScoped

    switch (tp.type) {
        case Type::DefaultTileProvider: {
            DefaultTileProvider& t = static_cast<DefaultTileProvider&>(tp);
            if (!t.asyncTextureDataProvider) {
                break;
            }

            t.asyncTextureDataProvider->update();
            bool hasUploaded = initTexturesFromLoadedData(t);

            if (t.asyncTextureDataProvider->shouldBeDeleted()) {
                t.asyncTextureDataProvider = nullptr;
                initAsyncTileDataReader(
                    t,
                    tileTextureInitData(t.layerGroupID, t.padTiles, t.tilePixelSize)
                );
            }
            if (hasUploaded) {
                return 1;
            }
            break;
        }
        case Type::SingleImageTileProvider:
            break;
        case Type::SizeReferenceTileProvider:
            break;
        case Type::TileIndexTileProvider:
            break;
        case Type::ByIndexTileProvider: {
            TileProviderByIndex& t = static_cast<TileProviderByIndex&>(tp);
            using K = TileIndex::TileHashKey;
            using V = std::unique_ptr<TileProvider>;
            for (std::pair<const K, V>& it : t.tileProviderMap) {
                update(*it.second);
            }
            update(*t.defaultTileProvider);
            break;
        }
        case Type::ByLevelTileProvider: {
            TileProviderByLevel& t = static_cast<TileProviderByLevel&>(tp);
            for (const std::unique_ptr<TileProvider>& provider : t.levelTileProviders) {
                update(*provider);
            }
            break;
        }
        case Type::TemporalTileProvider: {
            TemporalTileProvider& t = static_cast<TemporalTileProvider&>(tp);
            if (t.successfulInitialization) {
                TileProvider* newCurr = getTileProvider(t, global::timeManager->time());
                if (newCurr) {
                    t.currentTileProvider = newCurr;
                }
                if (t.currentTileProvider) {
                    update(*t.currentTileProvider);
                }
            }
            break;
        }
        default:
            throw ghoul::MissingCaseException();
    }
    return 0;
}






void reset(TileProvider& tp) {
    ZoneScoped

    switch (tp.type) {
        case Type::DefaultTileProvider: {
            DefaultTileProvider& t = static_cast<DefaultTileProvider&>(tp);
            t.tileCache->clear();
            if (t.asyncTextureDataProvider) {
                t.asyncTextureDataProvider->prepareToBeDeleted();
            }
            else {
                initAsyncTileDataReader(
                    t,
                    tileTextureInitData(t.layerGroupID, t.padTiles, t.tilePixelSize)
                );
            }
            break;
        }
        case Type::SingleImageTileProvider: {
            SingleImageProvider& t = static_cast<SingleImageProvider&>(tp);

            if (t.filePath.value().empty()) {
                return;
            }
            t.tileTexture = ghoul::io::TextureReader::ref().loadTexture(t.filePath);
            if (!t.tileTexture) {
                throw ghoul::RuntimeError(
                    fmt::format("Unable to load texture '{}'", t.filePath.value())
                );
            }
            Tile::Status tileStatus = Tile::Status::OK;

            t.tileTexture->uploadTexture();
            t.tileTexture->setFilter(
                ghoul::opengl::Texture::FilterMode::AnisotropicMipMap
            );

            t.tile = Tile{ t.tileTexture.get(), std::nullopt, tileStatus };
            break;
        }
        case Type::SizeReferenceTileProvider: {
            SizeReferenceTileProvider& t = static_cast<SizeReferenceTileProvider&>(tp);
            reset(t);
            break;
        }
        case Type::TileIndexTileProvider: {
            TileIndexTileProvider& t = static_cast<TileIndexTileProvider&>(tp);
            reset(t);
            break;
        }
        case Type::ByIndexTileProvider: {
            TileProviderByIndex& t = static_cast<TileProviderByIndex&>(tp);
            using K = TileIndex::TileHashKey;
            using V = std::unique_ptr<TileProvider>;
            for (std::pair<const K, V>& it : t.tileProviderMap) {
                reset(*it.second);
            }
            reset(*t.defaultTileProvider);
            break;
        }
        case Type::ByLevelTileProvider: {
            TileProviderByLevel& t = static_cast<TileProviderByLevel&>(tp);
            for (const std::unique_ptr<TileProvider>& provider : t.levelTileProviders) {
                reset(*provider);
            }
            break;
        }
        case Type::TemporalTileProvider: {
            TemporalTileProvider& t = static_cast<TemporalTileProvider&>(tp);
            if (t.successfulInitialization) {
                using K = TemporalTileProvider::TimeKey;
                using V = std::unique_ptr<TileProvider>;
                for (std::pair<const K, V>& it : t.tileProviderMap) {
                    reset(*it.second);
                }
            }
            break;
        }
        default:
            throw ghoul::MissingCaseException();
    }
}






int maxLevel(TileProvider& tp) {
    switch (tp.type) {
        case Type::DefaultTileProvider: {
            DefaultTileProvider& t = static_cast<DefaultTileProvider&>(tp);
            // 22 is the current theoretical maximum based on the number of hashes that
            // are possible to uniquely identify a tile. See ProviderTileHasher in
            // memoryawaretilecache.h
            return t.asyncTextureDataProvider ?
                t.asyncTextureDataProvider->rawTileDataReader().maxChunkLevel() :
                22;
        }
        case Type::SingleImageTileProvider:
            return 1337; // unlimited
        case Type::SizeReferenceTileProvider:
            return 1337; // unlimited
        case Type::TileIndexTileProvider:
            return 1337; // unlimited
        case Type::ByIndexTileProvider: {
            TileProviderByIndex& t = static_cast<TileProviderByIndex&>(tp);
            return maxLevel(*t.defaultTileProvider);
        }
        case Type::ByLevelTileProvider: {
            TileProviderByLevel& t = static_cast<TileProviderByLevel&>(tp);
            return static_cast<int>(t.providerIndices.size() - 1);
        }
        case Type::TemporalTileProvider: {
            TemporalTileProvider& t = static_cast<TemporalTileProvider&>(tp);
            if (t.successfulInitialization) {
                ensureUpdated(t);
                return maxLevel(*t.currentTileProvider);
            }
            else {
                return 0;
            }
        }
        default:
            throw ghoul::MissingCaseException();
    }
}






float noDataValueAsFloat(TileProvider& tp) {
    ZoneScoped

    ghoul_assert(tp.isInitialized, "TileProvider was not initialized.");
    switch (tp.type) {
        case Type::DefaultTileProvider: {
            DefaultTileProvider& t = static_cast<DefaultTileProvider&>(tp);
            return t.asyncTextureDataProvider ?
                t.asyncTextureDataProvider->noDataValueAsFloat() :
                std::numeric_limits<float>::min();
        }
        case Type::SingleImageTileProvider:
            return std::numeric_limits<float>::min();
        case Type::SizeReferenceTileProvider:
            return std::numeric_limits<float>::min();
        case Type::TileIndexTileProvider:
            return std::numeric_limits<float>::min();
        case Type::ByIndexTileProvider:
            return std::numeric_limits<float>::min();
        case Type::ByLevelTileProvider:
            return std::numeric_limits<float>::min();
        case Type::TemporalTileProvider:
            return std::numeric_limits<float>::min();
        default:
            throw ghoul::MissingCaseException();
    }
}





ChunkTile chunkTile(TileProvider& tp, TileIndex tileIndex, int parents, int maxParents) {
    ZoneScoped

    ghoul_assert(tp.isInitialized, "TileProvider was not initialized.");

    auto ascendToParent = [](TileIndex& ti, TileUvTransform& uv) {
        uv.uvOffset *= 0.5;
        uv.uvScale *= 0.5;

        uv.uvOffset += ti.positionRelativeParent();

        ti.x /= 2;
        ti.y /= 2;
        ti.level--;
    };

    TileUvTransform uvTransform = { glm::vec2(0.f, 0.f), glm::vec2(1.f, 1.f) };

    // Step 1. Traverse 0 or more parents up the chunkTree as requested by the caller
    for (int i = 0; i < parents && tileIndex.level > 1; i++) {
        ascendToParent(tileIndex, uvTransform);
    }
    maxParents -= parents;

    // Step 2. Traverse 0 or more parents up the chunkTree to make sure we're inside
    //         the range of defined data.
    int maximumLevel = maxLevel(tp);
    while (tileIndex.level > maximumLevel) {
        ascendToParent(tileIndex, uvTransform);
        maxParents--;
    }
    if (maxParents < 0) {
        return ChunkTile { Tile(), uvTransform, TileDepthTransform() };
    }

    // Step 3. Traverse 0 or more parents up the chunkTree until we find a chunk that
    //         has a loaded tile ready to use.
    while (tileIndex.level > 1) {
        Tile t = tile(tp, tileIndex);
        if (t.status != Tile::Status::OK) {
            if (--maxParents < 0) {
                return ChunkTile { Tile(), uvTransform, TileDepthTransform() };
            }
            ascendToParent(tileIndex, uvTransform);
        }
        else {
            return ChunkTile { std::move(t), uvTransform, TileDepthTransform() };
        }
    }

    return ChunkTile { Tile(), uvTransform, TileDepthTransform() };
}






ChunkTilePile chunkTilePile(TileProvider& tp, TileIndex tileIndex, int pileSize) {
    ZoneScoped

    ghoul_assert(tp.isInitialized, "TileProvider was not initialized.");
    ghoul_assert(pileSize >= 0, "pileSize must be positive");

    ChunkTilePile chunkTilePile;
    std::fill(chunkTilePile.begin(), chunkTilePile.end(), std::nullopt);
    for (int i = 0; i < pileSize; ++i) {
        chunkTilePile[i] = chunkTile(tp, tileIndex, i);
        if (chunkTilePile[i]->tile.status == Tile::Status::Unavailable) {
            if (i == 0) {
                // First iteration
                chunkTilePile[i]->tile = DefaultTile;
                chunkTilePile[i]->uvTransform.uvOffset = { 0.f, 0.f };
                chunkTilePile[i]->uvTransform.uvScale = { 1.f, 1.f };
            }
            else {
                // We are iterating through the array one-by-one, so we are guaranteed
                // that for tile 'i', tile 'i-1' already was initializated
                chunkTilePile[i]->tile = chunkTilePile[i - 1]->tile;
                chunkTilePile[i]->uvTransform.uvOffset =
                    chunkTilePile[i - 1]->uvTransform.uvOffset;
                chunkTilePile[i]->uvTransform.uvScale =
                    chunkTilePile[i - 1]->uvTransform.uvScale;
            }
        }
    }
    return chunkTilePile;
}

} // namespace openspace::globebrowsing::tileprovider
