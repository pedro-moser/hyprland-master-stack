#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/layout/algorithm/TiledAlgorithm.hpp>
#include <hyprland/src/layout/algorithm/Algorithm.hpp>
#include <hyprland/src/layout/space/Space.hpp>
#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>

#include "MasterStackAlgorithm.hpp"

using namespace Layout::Tiled;

HANDLE PHANDLE = nullptr;

static std::function<SDispatchResult(std::string)> g_originalMoveFocus;

static CMasterStackAlgorithm* getCurrentAlgo() {
    const auto MON = g_pCompositor->getMonitorFromCursor();
    if (!MON || !MON->m_activeWorkspace)
        return nullptr;

    const auto& SPACE = MON->m_activeWorkspace->m_space;
    if (!SPACE)
        return nullptr;

    const auto& ALGO = SPACE->algorithm();
    if (!ALGO)
        return nullptr;

    return dynamic_cast<CMasterStackAlgorithm*>(ALGO->tiledAlgo().get());
}

static SDispatchResult hookedMoveFocus(std::string args) {
    auto* algo = getCurrentAlgo();
    if (algo && algo->isOnStack()) {
        // block at boundaries
        if ((args == "u" || args == "k") && algo->isFirstStack())
            return {};
        if ((args == "d" || args == "j") && algo->isLastStack())
            return {};
    }
    return g_originalMoveFocus(args);
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[master-stack] Version mismatch! Recompile the plugin.",
            CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[master-stack] Version mismatch");
    }

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:master-stack:mfact", Hyprlang::FLOAT{0.5F});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:master-stack:peek_height", Hyprlang::INT{40});

    HyprlandAPI::addTiledAlgo(PHANDLE, "master-stack",
        &typeid(CMasterStackAlgorithm),
        [] { return makeUnique<CMasterStackAlgorithm>(); });

    g_originalMoveFocus = g_pKeybindManager->m_dispatchers["movefocus"];
    g_pKeybindManager->m_dispatchers["movefocus"] = hookedMoveFocus;

    HyprlandAPI::addNotification(PHANDLE, "[master-stack] Loaded successfully!",
        CHyprColor{0.2, 1.0, 0.2, 1.0}, 3000);

    return {"master-stack", "Master + card-stack layout", "Pedro", "0.2"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    if (g_originalMoveFocus)
        g_pKeybindManager->m_dispatchers["movefocus"] = g_originalMoveFocus;
    HyprlandAPI::removeAlgo(PHANDLE, "master-stack");
}
