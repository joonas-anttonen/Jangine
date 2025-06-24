#include <Jangine/Core.hpp>

#include <iostream>

int main(int argc, char *argv[])
{
    Jangine::Core core;

    Jangine::Parameters params{
        .enableDebugging = true,
        .appVersion = {1, 0, 0},
        .appName = "Jangine",
    };
    core.Run(params);
    
    return 0;
}