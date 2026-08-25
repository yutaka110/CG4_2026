#include "RailShooterHudDefinitionAsset.h"

#include "CourseAssetParsing.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
constexpr uintmax_t kMaximumAssetBytes = 64u*1024u;
void SetError(std::string* output, std::string message) {
    if (output != nullptr) *output = std::move(message);
}
std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}
bool ParseUInt(std::string_view text, uint32_t& output) {
    const auto result = std::from_chars(text.data(), text.data()+text.size(), output);
    return !text.empty() && result.ec == std::errc{} &&
        result.ptr == text.data()+text.size();
}
bool ParseFloat(std::string_view text, float& output) {
    std::string owned{text};
    char* end = nullptr;
    const float parsed = std::strtof(owned.c_str(), &end);
    if (owned.empty() || end == owned.c_str() || *end != '\0' || !std::isfinite(parsed))
        return false;
    output = parsed;
    return true;
}
bool ParseBool(std::string value, bool& output) {
    value = Lower(course_asset_parsing::Trim(std::move(value)));
    if (value == "true" || value == "1" || value == "yes") { output = true; return true; }
    if (value == "false" || value == "0" || value == "no") { output = false; return true; }
    return false;
}
bool ValidId(const std::string& value) {
    return !value.empty() && value.size() <= 96 &&
        std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return std::isalnum(c) || c == '_' || c == '-' || c == '.';
        });
}
bool InRange(float value, float minimum, float maximum) {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
bool ValidColor(const Vector4& color) {
    return InRange(color.x,0.0f,8.0f) && InRange(color.y,0.0f,8.0f) &&
        InRange(color.z,0.0f,8.0f) && InRange(color.w,0.0f,1.0f);
}
} // namespace

