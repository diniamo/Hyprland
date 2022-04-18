#include "ConfigManager.hpp"
#include "../managers/KeybindManager.hpp"

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iostream>

CConfigManager::CConfigManager() {
    setDefaultVars();
}

void CConfigManager::setDefaultVars() {
    configValues["general:max_fps"].intValue = 240;
    configValues["general:sensitivity"].floatValue = 0.25f;
    configValues["general:apply_sens_to_raw"].intValue = 1;
    configValues["general:main_mod"].strValue = "SUPER";                                               // exposed to the user for easier configuring
    configValues["general:main_mod_internal"].intValue = g_pKeybindManager->stringToModMask("SUPER");  // actually used and automatically calculated

    configValues["general:damage_tracking"].strValue = "none";
    configValues["general:damage_tracking_internal"].intValue = DAMAGE_TRACKING_NONE;

    configValues["general:border_size"].intValue = 1;
    configValues["general:gaps_in"].intValue = 5;
    configValues["general:gaps_out"].intValue = 20;
    configValues["general:col.active_border"].intValue = 0xffffffff;
    configValues["general:col.inactive_border"].intValue = 0xff444444;

    configValues["decoration:rounding"].intValue = 1;
    configValues["decoration:blur"].intValue = 1;
    configValues["decoration:blur_size"].intValue = 8;
    configValues["decoration:blur_passes"].intValue = 1;
    configValues["decoration:active_opacity"].floatValue = 1;
    configValues["decoration:inactive_opacity"].floatValue = 1;

    configValues["dwindle:pseudotile"].intValue = 0;
    configValues["dwindle:col.group_border"].intValue = 0x66777700;
    configValues["dwindle:col.group_border_active"].intValue = 0x66ffff00;

    configValues["animations:enabled"].intValue = 1;
    configValues["animations:speed"].floatValue = 7.f;
    configValues["animations:windows_speed"].floatValue = 0.f;
    configValues["animations:windows"].intValue = 1;
    configValues["animations:borders_speed"].floatValue = 0.f;
    configValues["animations:borders"].intValue = 1;
    configValues["animations:fadein_speed"].floatValue = 0.f;
    configValues["animations:fadein"].intValue = 1;

    configValues["input:kb_layout"].strValue = "en";
    configValues["input:kb_variant"].strValue = "";
    configValues["input:kb_options"].strValue = "";
    configValues["input:kb_rules"].strValue = "";
    configValues["input:kb_model"].strValue = "";

    configValues["input:follow_mouse"].intValue = 1;

    configValues["autogenerated"].intValue = 0;
}

void CConfigManager::init() {
    
    loadConfigLoadVars();

    const char* const ENVHOME = getenv("HOME");

    const std::string CONFIGPATH = ENVHOME + (ISDEBUG ? (std::string) "/.config/hypr/hyprlandd.conf" : (std::string) "/.config/hypr/hyprland.conf");

    struct stat fileStat;
    int err = stat(CONFIGPATH.c_str(), &fileStat);
    if (err != 0) {
        Debug::log(WARN, "Error at statting config, error %i", errno);
    }

    lastModifyTime = fileStat.st_mtime;

    isFirstLaunch = false;
}

void CConfigManager::configSetValueSafe(const std::string& COMMAND, const std::string& VALUE) {
    if (configValues.find(COMMAND) == configValues.end()) {
        parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">: No such field.";
        return;
    }


    auto& CONFIGENTRY = configValues.at(COMMAND);
    if (CONFIGENTRY.intValue != -1) {
        try {
            if (VALUE.find("0x") == 0) {
                // Values with 0x are hex
                const auto VALUEWITHOUTHEX = VALUE.substr(2);
                CONFIGENTRY.intValue = stol(VALUEWITHOUTHEX, nullptr, 16);
            } else
                CONFIGENTRY.intValue = stol(VALUE);
        } catch (...) {
            Debug::log(WARN, "Error reading value of %s", COMMAND.c_str());
            parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">.";
        }
    } else if (CONFIGENTRY.floatValue != -1) {
        try {
            CONFIGENTRY.floatValue = stof(VALUE);
        } catch (...) {
            Debug::log(WARN, "Error reading value of %s", COMMAND.c_str());
            parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">.";
        }
    } else if (CONFIGENTRY.strValue != "") {
        try {
            CONFIGENTRY.strValue = VALUE;
        } catch (...) {
            Debug::log(WARN, "Error reading value of %s", COMMAND.c_str());
            parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">.";
        }
    }
}

