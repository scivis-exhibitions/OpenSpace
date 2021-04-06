#include <openspace/util/openspacemodule.h>


#include <openspace/documentation/documentation.h>
#include <modules/skybrowser/skybrowsermodule.h>
#include <openspace/engine/globals.h>
#include <openspace/engine/moduleengine.h>
#include <openspace/rendering/renderengine.h>

#include <openspace/scripting/scriptengine.h>
#include <ghoul/misc/dictionaryluaformatter.h>

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/fmt.h>
#include <ghoul/glm.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/misc/assert.h>
#include <fstream>
#include <sstream>
#include <modules/skybrowser/include/screenspaceskybrowser.h>
#include <modules/skybrowser/include/screenspaceskytarget.h>
#include <openspace/interaction/navigationhandler.h>
#include <openspace/util/camera.h>
#include <thread> 

#include <openspace/util/coordinateconversion.h>
#include <glm/gtx/rotate_vector.hpp>


namespace {
    constexpr const char _loggerCat[] = "SkyBrowserModule";
} // namespace


namespace openspace::skybrowser::luascriptfunctions {

    int loadImgCollection(lua_State* L) {
        ghoul::lua::checkArgumentsAndThrow(L, 0, "lua::loadCollection");

        ScreenSpaceSkyBrowser* browser = dynamic_cast<ScreenSpaceSkyBrowser*>(global::renderEngine->screenSpaceRenderable("SkyBrowser1"));
        std::string url = "http://www.worldwidetelescope.org/wwtweb/catalog.aspx?W=wise";
        browser->sendMessageToWWT(browser->createMessageForLoadingWWTImgColl(url));
        browser->sendMessageToWWT(browser->createMessageForSettingForegroundWWT("Andromeda Galaxy"));
       // browser->sendMessageToWWT(browser->createMessageForMovingWWTCamera(glm::vec2(0.712305533333333, 41.269167), 24.0f));
        browser->sendMessageToWWT(browser->createMessageForSettingForegroundOpacityWWT(100));
        return 1;
    }
    
    int followCamera(lua_State* L) {
        ghoul::lua::checkArgumentsAndThrow(L, 0, "lua::followCamera");

        SkyBrowserModule* module = global::moduleEngine->module<SkyBrowserModule>();
        std::string root = "https://raw.githubusercontent.com/WorldWideTelescope/wwt-web-client/master/assets/webclient-explore-root.wtml";

        module->getWWTDataHandler()->loadWTMLCollectionsFromURL(root, "root");
        module->getWWTDataHandler()->printAllUrls();
        LINFO(std::to_string( module->getWWTDataHandler()->loadAllImagesFromXMLs()));

        return 1;
    }

    int moveBrowser(lua_State* L) {
        ghoul::lua::checkArgumentsAndThrow(L, 0, "lua::moveBrowser");
        SkyBrowserModule* module = global::moduleEngine->module<SkyBrowserModule>();
        module->getWWTDataHandler()->loadWTMLCollectionsFromDirectory(absPath("${MODULE_SKYBROWSER}/WWTimagedata/"));
        module->getWWTDataHandler()->printAllUrls();
        LINFO(std::to_string(module->getWWTDataHandler()->loadAllImagesFromXMLs()));
        return 1;
    }

    int createBrowser(lua_State* L) {
        ghoul::lua::checkArgumentsAndThrow(L, 0, "lua::createBrowser");
        SkyBrowserModule* module = global::moduleEngine->module<SkyBrowserModule>();
        std::vector<std::string> names = module->getWWTDataHandler()->getAllThumbnailUrls();

        lua_newtable(L);
        int number = 1;
        for (const std::string& s : names) {
            lua_pushstring(L, s.c_str());
            lua_rawseti(L, -2, number);
            ++number;
        }

        return 1;
    }
    int adjustCamera(lua_State* L) {
        ghoul::lua::checkArgumentsAndThrow(L, 0, "lua::adjustCamera");

        return 1;
    }
    
}
