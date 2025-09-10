#pragma once
#pragma warning(disable : 4275 4251) // Disable warnings related to DLL interface and inheritance

// Detect platform
#if defined(_WIN32)
#define JANGINE_PLATFORM_WINDOWS
#elif defined(__linux__)
#define JANGINE_PLATFORM_LINUX
#else
#define JANGINE_PLATFORM_UNKNOWN
#endif

#ifndef JANGINE_API
#if defined(_WIN32)
#if defined(JANGINE_BUILD)
#define JANGINE_API __declspec(dllexport)
#else
#define JANGINE_API __declspec(dllimport)
#endif // JANGINE_BUILD
#else
#define JANGINE_API
#endif // _WIN32
#endif // JANGINE_API

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <istream>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <ranges>
#include <shared_mutex>
#include <span>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <thread>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <variant>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Geometry>

typedef bool bool_t;
typedef void void_t;

template <class T, class U>
concept DerivedFrom = std::is_base_of<U, T>::value;

namespace Jangine::Gfx
{
    class Core;

    // Auto-destroying unique handle for Gfx objects
    template <typename T>
    using Handle = std::unique_ptr<T, std::reference_wrapper<Core>>;

    // Auto-destroying shared handle for Gfx objects
    template <typename T>
    using SharedHandle = std::shared_ptr<T>;

    // Weak handle for Gfx objects
    template <typename T>
    using WeakHandle = std::weak_ptr<T>;

    template <typename T>
    inline Handle<T> MakeUnique(T *ptr, Core &core)
    {
        return Handle<T>(ptr, std::ref(core));
    }

    template <typename T>
    inline SharedHandle<T> PromoteToShared(Handle<T> &&uniqueHandle)
    {
        T *ptr = uniqueHandle.release();
        // Create shared_ptr with the same deleter logic
        return SharedHandle<T>(ptr, uniqueHandle.get_deleter());
    }

    template <typename T>
    inline SharedHandle<T> MakeShared(T *ptr, Core &core)
    {
        return std::shared_ptr<T>(ptr, std::ref(core));
    }

    template <typename T>
    inline WeakHandle<T> MakeWeak(SharedHandle<T> sharedHandle)
    {
        return WeakHandle<T>(sharedHandle);
    }
}

namespace Jangine
{
    /// @brief Create a span of bytes from a vector of any type
    template <typename T>
    static constexpr std::span<const std::byte> Span(const std::vector<T> &vector)
    {
        return std::span<const std::byte>(
            reinterpret_cast<const std::byte *>(vector.data()),
            vector.size() * sizeof(T));
    }

    template <typename T>
    static constexpr std::span<const std::byte> Span(const T &value)
    {
        return std::span<const std::byte>(
            reinterpret_cast<const std::byte *>(&value),
            sizeof(T));
    }

    namespace Math
    {
        constexpr float_t PI = 3.14159265358979323846f;
        constexpr float_t TAU = 2 * PI;

        constexpr float_t deg_to_rad(float_t degrees)
        {
            return degrees * (PI / 180.0f);
        }

        constexpr float_t rad_to_deg(float_t radians)
        {
            return radians * (180.0f / PI);
        }

        template <typename T = size_t>
        static constexpr T AlignUp(size_t value, size_t alignment)
        {
            return static_cast<T>((value + alignment - 1) & ~(alignment - 1));
        }

        /// @brief Linearly maps value [fromMin, fromMax] to [toMin, toMax]
        template <typename T>
        static constexpr T Map(T value, T fromMin, T fromMax, T toMin, T toMax)
        {
            return toMin + (toMax - toMin) * ((value - fromMin) / (fromMax - fromMin));
        }
    }

    inline std::string DurationToSIString(std::chrono::microseconds duration)
    {
        if (duration.count() < 1000)
        {
            return std::to_string(duration.count()) + " us";
        }
        else if (duration.count() < 1000000)
        {
            return std::to_string(duration.count() / 1000.0) + " ms";
        }
        else
        {
            return std::to_string(duration.count() / 1000000.0) + " s";
        }
    }

    inline std::string SizeToStringIEC(std::uintmax_t size)
    {
        const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
        uint32_t unit_index = 0;
        double_t size_in_units = static_cast<double_t>(size);

        while (size_in_units >= 1024 && unit_index < 4)
        {
            size_in_units /= 1024;
            ++unit_index;
        }

        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.2f %s", size_in_units, units[unit_index]);
        return std::string(buffer);
    }

    struct Guid
    {
        std::array<uint8_t, 16> bytes{};
    };

