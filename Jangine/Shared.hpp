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

#if defined(_WIN32)
#if defined(JANGINE_BUILD)
#define JANGINE_API __declspec(dllexport)
#else
#define JANGINE_API __declspec(dllimport)
#endif // JANGINE_BUILD
#else
#define JANGINE_API
#endif // _WIN32

#include <typeinfo>
#include <exception>
#include <string>
#include <atomic>
#include <memory>
#include <functional>
#include <array>
#include <chrono>

typedef bool bool_t;
typedef void void_t;

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

    struct Surface
    {
        void_t *vulkanHandle = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
    };

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

    inline void ThrowNotSupportedIf(bool condition, const std::string &message)
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
    T *ThrowInvalidOperationIfNull(T *ptr, const std::string &message = "Null pointer exception")
    {
        if (ptr == nullptr)
        {
            throw InvalidOperationException(message);
        }
        return ptr;
    }
}