bool RailShooterHudDefinitionAsset::LoadFromFile(
    const std::filesystem::path& path,
    std::string* errorMessage) {
    const auto reject = [errorMessage,&path](const std::string& reason) {
        SetError(errorMessage, path.generic_string()+": "+reason);
        return false;
    };
    std::error_code fileError;
    const uintmax_t bytes = std::filesystem::file_size(path,fileError);
    if (fileError || bytes == 0 || bytes > kMaximumAssetBytes)
        return reject("file is missing, empty, or exceeds the 64 KiB limit");
    std::ifstream file(path,std::ios::binary);
    if (!file.is_open()) return reject("could not open HUD definition asset");
    std::unordered_map<std::string,std::string> values;
    std::string line;
    uint32_t lineNumber = 0;
    bool headerRead = false;
    while (std::getline(file,line)) {
        ++lineNumber;
        line = course_asset_parsing::Trim(std::move(line));
        if (line.empty() || line[0] == '#') continue;
        if (!headerRead) {
            const std::vector<std::string> header = course_asset_parsing::SplitPipe(line);
            uint32_t schema = 0;
            if (header.size()!=2 || header[0]!="RAIL_SHOOTER_HUD" ||
                !ParseUInt(header[1],schema) || schema==0 ||
                schema>kRailShooterHudAssetSchemaVersion)
                return reject("unsupported or missing RAIL_SHOOTER_HUD header");
            headerRead = true;
            continue;
        }
        const size_t separator = line.find('=');
        if (separator == std::string::npos)
            return reject("expected key=value at line "+std::to_string(lineNumber));
        const std::string key = course_asset_parsing::Trim(line.substr(0,separator));
        const std::string value = course_asset_parsing::Trim(line.substr(separator+1));
        if (key.empty() || !values.emplace(key,value).second)
            return reject("empty or duplicate key at line "+std::to_string(lineNumber));
    }
    if (!headerRead) return reject("HUD definition header was not found");
    const std::unordered_set<std::string> allowed{
        "assetId","displayName","enabled","scale","opacity","safeAreaPixels",
        "smoothingResponse","criticalPulseHz","maximumDrawCommands",
        "showPlayerHealth","showVehicleIntegrity","showWeapon","showWaveObjective",
        "showScore","showSpeed","showThreat","showSessionBanner","leftPanelWidth",
        "rightPanelWidth","topCenterWidth","barHeight","healthCriticalThreshold",
        "vehicleCriticalThreshold","panelColorR","panelColorG","panelColorB","panelColorA",
        "primaryColorR","primaryColorG","primaryColorB","primaryColorA",
        "healthyColorR","healthyColorG","healthyColorB","healthyColorA",
        "warningColorR","warningColorG","warningColorB","warningColorA",
        "criticalColorR","criticalColorG","criticalColorB","criticalColorA",
        "textColorR","textColorG","textColorB","textColorA",
        "mutedColorR","mutedColorG","mutedColorB","mutedColorA"};
    for (const auto& [key,value] : values) {
        (void)value;
        if (!allowed.contains(key)) return reject("unknown key: "+key);
    }
    RailShooterHudDefinitionAsset loaded = Defaults();
    const auto find = [&values](const char* key)->const std::string* {
        const auto found=values.find(key); return found==values.end()?nullptr:&found->second;
    };
    if (const std::string* value=find("assetId")) loaded.assetId=*value;
    else return reject("assetId is required");
    if (const std::string* value=find("displayName")) loaded.displayName=*value;
    const auto parseFloat=[&find](const char* key,float& target){
        const std::string* value=find(key); return value==nullptr||ParseFloat(*value,target);};
    const auto parseUInt=[&find](const char* key,uint32_t& target){
        const std::string* value=find(key); return value==nullptr||ParseUInt(*value,target);};
    const auto parseBool=[&find](const char* key,bool& target){
        const std::string* value=find(key); return value==nullptr||ParseBool(*value,target);};
#define HUD_FLOAT(key,field) if(!parseFloat(key,loaded.field)) return reject("malformed value: " key)
#define HUD_UINT(key,field) if(!parseUInt(key,loaded.field)) return reject("malformed value: " key)
#define HUD_BOOL(key,field) if(!parseBool(key,loaded.field)) return reject("malformed value: " key)
    HUD_BOOL("enabled",enabled); HUD_FLOAT("scale",scale); HUD_FLOAT("opacity",opacity);
    HUD_FLOAT("safeAreaPixels",safeAreaPixels); HUD_FLOAT("smoothingResponse",smoothingResponse);
    HUD_FLOAT("criticalPulseHz",criticalPulseHz); HUD_UINT("maximumDrawCommands",maximumDrawCommands);
    HUD_BOOL("showPlayerHealth",showPlayerHealth); HUD_BOOL("showVehicleIntegrity",showVehicleIntegrity);
    HUD_BOOL("showWeapon",showWeapon); HUD_BOOL("showWaveObjective",showWaveObjective);
    HUD_BOOL("showScore",showScore); HUD_BOOL("showSpeed",showSpeed);
    HUD_BOOL("showThreat",showThreat); HUD_BOOL("showSessionBanner",showSessionBanner);
    HUD_FLOAT("leftPanelWidth",leftPanelWidth); HUD_FLOAT("rightPanelWidth",rightPanelWidth);
    HUD_FLOAT("topCenterWidth",topCenterWidth); HUD_FLOAT("barHeight",barHeight);
    HUD_FLOAT("healthCriticalThreshold",healthCriticalThreshold);
    HUD_FLOAT("vehicleCriticalThreshold",vehicleCriticalThreshold);
    HUD_FLOAT("panelColorR",panelColor.x); HUD_FLOAT("panelColorG",panelColor.y);
    HUD_FLOAT("panelColorB",panelColor.z); HUD_FLOAT("panelColorA",panelColor.w);
    HUD_FLOAT("primaryColorR",primaryColor.x); HUD_FLOAT("primaryColorG",primaryColor.y);
    HUD_FLOAT("primaryColorB",primaryColor.z); HUD_FLOAT("primaryColorA",primaryColor.w);
    HUD_FLOAT("healthyColorR",healthyColor.x); HUD_FLOAT("healthyColorG",healthyColor.y);
    HUD_FLOAT("healthyColorB",healthyColor.z); HUD_FLOAT("healthyColorA",healthyColor.w);
    HUD_FLOAT("warningColorR",warningColor.x); HUD_FLOAT("warningColorG",warningColor.y);
    HUD_FLOAT("warningColorB",warningColor.z); HUD_FLOAT("warningColorA",warningColor.w);
    HUD_FLOAT("criticalColorR",criticalColor.x); HUD_FLOAT("criticalColorG",criticalColor.y);
    HUD_FLOAT("criticalColorB",criticalColor.z); HUD_FLOAT("criticalColorA",criticalColor.w);
    HUD_FLOAT("textColorR",textColor.x); HUD_FLOAT("textColorG",textColor.y);
    HUD_FLOAT("textColorB",textColor.z); HUD_FLOAT("textColorA",textColor.w);
    HUD_FLOAT("mutedColorR",mutedColor.x); HUD_FLOAT("mutedColorG",mutedColor.y);
    HUD_FLOAT("mutedColorB",mutedColor.z); HUD_FLOAT("mutedColorA",mutedColor.w);
#undef HUD_FLOAT
#undef HUD_UINT
#undef HUD_BOOL
    if (!loaded.Validate(errorMessage)) return false;
    *this=std::move(loaded);
    return true;
}

