#pragma once

#include "../Shared.hpp"

namespace Jangine::Gfx::IO
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
    struct formatter<Jangine::Gfx::IO::Status>
    {
        constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

        template <typename FormatContext>
        auto format(Jangine::Gfx::IO::Status status, FormatContext &ctx) const
        {
            switch (status)
            {
            case Jangine::Gfx::IO::Status::SUCCESS:
                return format_to(ctx.out(), "OK");
            case Jangine::Gfx::IO::Status::FAILURE:
                return format_to(ctx.out(), "FAILURE");
            default:
                return format_to(ctx.out(), "Unknown Status");
            }
        }
    };
}