void CConfigManager::handleRawExec(const std::string& command, const std::string& args) {
    // Exec in the background dont wait for it.
    if (fork() == 0) {
        execl("/bin/sh", "/bin/sh", "-c", args.c_str(), nullptr);

        _exit(0);
    }
}

void CConfigManager::handleMonitor(const std::string& command, const std::string& args) {

    // get the monitor config
    SMonitorRule newrule;

    std::string curitem = "";

    std::string argZ = args;

    auto nextItem = [&]() {
        auto idx = argZ.find_first_of(',');

        if (idx != std::string::npos) {
            curitem = argZ.substr(0, idx);
            argZ = argZ.substr(idx + 1);
        } else {
            curitem = argZ;
            argZ = "";
        }
    };

    nextItem();

    newrule.name = curitem;

    nextItem();

    if (curitem == "disable" || curitem == "disabled") {
        newrule.disabled = true;

        m_dMonitorRules.push_back(newrule);

        return;
    }

    newrule.resolution.x = stoi(curitem.substr(0, curitem.find_first_of('x')));
    newrule.resolution.y = stoi(curitem.substr(curitem.find_first_of('x') + 1, curitem.find_first_of('@')));

    if (curitem.find_first_of('@') != std::string::npos)
        newrule.refreshRate = stof(curitem.substr(curitem.find_first_of('@') + 1));

    nextItem();

    newrule.offset.x = stoi(curitem.substr(0, curitem.find_first_of('x')));
    newrule.offset.y = stoi(curitem.substr(curitem.find_first_of('x') + 1));

    nextItem();

    newrule.mfact = stof(curitem);

    nextItem();

    newrule.scale = stof(curitem);

    m_dMonitorRules.push_back(newrule);
}

void CConfigManager::handleBind(const std::string& command, const std::string& value) {
    // example:
    // bind=SUPER,G,exec,dmenu_run <args>

    auto valueCopy = value;

    const auto MOD = g_pKeybindManager->stringToModMask(valueCopy.substr(0, valueCopy.find_first_of(",")));
    valueCopy = valueCopy.substr(valueCopy.find_first_of(",") + 1);

    const auto KEY = valueCopy.substr(0, valueCopy.find_first_of(","));
    valueCopy = valueCopy.substr(valueCopy.find_first_of(",") + 1);

    const auto HANDLER = valueCopy.substr(0, valueCopy.find_first_of(","));
    valueCopy = valueCopy.substr(valueCopy.find_first_of(",") + 1);

    const auto COMMAND = valueCopy;

    if (KEY != "")
        g_pKeybindManager->addKeybind(SKeybind{KEY, MOD, HANDLER, COMMAND});
}

void CConfigManager::handleWindowRule(const std::string& command, const std::string& value) {
    const auto RULE = value.substr(0, value.find_first_of(","));
    const auto VALUE = value.substr(value.find_first_of(",") + 1);

    // check rule and value
    if (RULE == "" || VALUE == "") {
        return;
    }

    // verify we support a rule
    if (RULE != "float" 
        && RULE != "tile"
        && RULE.find("move") != 0
        && RULE.find("size") != 0
        && RULE.find("pseudo") != 0
        && RULE.find("monitor") != 0) {
            Debug::log(ERR, "Invalid rule found: %s", RULE.c_str());
            parseError = "Invalid rule found: " + RULE;
            return;
        }

    m_dWindowRules.push_back({RULE, VALUE});

}

void CConfigManager::handleDefaultWorkspace(const std::string& command, const std::string& value) {

    const auto DISPLAY = value.substr(0, value.find_first_of(','));
    const auto WORKSPACEID = stoi(value.substr(value.find_first_of(',') + 1));

    for (auto& mr : m_dMonitorRules) {
        if (mr.name == DISPLAY) {
            mr.defaultWorkspaceID = WORKSPACEID;
            break;
        }
    }
}

