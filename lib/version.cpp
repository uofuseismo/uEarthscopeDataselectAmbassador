#include <string>
#include "uEarthscopeDataselectAmbassador/version.hpp"

using namespace UEarthscopeDataselectAmbassador;

int Version::getMajor() noexcept
{
    return uEarthscopeDataselectAmbassador_MAJOR;
}

int Version::getMinor() noexcept
{
    return uEarthscopeDataselectAmbassador_MINOR;
}

int Version::getPatch() noexcept
{
    return uEarthscopeDataselectAmbassador_PATCH;
}

//NOLINTBEGIN(bugprone-easily-swappable-parameters)
bool Version::isAtLeast(const int major, const int minor,
                        const int patch) noexcept
//NOLINTEND(bugprone-easily-swappable-parameters)
{
    if (uEarthscopeDataselectAmbassador_MAJOR < major){return false;}
    if (uEarthscopeDataselectAmbassador_MAJOR > major){return true;}
    if (uEarthscopeDataselectAmbassador_MINOR < minor){return false;}
    if (uEarthscopeDataselectAmbassador_MINOR > minor){return true;}
    if (uEarthscopeDataselectAmbassador_PATCH < patch){return false;}
    return true;
}

std::string Version::getVersion() noexcept
{
    std::string version{uEarthscopeDataselectAmbassador_VERSION};
    return version;
}

std::string Version::getTag() noexcept
{
    std::string tag{uEarthscopeDataselectAmbassador_GITTAG};
    return tag;
}

std::string Version::getVersionWithTag() noexcept
{
    auto tag = Version::getTag();
    if (tag.empty())
    {
        return Version::getVersion();
    }
    else
    {
        return Version::getVersion() + "-" + tag;
    }
}
