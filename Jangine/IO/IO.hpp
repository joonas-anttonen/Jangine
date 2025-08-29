#pragma once

#include "Shared.hpp"

namespace Jangine::IO
{
    enum class Status
    {
        SUCCESS,
        FAILURE,
    }; 
}

// Custom formatters
namespace std
{
    template <>
    struct formatter<Jangine::IO::Status>
    {
        constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

        template <typename FormatContext>
        auto format(Jangine::IO::Status status, FormatContext &ctx) const
        {
            switch (status)
            {
            case Jangine::IO::Status::SUCCESS:
                return format_to(ctx.out(), "OK");
            case Jangine::IO::Status::FAILURE:
                return format_to(ctx.out(), "FAILURE");
            default:
                return format_to(ctx.out(), "Unknown Status");
            }
        }
    };
}
