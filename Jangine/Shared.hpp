#pragma once

// Custom platform defines
#if defined(_WIN32)
#define JANGINE_PLATFORM_WINDOWS
#elif defined(__linux__)
#define JANGINE_PLATFORM_LINUX
#else
#define JANGINE_PLATFORM_UNKNOWN
#endif // Platform detection

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

typedef bool bool_t;
typedef void void_t;

namespace Jangine
{
    template <typename T>
    const char *ResolveTypeName()
    {
        return typeid(T).name();
    }

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

    inline void ThrowInvalidOperationIf(bool condition, const std::string &message)
    {
        if (condition)
        {
            throw InvalidOperationException(message);
        }
    }

    template <typename T>
    void ThrowInvalidOperationIfNull(T *ptr)
    {
        if (ptr == nullptr)
        {
            throw InvalidOperationException("Null pointer exception");
        }
    }
}