#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialiseGui;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.setPassesAreLogged (false);

    juce::String category;
    if (argc > 1) category = juce::String::fromUTF8 (argv[1]);
    if (category.isEmpty()) runner.runAllTests();
    else runner.runTestsInCategory (category);

    int failures = 0;
    for (int index = 0; index < runner.getNumResults(); ++index)
    {
        const auto* result = runner.getResult (index);
        std::cout << result->unitTestName << ": " << result->passes << " passed, "
                  << result->failures << " failed\n";
        failures += result->failures;
    }
    return failures == 0 ? 0 : 1;
}
