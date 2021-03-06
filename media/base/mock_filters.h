// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A new breed of mock media filters, this time using gmock!  Feel free to add
// actions if you need interesting side-effects.
//
// Don't forget you can use StrictMock<> and NiceMock<> if you want the mock
// filters to fail the test or do nothing when an unexpected method is called.
// http://code.google.com/p/googlemock/wiki/CookBook#Nice_Mocks_and_Strict_Mocks

#ifndef MEDIA_BASE_MOCK_FILTERS_H_
#define MEDIA_BASE_MOCK_FILTERS_H_

#include <string>

#include "base/callback.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_renderer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor.h"
#include "media/base/decryptor_client.h"
#include "media/base/demuxer.h"
#include "media/base/filters.h"
#include "media/base/filter_collection.h"
#include "media/base/pipeline.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_renderer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

// Use this template to test for object destruction by setting expectations on
// the method OnDestroy().
//
// TODO(scherkus): not sure about the naming...  perhaps contribute this back
// to gmock itself!
template<class MockClass>
class Destroyable : public MockClass {
 public:
  Destroyable() {}

  MOCK_METHOD0(OnDestroy, void());

 protected:
  virtual ~Destroyable() {
    OnDestroy();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Destroyable);
};

// TODO(scherkus): remove when CompositeFilter is removed, see
// http://crbug.com/126069
class MockFilter : public Filter {
 public:
  MockFilter();

  // Filter implementation.
  void SetHost(FilterHost* host) OVERRIDE;
  MOCK_METHOD1(Play, void(const base::Closure& callback));
  MOCK_METHOD1(Pause, void(const base::Closure& callback));
  MOCK_METHOD1(Flush, void(const base::Closure& callback));
  MOCK_METHOD1(Stop, void(const base::Closure& callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, const PipelineStatusCB& cb));

  FilterHost* host() { return host_; }

 protected:
  virtual ~MockFilter();

 private:
  FilterHost* host_;

  DISALLOW_COPY_AND_ASSIGN(MockFilter);
};

class MockDemuxer : public Demuxer {
 public:
  MockDemuxer();

  // Demuxer implementation.
  MOCK_METHOD2(Initialize, void(DemuxerHost* host, const PipelineStatusCB& cb));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, const PipelineStatusCB& cb));
  MOCK_METHOD1(Stop, void(const base::Closure& callback));
  MOCK_METHOD0(OnAudioRendererDisabled, void());
  MOCK_METHOD1(GetStream, scoped_refptr<DemuxerStream>(DemuxerStream::Type));
  MOCK_CONST_METHOD0(GetStartTime, base::TimeDelta());
  MOCK_METHOD0(GetBitrate, int());

 protected:
  virtual ~MockDemuxer();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDemuxer);
};

class MockDemuxerStream : public DemuxerStream {
 public:
  MockDemuxerStream();

  // DemuxerStream implementation.
  MOCK_METHOD0(type, Type());
  MOCK_METHOD1(Read, void(const ReadCB& read_cb));
  MOCK_METHOD0(audio_decoder_config, const AudioDecoderConfig&());
  MOCK_METHOD0(video_decoder_config, const VideoDecoderConfig&());
  MOCK_METHOD0(EnableBitstreamConverter, void());
  MOCK_METHOD0(GetBufferedRanges, Ranges<base::TimeDelta>());

 protected:
  virtual ~MockDemuxerStream();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDemuxerStream);
};

class MockVideoDecoder : public VideoDecoder {
 public:
  MockVideoDecoder();

  // VideoDecoder implementation.
  MOCK_METHOD3(Initialize, void(const scoped_refptr<DemuxerStream>&,
                                const PipelineStatusCB&,
                                const StatisticsCB&));
  MOCK_METHOD1(Read, void(const ReadCB&));
  MOCK_METHOD1(Reset, void(const base::Closure&));
  MOCK_METHOD1(Stop, void(const base::Closure&));
  MOCK_METHOD0(natural_size, const gfx::Size&());
  MOCK_CONST_METHOD0(HasAlpha, bool());

 protected:
  virtual ~MockVideoDecoder();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoDecoder);
};

class MockAudioDecoder : public AudioDecoder {
 public:
  MockAudioDecoder();

  // AudioDecoder implementation.
  MOCK_METHOD3(Initialize, void(const scoped_refptr<DemuxerStream>&,
                                const PipelineStatusCB&,
                                const StatisticsCB&));
  MOCK_METHOD1(Read, void(const ReadCB&));
  MOCK_METHOD1(ProduceAudioSamples, void(scoped_refptr<Buffer>));
  MOCK_METHOD0(bits_per_channel, int(void));
  MOCK_METHOD0(channel_layout, ChannelLayout(void));
  MOCK_METHOD0(samples_per_second, int(void));
  MOCK_METHOD1(Reset, void(const base::Closure&));

 protected:
  virtual ~MockAudioDecoder();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioDecoder);
};

class MockVideoRenderer : public VideoRenderer {
 public:
  MockVideoRenderer();

  // Filter implementation.
  MOCK_METHOD1(SetHost, void(FilterHost* host));
  MOCK_METHOD1(Play, void(const base::Closure& callback));
  MOCK_METHOD1(Pause, void(const base::Closure& callback));
  MOCK_METHOD1(Flush, void(const base::Closure& callback));
  MOCK_METHOD1(Stop, void(const base::Closure& callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, const PipelineStatusCB& cb));

