//
// Copyright (c) 2008-2018 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once

#include "../Core/Object.h"

namespace Urho3D
{

/// Low-resolution operating system timer.
class URHO3D_API Timer
{
public:
    /// Construct. Get the starting clock value. No timeout.
    Timer();

    /// Construct. Specify duration in milliseconds until the timer times-out.
    Timer(unsigned timeoutDurationMs);

    /// Return elapsed milliseconds and optionally reset. 
    unsigned GetMSec(bool reset);

    /// Return the clock value in milliseconds when the timer was started.
    unsigned GetStartTime();

    /// Sets a new timeout duration in milliseconds.  duration is from the starting time of the timer. Timer will not be timed-out if timeoutDuration is 0. optionally reset.
    void SetTimeoutDuration(unsigned timeoutDurationMs, bool reset = false);

    /// Return the duration in milliseconds for the timeout.
    unsigned GetTimeoutDuration();

    ///Return whether the timer has timed-out (is in over-time)
    bool IsTimedOut();

    /// Reset the timer
    void Reset();

private:
    /// Starting clock value in milliseconds.
    unsigned startTime_{};
    unsigned timeoutDuration_;
};

/// High-resolution operating system timer used in profiling.
class URHO3D_API HiresTimer
{
    friend class Time;

public:
    /// Construct. Get the starting high-resolution clock value.
    HiresTimer();

    /// Construct. Specify duration in microseconds until the timer times-out.
    HiresTimer(long long timeoutDurationUs);

    /// Return elapsed microseconds
    long long GetUSec(bool reset);

    ///Return the microsecond clock value when the timer was started.
    long long GetStartTime();

    /// Sets a new timeout duration in microseconds.  duration is from the starting time of the timer. optionally reset.
    void SetTimeoutDuration(long long timeoutDurationUs, bool reset = false);

    ///Returns the duration for the timeout. 0 if no timeout duration was specified.
    long long GetTimeoutDuration();

    ///Return whether the timer has timed-out.
    bool IsTimedOut();

    /// Reset the timer
    void Reset();

    /// Return if high-resolution timer is supported.
    static bool IsSupported() { return supported; }

    /// Return high-resolution timer frequency if supported.
    static long long GetFrequency() { return frequency; }

    /// converts CPU ticks to microseconds.
    static long long TicksToUSec(long long ticks);

    /// converts microseconds to CPU ticks.
    static long long USecToTicks(long long microseconds);

private:
    /// Starting clock value in CPU ticks.
    long long startTime_{};

    /// Clock ticks until the timer times-out.
    long long timeoutDurationTicks_;

    /// High-resolution timer support flag.
    static bool supported;

    /// High-resolution timer frequency.
    static long long frequency;
};

/// %Time and frame counter subsystem.
class URHO3D_API Time : public Object
{
    URHO3D_OBJECT(Time, Object);

public:
    /// Construct.
    explicit Time(Context* context);
    /// Destruct. Reset the low-resolution timer period if set.
    ~Time() override;

    /// Set the low-resolution timer period in milliseconds. 0 resets to the default period.
    void SetTimerPeriod(unsigned mSec);

    /// Return current low-resolution timer period in milliseconds.
    unsigned GetTimerPeriod() const { return timerPeriod_; }

    /// Return elapsed time from program start as seconds.
    float GetElapsedTime();

    /// Get system time as milliseconds.
    static unsigned GetSystemTime();
    /// Get system time as seconds since 1.1.1970.
    static unsigned GetTimeSinceEpoch();
    /// Get a date/time stamp as a string.
    static String GetTimeStamp();
    /// Sleep for a number of milliseconds.
    static void Sleep(unsigned mSec);

private:
    /// Elapsed time since program start.
    Timer elapsedTime_;
    /// Frame number.
    unsigned frameNumber_;
    /// Timestep in seconds.
    float timeStep_;
    /// Low-resolution timer period.
    unsigned timerPeriod_;
};

}
