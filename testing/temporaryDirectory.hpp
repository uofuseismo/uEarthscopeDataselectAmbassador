#ifndef TESTING_TEMPORARY_DIRECTORY_HPP
#define TESTING_TEMPORARY_DIRECTORY_HPP
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <system_error>

namespace Testing
{

/// A scratch directory that is removed when the test case ends, pass or fail.
class TemporaryDirectory
{
public:
    TemporaryDirectory()
    {
        std::random_device device;
        std::uniform_int_distribution<std::uint64_t> distribution;
        mPath = std::filesystem::temp_directory_path()
              / ("uEarthscopeDataselectAmbassadorTest_"
                + std::to_string(distribution(device)));
        std::filesystem::create_directories(mPath);
    }
    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory &) = delete;
    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(mPath, error);
    }
    /// Writes contents to name inside the directory and returns its path.
    [[nodiscard]] std::filesystem::path write(const std::string &name,
                                              const std::string &contents) const
    {
        auto path = mPath / name;
        std::ofstream file(path);
        file << contents;
        return path;
    }
    [[nodiscard]] const std::filesystem::path &path() const noexcept
    {
        return mPath;
    }
private:
    std::filesystem::path mPath;
};

}
#endif
