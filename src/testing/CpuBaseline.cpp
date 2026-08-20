#include <nf/testing/CpuBaseline.h>

#include <ctime>

namespace nf::testing
{
namespace
{
    /*  **Process CPU time, not wall clock.** `std::clock` is the portable one and it measures what
        this process burned rather than how long it waited, which is the whole point: a figure taken
        on a machine doing something else must not be inflated by the something else.

        Its resolution is coarse — a microsecond nominally, in practice a scheduler tick — which is
        why every measurement below runs for seconds rather than for blocks. */
    double cpuSecondsNow() noexcept
    {
        return (double) std::clock() / (double) CLOCKS_PER_SEC;
    }

    juce::String fractionString (double v)
    {
        return juce::String (v, 6);
    }
}

juce::String CpuCell::key() const
{
    return juce::String (blockSize) + "@" + juce::String (sampleRate, 0)
         + (editorOpen ? "/editor" : "/closed");
}

juce::String CpuCell::describe() const
{
    return key() + " = " + juce::String (coreFraction * 100.0, 2) + " % of one core";
}

bool CpuProvenance::isComplete() const
{
    return machine.isNotEmpty() && os.isNotEmpty() && config.isNotEmpty()
        && coreCommit.isNotEmpty() && takenOn.isNotEmpty();
}

const CpuCell* CpuBaseline::find (int blockSize, double sampleRate, bool editorOpen) const
{
    for (const auto& c : cells)
        if (c.blockSize == blockSize && std::abs (c.sampleRate - sampleRate) < 1.0
            && c.editorOpen == editorOpen)
            return &c;

    return nullptr;
}

std::vector<std::pair<int, double>> standardMatrix()
{
    return { { 64, 48000.0 }, { 128, 48000.0 }, { 512, 48000.0 },
             { 128, 44100.0 }, { 128, 96000.0 } };
}

CpuCell measureCpu (juce::AudioProcessor& processor, int blockSize, double sampleRate,
                    bool editorOpen, double seconds)
{
    CpuCell cell;
    cell.blockSize = blockSize;
    cell.sampleRate = sampleRate;
    cell.editorOpen = editorOpen;

    processor.setRateAndBufferSizeDetails (sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    const int channels = juce::jmax (2, processor.getTotalNumOutputChannels());
    juce::AudioBuffer<float> buffer (channels, blockSize);
    juce::MidiBuffer midi;

    /*  A signal rather than silence. Several castings gate, and a gate that never opens is a
        measurement of the closed branch — the cheap one. This is the known-case rule applied to a
        performance figure: a configuration that silences the plugin reports a small number for the
        trivial reason and looks like a result. */
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < blockSize; ++i)
            buffer.setSample (ch, i, 0.25f * std::sin ((float) i * 0.07f + (float) ch));

    /*  **The editor is parented to a HOLDER and painted through it**, not painted directly.

        `paintEntireComponent` does not consult a component's image cache — it is what the cache
        calls to FILL itself. The cache is read one level up, in `paintWithinParentContext`, which
        runs only when a PARENT paints a CHILD. A harness timing `paintEntireComponent` on the
        editor measures uncached paint and reports it as cached: Reflect-84's large knob came back
        4516 us "cached" against 4513 us uncached before that was caught, and the two figures
        agreeing to three parts in four thousand is what gave it away.

        So the editor goes inside a holder and the holder is painted. Everything with
        `setBufferedToImage` then behaves as it does in a host. */
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::Component holder;
    juce::Image frame;

    if (editorOpen)
    {
        editor.reset (processor.createEditorIfNeeded());
        jassert (editor != nullptr);

        if (editor != nullptr)
        {
            holder.setSize (editor->getWidth(), editor->getHeight());
            holder.addAndMakeVisible (*editor);
            frame = juce::Image (juce::Image::ARGB, juce::jmax (1, holder.getWidth()),
                                  juce::jmax (1, holder.getHeight()), true);
        }
    }

    // Warm-up. The first block after a prepare allocates in several castings and re-arms state in
    // others; both belong to category 1 and 3 rather than here, and including them would put a
    // one-off into a per-block figure.
    for (int i = 0; i < 32; ++i)
        processor.processBlock (buffer, midi);

    const int blocks = juce::jmax (1, (int) std::ceil (seconds * sampleRate / (double) blockSize));
    const double audioSeconds = (double) blocks * (double) blockSize / sampleRate;

    double nextFrameAt = 0.0;

    const double cpuStart = cpuSecondsNow();
    const auto wallStart = juce::Time::getMillisecondCounterHiRes();

    for (int i = 0; i < blocks; ++i)
    {
        processor.processBlock (buffer, midi);

        /*  **Pump the message loop while rendering, when an editor is open.** An editor that exists
            but never receives a message costs nothing, and several castings run 20 Hz timers and
            repaint scopes continuously — so constructing one without pumping it would report a GUI
            price of zero and read as a plugin with a free editor.

            Paced against the audio clock rather than run flat out, so the timers fire at the rate
            they would in a host rather than as fast as the loop spins. */
        if (editor != nullptr)
        {
            /*  One GUI frame per ~16.7 ms of AUDIO, which is 60 fps against the audio clock rather
                than against how fast this loop happens to spin. Timers are fired synchronously —
                the castings' scopes and meters run on 20-60 Hz timers, and an editor whose timers
                never fire is an editor that costs nothing and would report a free GUI. */
            const double audioElapsed = (double) (i + 1) * (double) blockSize / sampleRate;

            if (audioElapsed >= nextFrameAt)
            {
                juce::Timer::callPendingTimersSynchronously();

                juce::Graphics g (frame);
                holder.paintEntireComponent (g, false);

                nextFrameAt += 1.0 / 60.0;
            }

            juce::ignoreUnused (wallStart);
        }
    }

    const double cpuUsed = cpuSecondsNow() - cpuStart;

    editor.reset();

    cell.coreFraction = audioSeconds > 0.0 ? cpuUsed / audioSeconds : 0.0;
    return cell;
}

juce::String CpuComparison::describe() const
{
    return measured.describe() + "  baseline " + juce::String (baselineFraction * 100.0, 2)
         + " %  ratio " + juce::String (ratio, 3)
         + (withinTolerance ? "" : "  ** OVER **");
}

std::vector<CpuComparison> compareToBaseline (const std::vector<CpuCell>& measured,
                                              const CpuBaseline& baseline, double tolerance)
{
    std::vector<CpuComparison> out;

    for (const auto& m : measured)
    {
        CpuComparison c;
        c.measured = m;

        if (const auto* recorded = baseline.find (m.blockSize, m.sampleRate, m.editorOpen))
        {
            c.baselineFraction = recorded->coreFraction;
            /*  A baseline of zero would divide to infinity and report as a catastrophic regression,
                which reads as a tool defect rather than as the empty input it is. `read` refuses a
                malformed file for the same reason. */
            c.ratio = recorded->coreFraction > 0.0 ? m.coreFraction / recorded->coreFraction : 0.0;
            c.withinTolerance = recorded->coreFraction > 0.0 && c.ratio <= tolerance;
        }
        else
        {
            // A cell the baseline does not carry is a failure, not a pass: the matrix is shared so
            // that six castings cannot each measure a different set and then be compared.
            c.withinTolerance = false;
        }

        out.push_back (c);
    }

    return out;
}

juce::String CpuBaseline::toJson() const
{
    juce::String j;
    j << "{\n";
    j << "  \"_comment\": \"Category 7's baseline. Figures are FRACTIONS OF ONE CORE - CPU time "
         "consumed divided by the audio duration rendered - so 0.02 is 2 % of a core. Re-baselining "
         "is an explicit, committed act: the provenance below records what produced these, which is "
         "what makes 'one sitting, all six, one machine state' enforceable after the fact. Do not "
         "edit a figure to make a regression go away.\",\n";
    j << "  \"provenance\": {\n";
    j << "    \"machine\": \"" << provenance.machine << "\",\n";
    j << "    \"os\": \"" << provenance.os << "\",\n";
    j << "    \"config\": \"" << provenance.config << "\",\n";
    j << "    \"coreCommit\": \"" << provenance.coreCommit << "\",\n";
    j << "    \"takenOn\": \"" << provenance.takenOn << "\"\n";
    j << "  },\n";
    j << "  \"session\": { \"closedInstances\": " << sessionClosedInstances
      << ", \"openInstances\": " << sessionOpenInstances
      << ", \"coreFraction\": " << fractionString (sessionCoreFraction) << " },\n";
    j << "  \"cells\": [\n";

    for (size_t i = 0; i < cells.size(); ++i)
    {
        const auto& c = cells[i];
        j << "    { \"blockSize\": " << c.blockSize
          << ", \"sampleRate\": " << juce::String (c.sampleRate, 0)
          << ", \"editorOpen\": " << (c.editorOpen ? "true" : "false")
          << ", \"coreFraction\": " << fractionString (c.coreFraction) << " }"
          << (i + 1 < cells.size() ? ",\n" : "\n");
    }

    j << "  ]\n}\n";
    return j;
}

std::optional<CpuBaseline> CpuBaseline::read (const juce::File& file, juce::String& whyNot)
{
    if (! file.existsAsFile())
    {
        whyNot = "no baseline at " + file.getFullPathName()
               + " - a missing baseline is a refusal, not a skip. Write one with an explicit, "
                 "committed re-baseline run; a check that does nothing when its input is absent is "
                 "indistinguishable from a check that passed.";
        return {};
    }

    const auto parsed = juce::JSON::parse (file.loadFileAsString());

    if (! parsed.isObject())
    {
        whyNot = file.getFullPathName() + " is not a JSON object";
        return {};
    }

    CpuBaseline b;

    if (const auto* p = parsed["provenance"].getDynamicObject())
    {
        b.provenance.machine    = p->getProperty ("machine").toString();
        b.provenance.os         = p->getProperty ("os").toString();
        b.provenance.config     = p->getProperty ("config").toString();
        b.provenance.coreCommit = p->getProperty ("coreCommit").toString();
        b.provenance.takenOn    = p->getProperty ("takenOn").toString();
    }

    if (! b.provenance.isComplete())
    {
        whyNot = file.getFullPathName() + " has incomplete provenance. A figure whose machine and "
                 "build are unrecorded cannot be compared against a later one, which is the only "
                 "thing this file is for.";
        return {};
    }

    if (const auto* s = parsed["session"].getDynamicObject())
    {
        b.sessionClosedInstances = (int) s->getProperty ("closedInstances");
        b.sessionOpenInstances   = (int) s->getProperty ("openInstances");
        b.sessionCoreFraction    = (double) s->getProperty ("coreFraction");
    }

    if (const auto* array = parsed["cells"].getArray())
    {
        for (const auto& entry : *array)
        {
            if (const auto* o = entry.getDynamicObject())
            {
                CpuCell c;
                c.blockSize    = (int) o->getProperty ("blockSize");
                c.sampleRate   = (double) o->getProperty ("sampleRate");
                c.editorOpen   = (bool) o->getProperty ("editorOpen");
                c.coreFraction = (double) o->getProperty ("coreFraction");
                b.cells.push_back (c);
            }
        }
    }

    if (b.cells.empty())
    {
        whyNot = file.getFullPathName() + " carries no cells";
        return {};
    }

    return b;
}

} // namespace nf::testing
