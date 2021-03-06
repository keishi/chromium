// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mp4/mp4_stream_parser.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/mp4/box_definitions.h"
#include "media/mp4/box_reader.h"
#include "media/mp4/es_descriptor.h"
#include "media/mp4/rcheck.h"

namespace media {
namespace mp4 {

MP4StreamParser::MP4StreamParser(bool has_sbr)
    : state_(kWaitingForInit),
      moof_head_(0),
      mdat_tail_(0),
      has_audio_(false),
      has_video_(false),
      audio_track_id_(0),
      video_track_id_(0),
      has_sbr_(has_sbr) {
}

MP4StreamParser::~MP4StreamParser() {}

void MP4StreamParser::Init(const InitCB& init_cb,
                           const NewConfigCB& config_cb,
                           const NewBuffersCB& audio_cb,
                           const NewBuffersCB& video_cb,
                           const NeedKeyCB& need_key_cb,
                           const NewMediaSegmentCB& new_segment_cb) {
  DCHECK_EQ(state_, kWaitingForInit);
  DCHECK(init_cb_.is_null());
  DCHECK(!init_cb.is_null());
  DCHECK(!config_cb.is_null());
  DCHECK(!audio_cb.is_null() || !video_cb.is_null());
  DCHECK(!need_key_cb.is_null());

  ChangeState(kParsingBoxes);
  init_cb_ = init_cb;
  config_cb_ = config_cb;
  audio_cb_ = audio_cb;
  video_cb_ = video_cb;
  need_key_cb_ = need_key_cb;
  new_segment_cb_ = new_segment_cb;
}

void MP4StreamParser::Flush() {
  DCHECK_NE(state_, kWaitingForInit);

  queue_.Reset();
  moof_head_ = 0;
  mdat_tail_ = 0;
}

bool MP4StreamParser::Parse(const uint8* buf, int size) {
  DCHECK_NE(state_, kWaitingForInit);

  if (state_ == kError)
    return false;

  queue_.Push(buf, size);

  BufferQueue audio_buffers;
  BufferQueue video_buffers;

  bool result, err = false;

  do {
    if (state_ == kParsingBoxes) {
      if (mdat_tail_ > queue_.head()) {
        result = queue_.Trim(mdat_tail_);
      } else {
        result = ParseBox(&err);
      }
    } else {
      DCHECK_EQ(kEmittingSamples, state_);
      result = EnqueueSample(&audio_buffers, &video_buffers, &err);
      if (result) {
        int64 max_clear = runs_->GetMaxClearOffset() + moof_head_;
        DCHECK(max_clear <= queue_.tail());
        err = !(ReadMDATsUntil(max_clear) && queue_.Trim(max_clear));
      }
    }
  } while (result && !err);

  if (!err)
    err = !SendAndFlushSamples(&audio_buffers, &video_buffers);

  if (err) {
    DLOG(ERROR) << "Error while parsing MP4";
    queue_.Reset();
    moov_.reset();
    runs_.reset();
    ChangeState(kError);
    return false;
  }

  return true;
}

bool MP4StreamParser::ParseBox(bool* err) {
  const uint8* buf;
  int size;
  queue_.Peek(&buf, &size);
  if (!size) return false;

  scoped_ptr<BoxReader> reader(BoxReader::ReadTopLevelBox(buf, size, err));
  if (reader.get() == NULL) return false;

  if (reader->type() == FOURCC_MOOV) {
    *err = !ParseMoov(reader.get());
  } else if (reader->type() == FOURCC_MOOF) {
    moof_head_ = queue_.head();
    *err = !ParseMoof(reader.get());

    // Set up first mdat offset for ParseMDATsUntil()
    mdat_tail_ = queue_.head() + reader->size();
  } else {
    DVLOG(2) << "Skipping unrecognized top-level box: "
             << FourCCToString(reader->type());
  }

  queue_.Pop(reader->size());
  return !(*err);
}


bool MP4StreamParser::ParseMoov(BoxReader* reader) {
  // TODO(strobe): Respect edit lists.
  moov_.reset(new Movie);
  RCHECK(moov_->Parse(reader));
  runs_.reset(new TrackRunIterator(moov_.get()));

  has_audio_ = false;
  has_video_ = false;

  AudioDecoderConfig audio_config;
  VideoDecoderConfig video_config;

  for (std::vector<Track>::const_iterator track = moov_->tracks.begin();
       track != moov_->tracks.end(); ++track) {
    // TODO(strobe): Only the first audio and video track present in a file are
    // used. (Track selection is better accomplished via Source IDs, though, so
    // adding support for track selection within a stream is low-priority.)
    const SampleDescription& samp_descr =
        track->media.information.sample_table.description;

    // TODO(strobe): When codec reconfigurations are supported, detect and send
    // a codec reconfiguration for fragments using a sample description index
    // different from the previous one
    size_t desc_idx = 0;
    for (size_t t = 0; t < moov_->extends.tracks.size(); t++) {
      const TrackExtends& trex = moov_->extends.tracks[t];
      if (trex.track_id == track->header.track_id) {
        desc_idx = trex.default_sample_description_index;
        break;
      }
    }
    RCHECK(desc_idx > 0);
    desc_idx -= 1;  // BMFF descriptor index is one-based

    if (track->media.handler.type == kAudio && !audio_config.IsValidConfig()) {
      RCHECK(!samp_descr.audio_entries.empty());

      // It is not uncommon to find otherwise-valid files with incorrect sample
      // description indices, so we fail gracefully in that case.
      if (static_cast<uint32>(desc_idx) >= samp_descr.audio_entries.size())
        desc_idx = 0;
      const AudioSampleEntry& entry = samp_descr.audio_entries[desc_idx];
      const AAC& aac = entry.esds.aac;

      // TODO(strobe): We accept all format values, pending clarification on
      // the formats used for encrypted media (http://crbug.com/132351).
      //  RCHECK(entry.format == FOURCC_MP4A ||
      //         (entry.format == FOURCC_ENCA &&
      //          entry.sinf.format.format == FOURCC_MP4A));

      // Check if it is MPEG4 AAC defined in ISO 14496 Part 3.
      RCHECK(entry.esds.object_type == kISO_14496_3);
      audio_config.Initialize(kCodecAAC, entry.samplesize,
                              aac.channel_layout(),
                              aac.GetOutputSamplesPerSecond(has_sbr_),
                              NULL, 0, false);
      has_audio_ = true;
      audio_track_id_ = track->header.track_id;
    }
    if (track->media.handler.type == kVideo && !video_config.IsValidConfig()) {
      RCHECK(!samp_descr.video_entries.empty());
      if (static_cast<uint32>(desc_idx) >= samp_descr.video_entries.size())
        desc_idx = 0;
      const VideoSampleEntry& entry = samp_descr.video_entries[desc_idx];

      //  RCHECK(entry.format == FOURCC_AVC1 ||
      //         (entry.format == FOURCC_ENCV &&
      //          entry.sinf.format.format == FOURCC_AVC1));

      // TODO(strobe): Recover correct crop box
      video_config.Initialize(kCodecH264, H264PROFILE_MAIN,  VideoFrame::YV12,
                              gfx::Size(entry.width, entry.height),
                              gfx::Rect(0, 0, entry.width, entry.height),
                              // Framerate of zero is provided to signal that
                              // the decoder should trust demuxer timestamps
                              0, 1,
                              entry.pixel_aspect.h_spacing,
                              entry.pixel_aspect.v_spacing,
                              // No decoder-specific buffer needed for AVC;
                              // SPS/PPS are embedded in the video stream
                              NULL, 0, false);
      has_video_ = true;
      video_track_id_ = track->header.track_id;
    }
  }

  // TODO(strobe): For now, we avoid sending new configs on a new
  // reinitialization segment, and instead simply embed the updated parameter
  // sets into the video stream. The conditional should be removed when
  // http://crbug.com/122913 is fixed.
  if (!init_cb_.is_null())
    RCHECK(config_cb_.Run(audio_config, video_config));

  base::TimeDelta duration;
  if (moov_->extends.header.fragment_duration > 0) {
    duration = TimeDeltaFromFrac(moov_->extends.header.fragment_duration,
                                 moov_->header.timescale);
  } else if (moov_->header.duration > 0) {
    duration = TimeDeltaFromFrac(moov_->header.duration,
                                 moov_->header.timescale);
  } else {
    duration = kInfiniteDuration();
  }

  if (!init_cb_.is_null())
    base::ResetAndReturn(&init_cb_).Run(true, duration);
  return true;
}

bool MP4StreamParser::ParseMoof(BoxReader* reader) {
  RCHECK(moov_.get());  // Must already have initialization segment
  MovieFragment moof;
  RCHECK(moof.Parse(reader));
  RCHECK(runs_->Init(moof));
  new_segment_cb_.Run(runs_->GetMinDecodeTimestamp());
  ChangeState(kEmittingSamples);
  return true;
}

bool MP4StreamParser::EnqueueSample(BufferQueue* audio_buffers,
                                    BufferQueue* video_buffers,
                                    bool* err) {
  if (!runs_->RunIsValid()) {
    // Flush any buffers we've gotten in this chunk so that buffers don't
    // cross NewSegment() calls
    *err = !SendAndFlushSamples(audio_buffers, video_buffers);
    if (*err) return false;
    ChangeState(kParsingBoxes);
    return true;
  }

  if (!runs_->SampleIsValid()) {
    runs_->AdvanceRun();
    return true;
  }

  DCHECK(!(*err));

  const uint8* buf;
  int size;
  queue_.Peek(&buf, &size);
  if (!size) return false;

  bool audio = has_audio_ && audio_track_id_ == runs_->track_id();
  bool video = has_video_ && video_track_id_ == runs_->track_id();

  // Skip this entire track if it's not one we're interested in
  if (!audio && !video) runs_->AdvanceRun();

  queue_.PeekAt(runs_->sample_offset() + moof_head_, &buf, &size);
  if (size < runs_->sample_size()) return false;

  std::vector<uint8> frame_buf(buf, buf + runs_->sample_size());
  if (video) {
    const AVCDecoderConfigurationRecord& avc_config =
        runs_->video_description().avcc;
    RCHECK(AVC::ConvertToAnnexB(avc_config.length_size, &frame_buf));
    if (runs_->is_keyframe())
      RCHECK(AVC::InsertParameterSets(avc_config, &frame_buf));
  }

  if (audio) {
    const AAC& aac = runs_->audio_description().esds.aac;
    RCHECK(aac.ConvertEsdsToADTS(&frame_buf));
  }

  scoped_refptr<StreamParserBuffer> stream_buf =
    StreamParserBuffer::CopyFrom(&frame_buf[0], frame_buf.size(),
                                 runs_->is_keyframe());

  stream_buf->SetDuration(runs_->duration());
  stream_buf->SetTimestamp(runs_->cts());
  stream_buf->SetDecodeTimestamp(runs_->dts());

  DVLOG(3) << "Pushing frame: aud=" << audio
           << ", key=" << runs_->is_keyframe()
           << ", dur=" << runs_->duration().InMilliseconds()
           << ", dts=" << runs_->dts().InMilliseconds()
           << ", cts=" << runs_->cts().InMilliseconds()
           << ", size=" << runs_->sample_size();

  if (audio) {
    audio_buffers->push_back(stream_buf);
  } else {
    video_buffers->push_back(stream_buf);
  }

  runs_->AdvanceSample();
  return true;
}

bool MP4StreamParser::SendAndFlushSamples(BufferQueue* audio_buffers,
                                          BufferQueue* video_buffers) {
  if (!audio_buffers->empty()) {
    if (audio_cb_.is_null() || !audio_cb_.Run(*audio_buffers))
      return false;
    audio_buffers->clear();
  }
  if (!video_buffers->empty()) {
    if (video_cb_.is_null() || !video_cb_.Run(*video_buffers))
      return false;
    video_buffers->clear();
  }
  return true;
}

bool MP4StreamParser::ReadMDATsUntil(const int64 tgt_offset) {
  DCHECK(tgt_offset <= queue_.tail());

  while (mdat_tail_ < tgt_offset) {
    const uint8* buf;
    int size;
    queue_.PeekAt(mdat_tail_, &buf, &size);

    FourCC type;
    int box_sz;
    bool err;
    if (!BoxReader::StartTopLevelBox(buf, size, &type, &box_sz, &err))
      return false;

    if (type != FOURCC_MDAT) {
      DLOG(WARNING) << "Unexpected type while parsing MDATs: "
                    << FourCCToString(type);
    }

    mdat_tail_ += box_sz;
  }

  return true;
}

void MP4StreamParser::ChangeState(State new_state) {
  DVLOG(2) << "Changing state: " << new_state;
  state_ = new_state;
}

}  // namespace mp4
}  // namespace media
