This project must be written as production-quality commercial audio software.

General Rules

Use C++20.

Use JUCE only.

No external DSP libraries.

No FFT.

No STFT.

No Phase Vocoder.

No RubberBand.

No SoundTouch.

No Signalsmith Stretch.

No third-party GUI libraries.

No OpenGL.

No Direct2D.

No Skia.

No Dear ImGui.

No NanoVG.

No Qt.

No wxWidgets.

Everything must compile with the latest JUCE.

Realtime Audio Rules

Never allocate memory inside processBlock().

Never resize std::vector inside audio callback.

No malloc/new/delete in realtime thread.

No mutexes.

No locks.

No condition_variable.

No exceptions.

No file access.

No console logging.

No printf.

No dynamic_cast inside processBlock().

No virtual allocations during processing.

Prevent denormal numbers.

Use lock-free programming whenever possible.

Code must be deterministic.

GUI Rules

Use only JUCE native GUI components.

Use:

juce::Component

juce::Slider

juce::Label

juce::Graphics

juce::LookAndFeel_V4

No animations.

No GPU rendering.

Minimal repainting.

GUI must never affect DSP performance.

Architecture

Separate GUI and DSP completely.

PluginProcessor should only connect components.

DSP must be implemented in a dedicated class.

Keep classes small.

Single responsibility.

Well commented code.

Readable modern C++.

Performance has priority over fancy architecture.

JUCE is on path ./../JUCE/

VST3 sdk is on path ./../vst3sdk/