    /*struct Identity
    {
        Guid guid;
        std::string name;

        explicit Identity(const Guid &guid, const std::string &name)
            : guid(guid), name(name) {}
    };*/

    class SpinLock
    {
        std::atomic_flag flag = ATOMIC_FLAG_INIT;

    public:
        void lock()
        {
            while (flag.test_and_set(std::memory_order_acquire))
            { /* spin */
            }
        }
        void unlock()
        {
            flag.clear(std::memory_order_release);
        }
    };

    struct JANGINE_API Version
    {
        uint32_t major;
        uint32_t minor;
        uint32_t patch;

        Version(uint32_t majorVersion, uint32_t minorVersion, uint32_t patchVersion)
            : major(majorVersion), minor(minorVersion), patch(patchVersion) {}

        std::string ToString() const
        {
            return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
        }

        bool operator==(const Version &other) const
        {
            return major == other.major && minor == other.minor && patch == other.patch;
        }

        bool operator!=(const Version &other) const
        {
            return !(*this == other);
        }

        bool operator<(const Version &other) const
        {
            return std::tie(major, minor, patch) < std::tie(other.major, other.minor, other.patch);
        }

        bool operator<=(const Version &other) const
        {
            return *this < other || *this == other;
        }

        bool operator>(const Version &other) const
        {
            return !(*this <= other);
        }

        bool operator>=(const Version &other) const
        {
            return !(*this < other);
        }
    };

    class JANGINE_API JangineException : public std::exception
    {
    public:
        explicit JangineException(const std::string &message)
            : msg(message) {}

        virtual const char *what() const noexcept override
        {
            return msg.c_str();
        }

    private:
        std::string msg;
    };

    class JANGINE_API NotSupportedException : public JangineException
    {
    public:
        explicit NotSupportedException(const std::string &message)
            : JangineException("Not supported: " + message) {}
    };

    class JANGINE_API NotImplementedException : public JangineException
    {
    public:
        explicit NotImplementedException(const std::string &message)
            : JangineException("Not implemented: " + message) {}
    };

    class JANGINE_API InvalidOperationException : public JangineException
    {
    public:
        explicit InvalidOperationException(const std::string &message)
            : JangineException("Invalid operation: " + message) {}
    };

    class JANGINE_API InvalidDataException : public JangineException
    {
    public:
        explicit InvalidDataException(const std::string &message)
            : JangineException("Invalid data: " + message) {}
    };

    inline void ThrowInvalidDataIf(bool condition, const std::string &message = "")
    {
        if (condition)
        {
            throw InvalidDataException(message);
        }
    }

    inline void ThrowNotSupportedIf(bool condition, const std::string &message = "")
    {
        if (condition)
        {
            throw NotSupportedException(message);
        }
    }

    inline void ThrowInvalidOperationIf(bool condition, const std::string &message = "")
    {
        if (condition)
        {
            throw InvalidOperationException(message);
        }
    }

    inline void ThrowInvalidOperationIfNot(bool condition, const std::string &message = "")
    {
        if (!condition)
        {
            throw InvalidOperationException(message);
        }
    }

    template <typename T>
    T *ThrowInvalidOperationIfNull(T *ptr, const std::string &message = "")
    {
        if (ptr == nullptr)
        {
            throw InvalidOperationException(message);
        }
        return ptr;
    }

    namespace IO
    {
        inline void CreateBinaryHeader(
            std::string_view data,
            std::string_view headerFileName,
            std::string_view arrayName,
            std::string_view namespaceName)
        {
            std::ofstream out(std::string(headerFileName), std::ios::out | std::ios::trunc);
            if (!out)
                throw std::runtime_error("CreateBinaryHeader");

            out << "// This file was autogenerated.\n";
            out << "#pragma once\n\n";
            out << "#include <cstdint>\n#include <cstddef>\n\n";
            out << "namespace " << namespaceName << " {\n\n";
            out << "constexpr std::size_t " << arrayName << "_size = " << data.size() << ";\n";
            out << "constexpr uint8_t " << arrayName << "[" << data.size() << "] = {";

            for (size_t i = 0; i < data.size(); ++i)
            {
                if (i % 16 == 0)
                {
                    out << "\n    ";
                }
                out << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                    << (static_cast<uint32_t>(static_cast<uint8_t>(data[i])));
                if (i + 1 != data.size())
                    out << ", ";
            }
            out << std::dec; // restore decimal output
            out << "\n};\n";
            out << "\n} // namespace " << namespaceName << "\n";
            out.close();
        }
    }
}