void CConfigManager::parseLine(std::string& line) {
    // first check if its not a comment
    const auto COMMENTSTART = line.find_first_of('#');
    if (COMMENTSTART == 0)
        return;

    // now, cut the comment off
    if (COMMENTSTART != std::string::npos)
        line = line.substr(0, COMMENTSTART);

    // remove shit at the beginning
    while (line[0] == ' ' || line[0] == '\t') {
        line = line.substr(1);
    }

    if (line.find(" {") != std::string::npos) {
        auto cat = line.substr(0, line.find(" {"));
        transform(cat.begin(), cat.end(), cat.begin(), ::tolower);
        currentCategory = cat;
        return;
    }

    if (line.find("}") != std::string::npos && currentCategory != "") {
        currentCategory = "";
        return;
    }

    // And parse
    // check if command
    const auto EQUALSPLACE = line.find_first_of('=');

    if (EQUALSPLACE == std::string::npos)
        return;

    const auto COMMAND = removeBeginEndSpacesTabs(line.substr(0, EQUALSPLACE));
    const auto VALUE = removeBeginEndSpacesTabs(line.substr(EQUALSPLACE + 1));
    //

    if (COMMAND == "exec") {
        if (isFirstLaunch) {
            firstExecRequests.push_back(VALUE);
        } else {
            handleRawExec(COMMAND, VALUE);
        }
        return;
    } else if (COMMAND == "exec-once") {
        if (isFirstLaunch) {
            firstExecRequests.push_back(VALUE);
        }
        return;
    } else if (COMMAND == "monitor") {
        handleMonitor(COMMAND, VALUE);
        return;
    } else if (COMMAND == "bind") {
        handleBind(COMMAND, VALUE);
        return;
    } else if (COMMAND == "workspace") {
        handleDefaultWorkspace(COMMAND, VALUE);
        return;
    } else if (COMMAND == "windowrule") {
        handleWindowRule(COMMAND, VALUE);
        return;
    }

    configSetValueSafe(currentCategory + (currentCategory == "" ? "" : ":") + COMMAND, VALUE);
}

void CConfigManager::loadConfigLoadVars() {
    Debug::log(LOG, "Reloading the config!");
    parseError = "";       // reset the error
    currentCategory = "";  // reset the category
    
    // reset all vars before loading
    setDefaultVars();

    m_dMonitorRules.clear();
    m_dWindowRules.clear();
    g_pKeybindManager->clearKeybinds();

    const char* const ENVHOME = getenv("HOME");
    const std::string CONFIGPATH = ENVHOME + (ISDEBUG ? (std::string) "/.config/hypr/hyprlandd.conf" : (std::string) "/.config/hypr/hyprland.conf");

    std::ifstream ifs;
    ifs.open(CONFIGPATH);

    if (!ifs.good()) {
        Debug::log(WARN, "Config reading error. (No file? Attempting to generate, backing up old one if exists)");
        try {
            std::filesystem::rename(CONFIGPATH, CONFIGPATH + ".backup");
        } catch(...) { /* Probably doesn't exist */}

        std::ofstream ofs;
        ofs.open(CONFIGPATH, std::ios::trunc);

        ofs << AUTOCONFIG;

        ofs.close();

        ifs.open(CONFIGPATH);

        if (!ifs.good()) {
            parseError = "Broken config file! (Could not open)";
            return;
        }
    }

    std::string line = "";
    int linenum = 1;
    if (ifs.is_open()) {
        while (std::getline(ifs, line)) {
            // Read line by line.
            try {
                parseLine(line);
            } catch (...) {
                Debug::log(ERR, "Error reading line from config. Line:");
                Debug::log(NONE, "%s", line.c_str());

                parseError += "Config error at line " + std::to_string(linenum) + ": Line parsing error.";
            }

            if (parseError != "" && parseError.find("Config error at line") != 0) {
                parseError = "Config error at line " + std::to_string(linenum) + ": " + parseError;
            }

            ++linenum;
        }

        ifs.close();
    }

    for (auto& m : g_pCompositor->m_lMonitors)
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m.ID);

    // Update the keyboard layout to the cfg'd one if this is not the first launch
    if (!isFirstLaunch)
        g_pInputManager->setKeyboardLayout();

    // Calculate the internal vars
    configValues["general:main_mod_internal"].intValue = g_pKeybindManager->stringToModMask(configValues["general:main_mod"].strValue);
    const auto DAMAGETRACKINGMODE = g_pHyprRenderer->damageTrackingModeFromStr(configValues["general:damage_tracking"].strValue);
    if (DAMAGETRACKINGMODE != DAMAGE_TRACKING_INVALID)
        configValues["general:damage_tracking_internal"].intValue = DAMAGETRACKINGMODE;
    else {
        parseError = "invalid value for general:damage_tracking, supported: full, monitor, none";
        configValues["general:damage_tracking_internal"].intValue = DAMAGE_TRACKING_NONE;
    }

    // parseError will be displayed next frame
    if (parseError != "")
        g_pHyprError->queueCreate(parseError + "\nHyprland may not work correctly.", CColor(255, 50, 50, 255));
    else if (configValues["autogenerated"].intValue == 1)
        g_pHyprError->queueCreate("Warning: You're using an autogenerated config! (config file: " + CONFIGPATH + " )\nSUPER+Enter -> kitty\nSUPER+T -> Alacritty\nSUPER+M -> exit Hyprland", CColor(255, 255, 70, 255));
    else
        g_pHyprError->destroy();
}

