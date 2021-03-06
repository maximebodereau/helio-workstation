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

class TransportListener
{
public:
    
    virtual ~TransportListener() {}

    // expect that any data in these three methods could come
    // from a separate thread (PlayerThread most likely)
    virtual void onSeek(float beatPosition, double currentTimeMs, double totalTimeMs) = 0;
    virtual void onTempoChanged(double msPerQuarter) = 0;
    virtual void onTotalTimeChanged(double timeMs) = 0;

    // these ones will be called with message manager locked
    virtual void onPlay() = 0;
    virtual void onStop() = 0;
    virtual void onRecord() = 0;
    virtual void onRecordFailed(const Array<MidiDeviceInfo> &devices) {}

};