  // VideoRenderer implementation.
  MOCK_METHOD4(Initialize, void(const scoped_refptr<VideoDecoder>& decoder,
                                const PipelineStatusCB& status_cb,
                                const StatisticsCB& statistics_cb,
                                const TimeCB& time_cb));

  MOCK_METHOD0(HasEnded, bool());

  // TODO(scherkus): although VideoRendererBase defines this method, this really
  // shouldn't be here OR should be renamed.
  MOCK_METHOD1(ConsumeVideoFrame, void(scoped_refptr<VideoFrame> frame));

 protected:
  virtual ~MockVideoRenderer();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoRenderer);
};

class MockAudioRenderer : public AudioRenderer {
 public:
  MockAudioRenderer();

  // AudioRenderer implementation.
  MOCK_METHOD7(Initialize, void(const scoped_refptr<AudioDecoder>& decoder,
                                const PipelineStatusCB& init_cb,
                                const base::Closure& underflow_cb,
                                const TimeCB& time_cb,
                                const base::Closure& ended_cb,
                                const base::Closure& disabled_cb,
                                const PipelineStatusCB& error_cb));
  MOCK_METHOD1(Play, void(const base::Closure& callback));
  MOCK_METHOD1(Pause, void(const base::Closure& callback));
  MOCK_METHOD1(Flush, void(const base::Closure& callback));
  MOCK_METHOD1(Stop, void(const base::Closure& callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, const PipelineStatusCB& cb));
  MOCK_METHOD0(HasEnded, bool());
  MOCK_METHOD1(SetVolume, void(float volume));
  MOCK_METHOD1(ResumeAfterUnderflow, void(bool buffer_more_audio));

 protected:
  virtual ~MockAudioRenderer();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioRenderer);
};

class MockDecryptor : public Decryptor {
 public:
  MockDecryptor();
  virtual ~MockDecryptor();

  MOCK_METHOD3(GenerateKeyRequest, void(const std::string& key_system,
                                        const uint8* init_data,
                                        int init_data_length));
  MOCK_METHOD6(AddKey, void(const std::string& key_system,
                            const uint8* key,
                            int key_length,
                            const uint8* init_data,
                            int init_data_length,
                            const std::string& session_id));
  MOCK_METHOD2(CancelKeyRequest, void(const std::string& key_system,
                                      const std::string& session_id));
  MOCK_METHOD2(Decrypt, void(const scoped_refptr<DecoderBuffer>& encrypted,
                             const DecryptCB& decrypt_cb));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDecryptor);
};

class MockDecryptorClient : public DecryptorClient {
 public:
  MockDecryptorClient();
  virtual ~MockDecryptorClient();

  MOCK_METHOD2(KeyAdded, void(const std::string&, const std::string&));
  MOCK_METHOD4(KeyError, void(const std::string&, const std::string&,
                              Decryptor::KeyError, int));
  // TODO(xhwang): This is a workaround of the issue that move-only parameters
  // are not supported in mocked methods. Remove this when the issue is fixed
  // (http://code.google.com/p/googletest/issues/detail?id=395) or when we use
  // std::string instead of scoped_array<uint8> (http://crbug.com/130689).
  MOCK_METHOD5(KeyMessageMock, void(const std::string& key_system,
                                    const std::string& session_id,
                                    const uint8* message,
                                    int message_length,
                                    const std::string& default_url));
  MOCK_METHOD4(NeedKeyMock, void(const std::string& key_system,
                                 const std::string& session_id,
                                 const uint8* init_data,
                                 int init_data_length));
  virtual void KeyMessage(const std::string& key_system,
                          const std::string& session_id,
                          scoped_array<uint8> message,
                          int message_length,
                          const std::string& default_url) OVERRIDE;
  virtual void NeedKey(const std::string& key_system,
                       const std::string& session_id,
                       scoped_array<uint8> init_data,
                       int init_data_length) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDecryptorClient);
};

// FilterFactory that returns canned instances of mock filters.  You can set
// expectations on the filters and then pass the collection into a pipeline.
class MockFilterCollection {
 public:
  MockFilterCollection();
  virtual ~MockFilterCollection();

  // Mock accessors.
  MockDemuxer* demuxer() const { return demuxer_; }
  MockVideoDecoder* video_decoder() const { return video_decoder_; }
  MockAudioDecoder* audio_decoder() const { return audio_decoder_; }
  MockVideoRenderer* video_renderer() const { return video_renderer_; }
  MockAudioRenderer* audio_renderer() const { return audio_renderer_; }

  // Creates the FilterCollection containing the mocks.
  scoped_ptr<FilterCollection> Create();

 private:
  scoped_refptr<MockDemuxer> demuxer_;
  scoped_refptr<MockVideoDecoder> video_decoder_;
  scoped_refptr<MockAudioDecoder> audio_decoder_;
  scoped_refptr<MockVideoRenderer> video_renderer_;
  scoped_refptr<MockAudioRenderer> audio_renderer_;

  DISALLOW_COPY_AND_ASSIGN(MockFilterCollection);
};

// Helper gmock action that calls SetError() on behalf of the provided filter.
ACTION_P2(SetError, filter, error) {
  filter->host()->SetError(error);
}

// Helper gmock action that calls SetDuration() on behalf of the provided
// filter.
ACTION_P2(SetDuration, filter, duration) {
  filter->host()->SetDuration(duration);
}

ACTION(RunClosure) {
  arg0.Run();
}

// Helper mock statistics callback.
class MockStatisticsCB {
 public:
  MockStatisticsCB();
  ~MockStatisticsCB();

  MOCK_METHOD1(OnStatistics, void(const media::PipelineStatistics& statistics));
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_FILTERS_H_