void CConfigManager::tick() {
    const char* const ENVHOME = getenv("HOME");

    const std::string CONFIGPATH = ENVHOME + (ISDEBUG ? (std::string) "/.config/hypr/hyprlandd.conf" : (std::string) "/.config/hypr/hyprland.conf");

    if (!std::filesystem::exists(CONFIGPATH)) {
        loadConfigLoadVars();
        return;
    }

    struct stat fileStat;
    int err = stat(CONFIGPATH.c_str(), &fileStat);
    if (err != 0) {
        Debug::log(WARN, "Error at ticking config at %s, error %i: %s", CONFIGPATH.c_str(), err, strerror(err));
        return;
    }

    // check if we need to reload cfg
    if (fileStat.st_mtime != lastModifyTime) {
        lastModifyTime = fileStat.st_mtime;

        loadConfigLoadVars();
    }
}

std::mutex configmtx;
SConfigValue CConfigManager::getConfigValueSafe(std::string val) {
    std::lock_guard<std::mutex> lg(configmtx);

    SConfigValue copy = configValues[val];

    return copy;
}

int CConfigManager::getInt(std::string v) {
    return getConfigValueSafe(v).intValue;
}

float CConfigManager::getFloat(std::string v) {
    return getConfigValueSafe(v).floatValue;
}

std::string CConfigManager::getString(std::string v) {
    return getConfigValueSafe(v).strValue;
}

void CConfigManager::setInt(std::string v, int val) {
    configValues[v].intValue = val;
}

void CConfigManager::setFloat(std::string v, float val) {
    configValues[v].floatValue = val;
}

void CConfigManager::setString(std::string v, std::string val) {
    configValues[v].strValue = val;
}

SMonitorRule CConfigManager::getMonitorRuleFor(std::string name) {
    SMonitorRule* found = nullptr;

    for (auto& r : m_dMonitorRules) {
        if (r.name == name) {
            found = &r;
            break;
        }
    }

    if (found)
        return *found;

    Debug::log(WARN, "No rule found for %s, trying to use the first.", name.c_str());

    for (auto& r : m_dMonitorRules) {
        if (r.name == "") {
            found = &r;
            break;
        }
    }

    if (found)
        return *found;

    Debug::log(WARN, "No rules configured. Using the default hardcoded one.");

    return SMonitorRule{.name = "", .resolution = Vector2D(1280, 720), .offset = Vector2D(0, 0), .mfact = 0.5f, .scale = 1};
}

std::vector<SWindowRule> CConfigManager::getMatchingRules(CWindow* pWindow) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return std::vector<SWindowRule>();

    std::vector<SWindowRule> returns;

    std::string title = g_pXWaylandManager->getTitle(pWindow);
    std::string appidclass = g_pXWaylandManager->getAppIDClass(pWindow);

    for (auto& rule : m_dWindowRules) {
        // check if we have a matching rule
        try {
            std::regex classCheck(rule.szValue);

            if (!std::regex_search(title, classCheck) && !std::regex_search(appidclass, classCheck))
                continue;
        } catch (...) {
            Debug::log(ERR, "Regex error at %s", rule.szValue.c_str());
        }

        // applies. Read the rule and behave accordingly
        Debug::log(LOG, "Window rule %s -> %s matched %x [%s]", rule.szRule.c_str(), rule.szValue.c_str(), pWindow, pWindow->m_szTitle.c_str());

        returns.push_back(rule);
    }

    return returns;
}

void CConfigManager::dispatchExecOnce() {
    if (firstExecDispatched || isFirstLaunch)
        return;

    firstExecDispatched = true;

    for (auto& c : firstExecRequests) {
        handleRawExec("", c);
    }

    firstExecRequests.clear(); // free some kb of memory :P
}