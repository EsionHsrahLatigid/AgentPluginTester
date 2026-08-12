#include "CaptureWriter.h"

namespace aph::capture
{
CaptureWriter::CaptureWriter()
{
    writerThread.startThread();
}

CaptureWriter::~CaptureWriter()
{
    stop();
    writerThread.stopThread (5000);
}

juce::Result CaptureWriter::start (const juce::File& destination,
                                   double sampleRate,
                                   int channelCount,
                                   int fifoSamples)
{
    stop();

    if (sampleRate <= 0.0 || channelCount <= 0 || fifoSamples <= 0)
        return juce::Result::fail ("Invalid capture format");

    finalFile = destination;
    temporaryFile = destination.getSiblingFile (destination.getFileName() + ".part");
    channels = channelCount;
    overflowCount.store (0);
    samplesAccepted.store (0);

    if (! finalFile.getParentDirectory().createDirectory())
        return juce::Result::fail ("Cannot create capture directory: " + finalFile.getParentDirectory().getFullPathName());

    temporaryFile.deleteFile();
    auto stream = std::unique_ptr<juce::OutputStream> (temporaryFile.createOutputStream());
    if (stream == nullptr)
        return juce::Result::fail ("Cannot open capture file: " + temporaryFile.getFullPathName());

    juce::WavAudioFormat wav;
    const auto options = juce::AudioFormatWriterOptions()
                             .withSampleRate (sampleRate)
                             .withNumChannels (channelCount)
                             .withBitsPerSample (32)
                             .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    auto writer = wav.createWriterFor (stream, options);
    if (writer == nullptr)
        return juce::Result::fail ("Cannot create 32-bit float WAV writer");

    threadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer.release(), writerThread, fifoSamples);
    active.store (true, std::memory_order_release);
    return juce::Result::ok();
}

bool CaptureWriter::push (const juce::AudioBuffer<float>& buffer) noexcept
{
    if (! active.load (std::memory_order_acquire) || threadedWriter == nullptr)
        return false;

    if (buffer.getNumChannels() < channels || buffer.getNumSamples() == 0)
        return buffer.getNumSamples() == 0;

    const auto accepted = threadedWriter->write (buffer.getArrayOfReadPointers(), buffer.getNumSamples());
    if (accepted)
        samplesAccepted.fetch_add (buffer.getNumSamples(), std::memory_order_relaxed);
    else
        overflowCount.fetch_add (1, std::memory_order_relaxed);
    return accepted;
}

juce::Result CaptureWriter::stop()
{
    active.store (false, std::memory_order_release);
    threadedWriter.reset();

    if (temporaryFile == juce::File())
        return juce::Result::ok();

    if (! temporaryFile.existsAsFile())
        return juce::Result::fail ("Temporary capture file is missing");

    if (finalFile.existsAsFile() && ! finalFile.deleteFile())
        return juce::Result::fail ("Cannot replace capture destination");

    if (! temporaryFile.moveFileTo (finalFile))
        return juce::Result::fail ("Cannot finalize capture file");

    temporaryFile = juce::File();
    return juce::Result::ok();
}
} // namespace aph::capture
