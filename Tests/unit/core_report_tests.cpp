#include "../../Source/core/EventSink.h"
#include "../../Source/core/Report.h"

#include <juce_core/juce_core.h>

namespace aph::tests
{
class CoreReportTests final : public juce::UnitTest
{
public:
    CoreReportTests() : UnitTest("ReportAndEvents", "core") {}

    void runTest() override
    {
        beginTest("Report contains required top-level fields");
        {
            Report report;
            report.completedAt = Report::timestampNowUtc();
            report.durationSeconds = 5.0;
            report.passed = true;
            report.exitCode = ExitCode::success;
            report.plugins.add({ 0, "uuid", "/tmp/Synth.vst3", "Synth", "Vendor", "1.0", "VST3", 64, true, false });
            AudioMeasurement measurement;
            measurement.tap = "output";
            measurement.sampleCount = 48000;
            measurement.channelCount = 2;
            measurement.sampleRate = 48000.0;
            report.addMeasurement (measurement);

            const auto json = report.toJson();
            expect(json.isObject());
            expect(json.getDynamicObject()->hasProperty("schemaVersion"));
            expect(json.getDynamicObject()->hasProperty("hostVersion"));
            expect(json.getDynamicObject()->hasProperty("platform"));
            expect(json.getDynamicObject()->hasProperty("configuration"));
            expect(json.getDynamicObject()->hasProperty("plugins"));
            expect(json.getDynamicObject()->hasProperty("timeline"));
            expect(json.getDynamicObject()->hasProperty("measurements"));
            expect(json.getDynamicObject()->hasProperty("assertions"));
            expect(json.getDynamicObject()->hasProperty("artifacts"));
            expect(json.getDynamicObject()->hasProperty("errors"));
            expect(json.getDynamicObject()->hasProperty("warnings"));
            expect(json.getDynamicObject()->hasProperty("startedAt"));
            expect(json.getDynamicObject()->hasProperty("completedAt"));
            expect(json.getDynamicObject()->hasProperty("durationSeconds"));
            expect(json.getDynamicObject()->hasProperty("passed"));
            expect(json.getDynamicObject()->hasProperty("exitCode"));
            expectEquals(static_cast<int>(json.getProperty("exitCode", -1)), 0);
        }

        beginTest("NDJSON event is a single compact JSON object line");
        {
            juce::MemoryOutputStream output;
            NdjsonEventSink sink(output);

            auto payload = juce::var(new juce::DynamicObject());
            payload.getDynamicObject()->setProperty("pid", 1234);
            payload.getDynamicObject()->setProperty("report", "report.json");

            juce::String error;
            expect(sink.emit("host_started", payload, &error), error);

            const auto text = output.toString();
            expect(text.endsWithChar('\n'));
            expectEquals(text.indexOfChar('\n'), text.length() - 1);

            const auto parsed = juce::JSON::parse(text.trim());
            expect(parsed.isObject());
            expectEquals(parsed.getProperty("schemaVersion", {}).toString(), juce::String("1.0"));
            expectEquals(parsed.getProperty("event", {}).toString(), juce::String("host_started"));
            expectEquals(static_cast<int>(parsed.getProperty("pid", 0)), 1234);
            expectEquals(parsed.getProperty("report", {}).toString(), juce::String("report.json"));
        }

        beginTest("Empty event name fails");
        {
            juce::MemoryOutputStream output;
            NdjsonEventSink sink(output);
            juce::String error;
            expect(! sink.emit({}, {}, &error));
            expect(error.isNotEmpty());
            expect(output.getDataSize() == 0);
        }
    }
};

static CoreReportTests coreReportTests;
} // namespace aph::tests
