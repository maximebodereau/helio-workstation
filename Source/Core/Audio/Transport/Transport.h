/*
    This file is part of Helio Workstation.

    Helio is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Helio is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Helio. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

class SleepTimer;
class OrchestraPit;
class PlayerThread;
class PlayerThreadPool;
class RendererThread;

#include "TransportListener.h"
#include "TransportPlaybackCache.h"
#include "ProjectListener.h"
#include "OrchestraListener.h"
#include "Instrument.h"

class Transport final : public Serializable,
                        public ProjectListener,
                        private OrchestraListener
{
public:

    Transport(OrchestraPit &orchestraPit, SleepTimer &sleepTimer);
    ~Transport() override;
    
    static String getTimeString(double timeMs, bool includeMilliseconds = false);
    static String getTimeString(const RelativeTime &relTime, bool includeMilliseconds = false);

    // Returns microseconds per quarter note
    static int getTempoByControllerValue(float controllerValue) noexcept;
    static float getControllerValueByTempo(double secondsPerQuarterNote) noexcept;

    //===------------------------------------------------------------------===//
    // Transport
    //===------------------------------------------------------------------===//
    
    float getSeekBeat() const noexcept;
    float getTotalTime() const noexcept;
    void seekToBeat(float beatPosition);
    
    void probeSoundAtBeat(float beatPosition,
        const MidiSequence *limitToLayer = nullptr);
    
    void probeSequence(const MidiMessageSequence &sequence);

    void startPlayback();
    void startPlaybackFragment(float startBeat, float endBeat, bool looped);

    bool isPlaying() const;
    void stopPlayback();
    void toggleStartStopPlayback();

    void startRecording();
    bool isRecording() const;
    void stopRecording();

    void startRender(const String &filename);
    bool isRendering() const;
    void stopRender();
    
    float getRenderingPercentsComplete() const;
    
    void findTimeAndTempoAt(float beatPosition,
        double &outTimeMs, double &outTempo) const;

    MidiMessage findFirstTempoEvent();

    //===------------------------------------------------------------------===//
    // Sending messages in real-time
    //===------------------------------------------------------------------===//
    
    void previewMidiMessage(const String &trackId, const MidiMessage &message) const;
    void stopSound(const String &trackId = "") const;

    void allNotesControllersAndSoundOff() const;
    
    //===------------------------------------------------------------------===//
    // OrchestraListener
    //===------------------------------------------------------------------===//

    void instrumentAdded(Instrument *instrument) override;
    void instrumentRemoved(Instrument *instrument) override;
    void instrumentRemovedPostAction() override;

    //===------------------------------------------------------------------===//
    // ProjectListener
    //===------------------------------------------------------------------===//
    
    void onChangeMidiEvent(const MidiEvent &oldEvent,
        const MidiEvent &newEvent) override;
    void onAddMidiEvent(const MidiEvent &event) override;
    void onRemoveMidiEvent(const MidiEvent &event) override;
    void onPostRemoveMidiEvent(MidiSequence *const layer) override;

    void onAddClip(const Clip &clip) override;
    void onChangeClip(const Clip &oldClip, const Clip &newClip) override;
    void onRemoveClip(const Clip &clip) override;
    void onPostRemoveClip(Pattern *const pattern) override;

    void onAddTrack(MidiTrack *const track) override;
    void onRemoveTrack(MidiTrack *const track) override;
    void onChangeTrackProperties(MidiTrack *const track) override;

    void onChangeProjectBeatRange(float firstBeat, float lastBeat) override;
    void onChangeViewBeatRange(float firstBeat, float lastBeat) override {}
    void onReloadProjectContent(const Array<MidiTrack *> &tracks) override;

    //===------------------------------------------------------------------===//
    // Listeners management
    //===------------------------------------------------------------------===//

    void addTransportListener(TransportListener *listener);
    void removeTransportListener(TransportListener *listener);

    //===------------------------------------------------------------------===//
    // Serializable
    //===------------------------------------------------------------------===//

    SerializedData serialize() const override;
    void deserialize(const SerializedData &data) override;
    void reset() override;

protected:

    void setTotalTime(float val);
    void setSeekBeat(float beatPosition);

    void broadcastPlay();
    void broadcastStop();
    void broadcastRecord();
    void broadcastRecordFailed(const Array<MidiDeviceInfo> &devices);
    void broadcastTempoChanged(double newTempo);
    void broadcastTotalTimeChanged(double timeMs);
    void broadcastSeek(float newBeat, double currentTimeMs, double totalTimeMs);

private:
    
    OrchestraPit &orchestra;
    SleepTimer &sleepTimer;

    UniquePointer<PlayerThreadPool> player;
    UniquePointer<RendererThread> renderer;

    friend class PlayerThread;
    friend class PlayerThreadPool;
    friend class RendererThread;

    float getProjectFirstBeat() const noexcept
    {
        return this->projectFirstBeat.get();
    }

    float getProjectLastBeat() const noexcept
    {
        return this->projectLastBeat.get();
    }

private:

    TransportPlaybackCache getPlaybackCache();
    void recacheIfNeeded() const;
    
    mutable TransportPlaybackCache playbackCache;
    mutable Atomic<bool> sequencesAreOutdated = true;
    
    // linksCache is <track id : instrument>
    mutable Array<const MidiTrack *> tracksCache;
    mutable FlatHashMap<String, WeakReference<Instrument>, StringHash> linksCache;

    void updateLinkForTrack(const MidiTrack *track);
    void removeLinkForTrack(const MidiTrack *track);
    
private:

    /*
        The purpose of this class is to help with previewing messages on the fly in piano roll.
        Will simply send messages to queue after a delay, and cancel all pending messages when
        someone calls allNotesControllersAndSoundOff(); that delay is needed because some plugins
        (e.g. Kontakt in my case) are often processing play/stop messages out of order, which is weird -
        note the word `queue` in addMessageToQueue() method name - but it still happens in some cases,
        for example, when the user drags some notes around quickly. Said that, for whatever reason,
        JUCE's message collector cannot be relied upon, and this hack is used instead.
    */
    class MidiMessageDelayedPreview final : private Timer
    {
    public:
        MidiMessageDelayedPreview() = default;
        ~MidiMessageDelayedPreview();
        void cancelPendingPreview();
        void previewMessage(const MidiMessage &message, WeakReference<Instrument> instrument);
    private:
        void timerCallback() override;
        Array<WeakReference<Instrument>> instruments;
        Array<MidiMessage> messages;
    };

    mutable MidiMessageDelayedPreview messagePreviewQueue;

private:

    Atomic<float> seekBeat = 0.0;
    Atomic<float> totalTime = PROJECT_DEFAULT_NUM_BEATS;
    
    Atomic<float> projectFirstBeat = 0.f;
    Atomic<float> projectLastBeat = PROJECT_DEFAULT_NUM_BEATS;

    // just a flag, all logic resides in MidiRecorder class:
    Atomic<bool> midiRecordingMode = false;

    ListenerList<TransportListener> transportListeners;

    JUCE_DECLARE_WEAK_REFERENCEABLE(Transport)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Transport)
};