bool RailShooterHudDefinitionAsset::SaveToFile(
    const std::filesystem::path& path,
    std::string* errorMessage) const {
    if (!Validate(errorMessage)) return false;
    std::ofstream file(path,std::ios::binary|std::ios::trunc);
    if (!file.is_open()) { SetError(errorMessage,"Could not write HUD asset: "+path.generic_string()); return false; }
    file<<"RAIL_SHOOTER_HUD|"<<schemaVersion<<"\n"<<std::boolalpha<<std::fixed<<std::setprecision(3)
        <<"assetId="<<assetId<<"\n"<<"displayName="<<displayName<<"\n"<<"enabled="<<enabled<<"\n"
        <<"scale="<<scale<<"\n"<<"opacity="<<opacity<<"\n"<<"safeAreaPixels="<<safeAreaPixels<<"\n"
        <<"smoothingResponse="<<smoothingResponse<<"\n"<<"criticalPulseHz="<<criticalPulseHz<<"\n"
        <<"maximumDrawCommands="<<maximumDrawCommands<<"\n"
        <<"showPlayerHealth="<<showPlayerHealth<<"\n"<<"showVehicleIntegrity="<<showVehicleIntegrity<<"\n"
        <<"showWeapon="<<showWeapon<<"\n"<<"showWaveObjective="<<showWaveObjective<<"\n"
        <<"showScore="<<showScore<<"\n"<<"showSpeed="<<showSpeed<<"\n"
        <<"showThreat="<<showThreat<<"\n"<<"showSessionBanner="<<showSessionBanner<<"\n"
        <<"leftPanelWidth="<<leftPanelWidth<<"\n"<<"rightPanelWidth="<<rightPanelWidth<<"\n"
        <<"topCenterWidth="<<topCenterWidth<<"\n"<<"barHeight="<<barHeight<<"\n"
        <<"healthCriticalThreshold="<<healthCriticalThreshold<<"\n"
        <<"vehicleCriticalThreshold="<<vehicleCriticalThreshold<<"\n";
    const auto color=[&file](const char* key,const Vector4& c){
        file<<key<<"R="<<c.x<<"\n"<<key<<"G="<<c.y<<"\n"<<key<<"B="<<c.z<<"\n"<<key<<"A="<<c.w<<"\n";};
    color("panelColor",panelColor); color("primaryColor",primaryColor); color("healthyColor",healthyColor);
    color("warningColor",warningColor); color("criticalColor",criticalColor); color("textColor",textColor);
    color("mutedColor",mutedColor);
    if (!file.good()) { SetError(errorMessage,"Failed while writing HUD asset: "+path.generic_string()); return false; }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool RailShooterHudDefinitionAsset::Validate(std::string* errorMessage) const {
    const auto reject=[errorMessage](const char* reason){SetError(errorMessage,reason);return false;};
    if (schemaVersion==0||schemaVersion>kRailShooterHudAssetSchemaVersion||!ValidId(assetId))
        return reject("HUD schema or asset ID is invalid.");
    if (!InRange(scale,0.5f,2.0f)||!InRange(opacity,0.1f,1.0f)||
        !InRange(safeAreaPixels,8.0f,160.0f)||!InRange(smoothingResponse,0.1f,60.0f)||
        !InRange(criticalPulseHz,0.1f,12.0f)||maximumDrawCommands<24||maximumDrawCommands>512)
        return reject("HUD scaling, timing, or draw budget is invalid.");
    if (!InRange(leftPanelWidth,120.0f,600.0f)||!InRange(rightPanelWidth,120.0f,600.0f)||
        !InRange(topCenterWidth,160.0f,800.0f)||!InRange(barHeight,4.0f,40.0f)||
        !InRange(healthCriticalThreshold,0.05f,0.8f)||
        !InRange(vehicleCriticalThreshold,0.05f,0.8f))
        return reject("HUD layout dimensions or warning thresholds are invalid.");
    if (!ValidColor(panelColor)||!ValidColor(primaryColor)||!ValidColor(healthyColor)||
        !ValidColor(warningColor)||!ValidColor(criticalColor)||!ValidColor(textColor)||
        !ValidColor(mutedColor)) return reject("HUD color values are invalid.");
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

RailShooterHudDefinitionAsset RailShooterHudDefinitionAsset::Defaults() { return {}; }
