#include <Jangine/Core.hpp>

#include <iostream>

int main(int argc, char *argv[])
{
    (void)argc; // Avoid unused parameter warning
    (void)argv; // Avoid unused parameter warning
    
    Jangine::Core core;

    Jangine::Parameters params{
        .enableDebugging = true,
        .appVersion = {1, 0, 0},
        .appName = "Jangine",
    };
    core.Run(params);
    
    return 0;
}