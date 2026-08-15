#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

/**
    Running a processor's own block body over sub-blocks no larger than the size it prepared for.

    ## This is the SECOND widening of the extraction's §I, and it is declared as one

    §I lists what must not be extracted, and the extraction's rule is to stop and report rather than
    widen. The first crossing was `nf/testing/` — a measurement harness in a library scoped as
    shipping behaviour, admitted on an argument and named at its own exemption. **A second crossing
    allowed silently is how a boundary stops meaning anything**, so this one is named here on the
    same model, with the same three-part test written out:

      - **It is behaviour, not appearance.** A loop over sample ranges; nothing here draws.
      - **It is identical across all six castings.** "What happens when a host delivers more samples
        than it declared" is one question asked six times — precisely the shape that produced six
        diverging copies of everything else this library owns.
      - **It has no per-casting meaning in it.** Core owns the *loop*; the casting owns the *body*,
        and core never learns what any stage inside it does.

    This one is a purer case than the Program model was, and it is *shipping* behaviour rather than
    test support — so it sits inside the library proper rather than beside the harness.

    ## Why it exists

    Five of the six castings grow a scratch buffer inside `processBlock` when a host over-delivers,
    which is a heap allocation on the audio thread. TapeRot's case is worse: its oversampler is sized
    by `initProcessing (maximumBlockSize)` and then fed whatever arrives, so **any** block larger than
    the prepared maximum writes out of bounds. That was bisected once as "survives 257, 300, 400 —
    crashes by 450", and every figure in that sentence was measured and meaningless: the writes went
    out of bounds and did not fault, so the probe reported "survived, finite" while corrupting
    adjacent heap. 450 was not a threshold, only the first size that reached an unmapped page.

    Chunking removes both in one mechanism rather than two: nothing is ever handed more than it
    prepared for, so there is nothing to grow and nothing to overrun.

    ## What the caller keeps OUTSIDE this call

    `juce::ScopedNoDenormals` — it is scoped, so once per `processBlock` is both correct and cheaper
    than once per chunk — and the clearing of unused output channels. What goes inside is the DSP.

    ## Audio only, and that is a stated precondition rather than an oversight

    There is no `MidiBuffer` overload because **no casting in this suite reads MIDI**. A casting that
    starts to must not reach for this without the MIDI events being offset into each sub-block, and
    that overload does not exist yet precisely so the omission is a compile error rather than a
    silent mis-timing.

    ## It COMPOSES with a caller's own subdivision — inside, never instead

    TapeRot's GEN cascade needs a second subdivision, splitting where its smoothed generation count
    crosses an integer so each span runs with one stage count. Reflect-84's LFO bank needs a third,
    splitting on a fixed update interval. Both go *inside* the body this calls, and the distinction
    that makes that non-negotiable is what each bounds:

    | | Splits on | Bounds |
    |---|---|---|
    | `processInChunks` | a fixed length | span **LENGTH** — a safety property |
    | a GEN-crossing subdivision | an integer crossing | span **stage count** — correctness |
    | an LFO-interval subdivision | a fixed sample interval | span **modulator age** — correctness |

    **Only this one bounds length, so only this one is safety.** A buffer in which GEN happens not to
    cross an integer is ONE span, so a crossing loop cannot substitute for this one — it would hand
    the oversampler the whole over-delivered buffer and reinstate the out-of-bounds write, with a
    green suite, because nothing automates GEN in the test that would catch it.
*/
namespace nf
{

/** Invokes `body` on consecutive views of `buffer`, none longer than `maxBlockSize`.

    The views are non-owning — they alias the caller's channel pointers — so writes land in the
    original buffer and nothing allocates.

    When the buffer already fits, `body` is invoked once **on the buffer itself**, so the ordinary
    case costs one comparison and is not merely equivalent to the unchunked form but identical to it.

    A non-multiple length is handled by a short final span; there is nothing for a caller to round.
*/
template <typename ProcessSubBlock>
void processInChunks (juce::AudioBuffer<float>& buffer, int maxBlockSize, ProcessSubBlock&& body)
{
    const int totalSamples = buffer.getNumSamples();

    // A non-positive maximum means "no limit stated", which is the only sane reading: chunking to
    // zero would loop forever and chunking to a negative number is not a request. It is not silently
    // corrected to something else, because a caller that has not prepared has a different bug.
    if (maxBlockSize <= 0 || totalSamples <= maxBlockSize)
    {
        body (buffer);
        return;
    }

    auto* const* channels = buffer.getArrayOfWritePointers();
    const int numChannels = buffer.getNumChannels();

    for (int offset = 0; offset < totalSamples; offset += maxBlockSize)
    {
        const int spanSamples = juce::jmin (maxBlockSize, totalSamples - offset);

        juce::AudioBuffer<float> span (channels, numChannels, offset, spanSamples);
        body (span);
    }
}

} // namespace nf
