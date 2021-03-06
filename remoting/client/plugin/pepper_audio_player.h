// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_AUDIO_PLAYER_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_AUDIO_PLAYER_H_

#include <list>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "ppapi/cpp/audio.h"
#include "ppapi/cpp/instance.h"
#include "remoting/client/audio_player.h"

namespace remoting {

class AudioPacket;

class PepperAudioPlayer : public AudioPlayer {
 public:
  explicit PepperAudioPlayer(pp::Instance* instance);
  virtual ~PepperAudioPlayer();

  // Returns true if successful, false otherwise.
  virtual bool Start() OVERRIDE;

  virtual void ProcessAudioPacket(scoped_ptr<AudioPacket> packet) OVERRIDE;

  virtual bool IsRunning() const OVERRIDE;

 private:
  typedef std::list<AudioPacket*> AudioPacketQueue;

  // Function called by the browser when it needs more audio samples.
  static void PepperAudioPlayerCallback(void* samples,
                                        uint32_t buffer_size,
                                        void* data);

  void FillWithSamples(void* samples, uint32_t buffer_size);

  pp::Audio audio_;

  // The count of sample frames per channel in an audio buffer.
  uint32_t samples_per_frame_;

  // Protects |queued_packets_| and |bytes_consumed_|.
  // This is necessary to prevent races,
  // because Pepper will callback on a separate thread.
  base::Lock lock_;

  AudioPacketQueue queued_packets_;

  // The number of bytes from |queued_packets_| that have been consumed.
  size_t bytes_consumed_;

  bool running_;

  DISALLOW_COPY_AND_ASSIGN(PepperAudioPlayer);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_AUDIO_PLAYER_H_
