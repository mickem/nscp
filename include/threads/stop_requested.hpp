// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <exception>

namespace threads {

// Thrown by a worker step that gave up because its stop signal fired (a
// collector fetch that let go of a stalled provider, an aborted search).
// Deliberately unrelated to any failure type: a collector's "this source is
// broken, disable it / count a failure" handling must never mistake a
// shutdown for an error, and catching this type is how a portable collector
// loop tells the two apart without knowing which platform API was abandoned.
//
// It lives in its own header rather than in stop_signal.hpp so that
// guarded_thread.hpp - included by every module that starts a worker - can
// recognise a clean shutdown without pulling <windows.h> in with it.
class stop_requested : public std::exception {
 public:
  const char *what() const noexcept override { return "stop requested"; }
};

}  // namespace threads
