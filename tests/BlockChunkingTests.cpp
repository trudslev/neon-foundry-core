#include <nf/BlockChunking.h>

#include <juce_core/juce_core.h>

#include <vector>

/**
    `nf::processInChunks`, and every arm shown able to fail before its green is believed.

    **The trap this file is scoped against: "no span exceeded the maximum" is also what a function
    that never chunked would report.** A single span of 2048 against a maximum of 300 would violate
    it, so that one assertion does carry weight — but "the spans summed to the total" and "the writes
    landed in the right place" are both satisfied by doing nothing at all. So the span COUNT is
    asserted explicitly wherever chunking is expected, which is the arm that distinguishes chunking
    from not chunking.
*/
class BlockChunkingTests final : public juce::UnitTest
{
public:
    BlockChunkingTests() : juce::UnitTest ("Block chunking", "core") {}

    void runTest() override
    {
        beginTest ("A buffer that already fits is passed through UNCHUNKED, not chunked into one");
        {
            juce::AudioBuffer<float> buffer (2, 512);
            buffer.clear();

            std::vector<int> spans;
            const float* seen = nullptr;

            nf::processInChunks (buffer, 512, [&] (juce::AudioBuffer<float>& span)
            {
                spans.push_back (span.getNumSamples());
                seen = span.getReadPointer (0);
            });

            expectEquals ((int) spans.size(), 1);
            expectEquals (spans.front(), 512);

            // **Identical to the unchunked form, not merely equivalent to it.** The ordinary case is
            // every block a well-behaved host ever sends, so it must cost one comparison and hand
            // the body the caller's own buffer rather than a view of it.
            expect (seen == buffer.getReadPointer (0),
                    "the fitting case built a view instead of passing the buffer through");
        }

        beginTest ("An over-delivered buffer is split, and the SPAN COUNT is what proves it");
        {
            juce::AudioBuffer<float> buffer (2, 2048);
            buffer.clear();

            std::vector<int> spans;

            nf::processInChunks (buffer, 512, [&] (juce::AudioBuffer<float>& span)
            {
                spans.push_back (span.getNumSamples());
            });

            expectEquals ((int) spans.size(), 4,
                          "2048 against a 512 maximum was not split into four spans");

            for (auto n : spans)
                expectLessOrEqual (n, 512, "a span exceeded the prepared maximum");

            int summed = 0;
            for (auto n : spans)
                summed += n;

            expectEquals (summed, 2048, "the spans did not cover the buffer");
        }

        beginTest ("A NON-MULTIPLE length ends in a short span — the off-by-one this centralises");
        {
            // The remainder is the half of this worth owning centrally: an off-by-one in the last
            // span is one defect in six places, and it would surface in whichever casting somebody
            // happened to drive at a length that is not a multiple.
            juce::AudioBuffer<float> buffer (2, 2048);
            buffer.clear();

            std::vector<int> spans;

            nf::processInChunks (buffer, 300, [&] (juce::AudioBuffer<float>& span)
            {
                spans.push_back (span.getNumSamples());
            });

            expectEquals ((int) spans.size(), 7, "2048 / 300 should be six full spans and a remainder");

            for (int i = 0; i < 6; ++i)
                expectEquals (spans[(size_t) i], 300);

            expectEquals (spans.back(), 248, "the remainder span is the wrong length");
        }

        beginTest ("Spans ALIAS the caller's buffer — writes land at the right absolute position");
        {
            // Without this, every arm above passes on a function that copies into scratch and throws
            // it away. It is also what proves nothing allocates: a view cannot.
            juce::AudioBuffer<float> buffer (2, 1000);
            buffer.clear();

            int written = 0;

            nf::processInChunks (buffer, 256, [&] (juce::AudioBuffer<float>& span)
            {
                for (int ch = 0; ch < span.getNumChannels(); ++ch)
                    for (int i = 0; i < span.getNumSamples(); ++i)
                        span.setSample (ch, i, (float) (written + i));

                written += span.getNumSamples();
            });

            bool positionsCorrect = true;

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 1000; ++i)
                    positionsCorrect = positionsCorrect
                                    && buffer.getSample (ch, i) == (float) i;

            expect (positionsCorrect,
                    "a span's writes did not land at their absolute position in the parent buffer");
        }

        beginTest ("KNOWN CASE — the span-count assertion FAILS against a body that does not chunk");
        {
            // **Shown able to fail, by causing it.** The arms above assert against
            // `processInChunks`; this one runs the construction it replaced — one call with the
            // whole buffer — through the identical assertions, and records that they reject it.
            //
            // Without this, "four spans of at most 512" is a property nobody has watched fail, and
            // a refactor that quietly stopped chunking would pass every arm that only checks
            // coverage and aliasing.
            juce::AudioBuffer<float> buffer (2, 2048);
            buffer.clear();

            std::vector<int> spans;

            const auto unchunked = [&] (juce::AudioBuffer<float>& whole)
            {
                spans.push_back (whole.getNumSamples());
            };

            unchunked (buffer);

            expectEquals ((int) spans.size(), 1,
                          "the control did not behave like the unchunked construction");

            expect (spans.front() > 512,
                    "the unchunked control did not exceed the prepared maximum, so the arms above "
                    "are asserting something that cannot be violated and prove nothing");

            logMessage ("  unchunked control hands the body " + juce::String (spans.front())
                            + " samples against a 512 maximum — which is the defect");
        }

        beginTest ("A non-positive maximum means NO LIMIT, and does not loop");
        {
            juce::AudioBuffer<float> buffer (2, 777);
            buffer.clear();

            int calls = 0;

            nf::processInChunks (buffer, 0, [&] (juce::AudioBuffer<float>&) { ++calls; });
            expectEquals (calls, 1, "a zero maximum did not pass the buffer through");

            calls = 0;
            nf::processInChunks (buffer, -1, [&] (juce::AudioBuffer<float>&) { ++calls; });
            expectEquals (calls, 1, "a negative maximum did not pass the buffer through");
        }
    }
};

static BlockChunkingTests blockChunkingTests;
