// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <string>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/memory/singleton.h"
#include "base/string_util.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/limits.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filters.h"
#include "media/base/test_data_util.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_decoder_unittest.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::IsNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

static const VideoFrame::Format kVideoFormat = VideoFrame::YV12;
static const gfx::Size kCodedSize(320, 240);
static const gfx::Rect kVisibleRect(320, 240);
static const AVRational kFrameRate = { 100, 1 };
static const AVRational kAspectRatio = { 1, 1 };

static const uint8 kKeyId[] = { 0x4b, 0x65, 0x79, 0x20, 0x49, 0x44 };
static const uint8 kSecretKey[] = {
  0xfb, 0x67, 0x8a, 0x91, 0x19, 0x12, 0x7b, 0x6b,
  0x0b, 0x63, 0x11, 0xf8, 0x6f, 0xe1, 0xc4, 0x2d
};

static const uint64 kIv = 3735928559ULL;
static const uint8 kHmac[] = {
  0x16, 0xc0, 0x65, 0x1f, 0xf8, 0x0b, 0x36, 0x16,
  0xb8, 0x32, 0x35, 0x56
};

ACTION_P(ReturnBuffer, buffer) {
  arg0.Run(buffer ? DemuxerStream::kOk : DemuxerStream::kAborted, buffer);
}

ACTION_P2(RunDecryptCB, status, buffer) {
  arg1.Run(status, buffer);
}

class FFmpegVideoDecoderTest : public testing::Test {
 public:
  FFmpegVideoDecoderTest()
      : decryptor_(new MockDecryptor()),
        decoder_(new FFmpegVideoDecoder(base::Bind(&Identity<MessageLoop*>,
                                                   &message_loop_))),
        demuxer_(new StrictMock<MockDemuxerStream>()),
        read_cb_(base::Bind(&FFmpegVideoDecoderTest::FrameReady,
                            base::Unretained(this))) {
    CHECK(FFmpegGlue::GetInstance());

    decoder_->set_decryptor(decryptor_.get());

    // Initialize various test buffers.
    frame_buffer_.reset(new uint8[kCodedSize.GetArea()]);
    end_of_stream_buffer_ = DecoderBuffer::CreateEOSBuffer();
    i_frame_buffer_ = ReadTestDataFile("vp8-I-frame-320x240");
    corrupt_i_frame_buffer_ = ReadTestDataFile("vp8-corrupt-I-frame");
    {
      scoped_refptr<DecoderBuffer> temp_buffer = ReadTestDataFile(
          "vp8-encrypted-I-frame-320x240");
      encrypted_i_frame_buffer_ = DecoderBuffer::CopyFrom(
          temp_buffer->GetData() + arraysize(kHmac),
          temp_buffer->GetDataSize() - arraysize(kHmac));
    }

    config_.Initialize(kCodecVP8, VIDEO_CODEC_PROFILE_UNKNOWN,
                       kVideoFormat, kCodedSize, kVisibleRect,
                       kFrameRate.num, kFrameRate.den,
                       kAspectRatio.num, kAspectRatio.den,
                       NULL, 0, true);
  }

  virtual ~FFmpegVideoDecoderTest() {}

  void Initialize() {
    InitializeWithConfig(config_);
  }

  void InitializeWithConfigAndStatus(const VideoDecoderConfig& config,
                                     PipelineStatus status) {
    EXPECT_CALL(*demuxer_, video_decoder_config())
        .WillOnce(ReturnRef(config));

    decoder_->Initialize(demuxer_, NewExpectedStatusCB(status),
                         base::Bind(&MockStatisticsCB::OnStatistics,
                                    base::Unretained(&statistics_cb_)));

    message_loop_.RunAllPending();
  }

  void InitializeWithConfig(const VideoDecoderConfig& config) {
    InitializeWithConfigAndStatus(config, PIPELINE_OK);
  }

  void Reset() {
    decoder_->Reset(NewExpectedClosure());
    message_loop_.RunAllPending();
  }

  void Stop() {
    decoder_->Stop(NewExpectedClosure());
    message_loop_.RunAllPending();
  }

  // Sets up expectations and actions to put FFmpegVideoDecoder in an active
  // decoding state.
  void EnterDecodingState() {
    VideoDecoder::DecoderStatus status;
    scoped_refptr<VideoFrame> video_frame;
    DecodeSingleFrame(i_frame_buffer_, &status, &video_frame);

    EXPECT_EQ(status, VideoDecoder::kOk);
    ASSERT_TRUE(video_frame);
    EXPECT_FALSE(video_frame->IsEndOfStream());
  }

  // Sets up expectations and actions to put FFmpegVideoDecoder in an end
  // of stream state.
  void EnterEndOfStreamState() {
    scoped_refptr<VideoFrame> video_frame;
    VideoDecoder::DecoderStatus status;
    Read(&status, &video_frame);
    EXPECT_EQ(status, VideoDecoder::kOk);
    ASSERT_TRUE(video_frame);
    EXPECT_TRUE(video_frame->IsEndOfStream());
  }

  // Decodes the single compressed frame in |buffer| and writes the
  // uncompressed output to |video_frame|. This method works with single
  // and multithreaded decoders. End of stream buffers are used to trigger
  // the frame to be returned in the multithreaded decoder case.
  void DecodeSingleFrame(const scoped_refptr<DecoderBuffer>& buffer,
                         VideoDecoder::DecoderStatus* status,
                         scoped_refptr<VideoFrame>* video_frame) {
    EXPECT_CALL(*demuxer_, Read(_))
        .WillOnce(ReturnBuffer(buffer))
        .WillRepeatedly(ReturnBuffer(end_of_stream_buffer_));

    EXPECT_CALL(statistics_cb_, OnStatistics(_));

    Read(status, video_frame);
  }

  // Decodes |i_frame_buffer_| and then decodes the data contained in
  // the file named |test_file_name|. This function expects both buffers
  // to decode to frames that are the same size.
  void DecodeIFrameThenTestFile(const std::string& test_file_name,
                                size_t expected_width,
                                size_t expected_height) {
    Initialize();

    VideoDecoder::DecoderStatus status_a;
    VideoDecoder::DecoderStatus status_b;
    scoped_refptr<VideoFrame> video_frame_a;
    scoped_refptr<VideoFrame> video_frame_b;

    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(test_file_name);

    EXPECT_CALL(*demuxer_, Read(_))
        .WillOnce(ReturnBuffer(i_frame_buffer_))
        .WillOnce(ReturnBuffer(buffer))
        .WillRepeatedly(ReturnBuffer(end_of_stream_buffer_));

    EXPECT_CALL(statistics_cb_, OnStatistics(_))
        .Times(2);

    Read(&status_a, &video_frame_a);
    Read(&status_b, &video_frame_b);

    size_t original_width = static_cast<size_t>(kVisibleRect.width());
    size_t original_height = static_cast<size_t>(kVisibleRect.height());

    EXPECT_EQ(status_a, VideoDecoder::kOk);
    EXPECT_EQ(status_b, VideoDecoder::kOk);
    ASSERT_TRUE(video_frame_a);
    ASSERT_TRUE(video_frame_b);
    EXPECT_EQ(original_width, video_frame_a->width());
    EXPECT_EQ(original_height, video_frame_a->height());
    EXPECT_EQ(expected_width, video_frame_b->width());
    EXPECT_EQ(expected_height, video_frame_b->height());
  }

  void Read(VideoDecoder::DecoderStatus* status,
            scoped_refptr<VideoFrame>* video_frame) {
    EXPECT_CALL(*this, FrameReady(_, _))
        .WillOnce(DoAll(SaveArg<0>(status), SaveArg<1>(video_frame)));

    decoder_->Read(read_cb_);

    message_loop_.RunAllPending();
  }

  // Generates a 16 byte CTR counter block. The CTR counter block format is a
  // CTR IV appended with a CTR block counter. |iv| is an 8 byte CTR IV.
  static scoped_array<uint8> GenerateCounterBlock(uint64 iv) {
    scoped_array<uint8> counter_block_data(
        new uint8[DecryptConfig::kDecryptionKeySize]);

    // Set the IV.
    memcpy(counter_block_data.get(), &iv, sizeof(iv));

    // Set block counter to all 0's.
    memset(counter_block_data.get() + sizeof(iv),
           0,
           DecryptConfig::kDecryptionKeySize - sizeof(iv));

    return counter_block_data.Pass();
  }


  MOCK_METHOD2(FrameReady, void(VideoDecoder::DecoderStatus,
                                const scoped_refptr<VideoFrame>&));

  MessageLoop message_loop_;
  scoped_ptr<MockDecryptor> decryptor_;
  scoped_refptr<FFmpegVideoDecoder> decoder_;
  scoped_refptr<StrictMock<MockDemuxerStream> > demuxer_;
  MockStatisticsCB statistics_cb_;
  VideoDecoderConfig config_;

  VideoDecoder::ReadCB read_cb_;

  // Various buffers for testing.
  scoped_array<uint8_t> frame_buffer_;
  scoped_refptr<DecoderBuffer> end_of_stream_buffer_;
  scoped_refptr<DecoderBuffer> i_frame_buffer_;
  scoped_refptr<DecoderBuffer> corrupt_i_frame_buffer_;
  scoped_refptr<DecoderBuffer> encrypted_i_frame_buffer_;

  // Used for generating timestamped buffers.
  std::deque<int64> timestamps_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecoderTest);
};

TEST_F(FFmpegVideoDecoderTest, Initialize_Normal) {
  Initialize();
}

TEST_F(FFmpegVideoDecoderTest, Initialize_UnsupportedDecoder) {
  // Test avcodec_find_decoder() returning NULL.
  VideoDecoderConfig config(kUnknownVideoCodec, VIDEO_CODEC_PROFILE_UNKNOWN,
                            kVideoFormat,
                            kCodedSize, kVisibleRect,
                            kFrameRate.num, kFrameRate.den,
                            kAspectRatio.num, kAspectRatio.den,
                            NULL, 0);
  InitializeWithConfigAndStatus(config, PIPELINE_ERROR_DECODE);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_UnsupportedPixelFormat) {
  // Ensure decoder handles unsupport pixel formats without crashing.
  VideoDecoderConfig config(kCodecVP8, VIDEO_CODEC_PROFILE_UNKNOWN,
                            VideoFrame::INVALID,
                            kCodedSize, kVisibleRect,
                            kFrameRate.num, kFrameRate.den,
                            kAspectRatio.num, kAspectRatio.den,
                            NULL, 0);
  InitializeWithConfigAndStatus(config, PIPELINE_ERROR_DECODE);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_OpenDecoderFails) {
  // Specify Theora w/o extra data so that avcodec_open2() fails.
  VideoDecoderConfig config(kCodecTheora, VIDEO_CODEC_PROFILE_UNKNOWN,
                            kVideoFormat,
                            kCodedSize, kVisibleRect,
                            kFrameRate.num, kFrameRate.den,
                            kAspectRatio.num, kAspectRatio.den,
                            NULL, 0);
  InitializeWithConfigAndStatus(config, PIPELINE_ERROR_DECODE);
}

TEST_F(FFmpegVideoDecoderTest, DecodeFrame_Normal) {
  Initialize();

  // Simulate decoding a single frame.
  VideoDecoder::DecoderStatus status;
  scoped_refptr<VideoFrame> video_frame;
  DecodeSingleFrame(i_frame_buffer_, &status, &video_frame);

  EXPECT_EQ(status, VideoDecoder::kOk);
  ASSERT_TRUE(video_frame);
  EXPECT_FALSE(video_frame->IsEndOfStream());
}

// Verify current behavior for 0 byte frames. FFmpeg simply ignores
// the 0 byte frames.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_0ByteFrame) {
  Initialize();

  scoped_refptr<DecoderBuffer> zero_byte_buffer = new DecoderBuffer(0);

  VideoDecoder::DecoderStatus status_a;
  VideoDecoder::DecoderStatus status_b;
  VideoDecoder::DecoderStatus status_c;
  scoped_refptr<VideoFrame> video_frame_a;
  scoped_refptr<VideoFrame> video_frame_b;
  scoped_refptr<VideoFrame> video_frame_c;

  EXPECT_CALL(*demuxer_, Read(_))
      .WillOnce(ReturnBuffer(i_frame_buffer_))
      .WillOnce(ReturnBuffer(zero_byte_buffer))
      .WillOnce(ReturnBuffer(i_frame_buffer_))
      .WillRepeatedly(ReturnBuffer(end_of_stream_buffer_));

  EXPECT_CALL(statistics_cb_, OnStatistics(_))
      .Times(2);

  Read(&status_a, &video_frame_a);
  Read(&status_b, &video_frame_b);
  Read(&status_c, &video_frame_c);

  EXPECT_EQ(status_a, VideoDecoder::kOk);
  EXPECT_EQ(status_b, VideoDecoder::kOk);
  EXPECT_EQ(status_c, VideoDecoder::kOk);

  ASSERT_TRUE(video_frame_a);
  ASSERT_TRUE(video_frame_b);
  ASSERT_TRUE(video_frame_c);

  EXPECT_FALSE(video_frame_a->IsEndOfStream());
  EXPECT_FALSE(video_frame_b->IsEndOfStream());
  EXPECT_TRUE(video_frame_c->IsEndOfStream());
}

TEST_F(FFmpegVideoDecoderTest, DecodeFrame_DecodeError) {
  Initialize();

  EXPECT_CALL(*demuxer_, Read(_))
      .WillOnce(ReturnBuffer(corrupt_i_frame_buffer_))
      .WillRepeatedly(ReturnBuffer(i_frame_buffer_));

  // The error is only raised on the second decode attempt, so we expect at
  // least one successful decode but we don't expect FrameReady() to be
  // executed as an error is raised instead.
  EXPECT_CALL(statistics_cb_, OnStatistics(_));

  // Our read should still get satisfied with end of stream frame during an
  // error.
  VideoDecoder::DecoderStatus status;
  scoped_refptr<VideoFrame> video_frame;
  Read(&status, &video_frame);
  EXPECT_EQ(status, VideoDecoder::kDecodeError);
  EXPECT_FALSE(video_frame);

  message_loop_.RunAllPending();
}

// Multi-threaded decoders have different behavior than single-threaded
// decoders at the end of the stream. Multithreaded decoders hide errors
// that happen on the last |codec_context_->thread_count| frames to avoid
// prematurely signalling EOS. This test just exposes that behavior so we can
// detect if it changes.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_DecodeErrorAtEndOfStream) {
  Initialize();

  VideoDecoder::DecoderStatus status;
  scoped_refptr<VideoFrame> video_frame;
  DecodeSingleFrame(corrupt_i_frame_buffer_, &status, &video_frame);

  EXPECT_EQ(status, VideoDecoder::kOk);
  ASSERT_TRUE(video_frame);
  EXPECT_TRUE(video_frame->IsEndOfStream());
}

// Decode |i_frame_buffer_| and then a frame with a larger width and verify
// the output size was adjusted.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_LargerWidth) {
  DecodeIFrameThenTestFile("vp8-I-frame-640x240", 640, 240);
}

// Decode |i_frame_buffer_| and then a frame with a smaller width and verify
// the output size was adjusted.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_SmallerWidth) {
  DecodeIFrameThenTestFile("vp8-I-frame-160x240", 160, 240);
}

// Decode |i_frame_buffer_| and then a frame with a larger height and verify
// the output size was adjusted.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_LargerHeight) {
  DecodeIFrameThenTestFile("vp8-I-frame-320x480", 320, 480);
}

// Decode |i_frame_buffer_| and then a frame with a smaller height and verify
// the output size was adjusted.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_SmallerHeight) {
  DecodeIFrameThenTestFile("vp8-I-frame-320x120", 320, 120);
}

// TODO(fgalligan): Enable test when encrypted test data is updated and new
// decryption code is landed. http://crbug.com/132801
TEST_F(FFmpegVideoDecoderTest, DISABLED_DecodeEncryptedFrame_Normal) {
  Initialize();

  // Simulate decoding a single encrypted frame.
  scoped_array<uint8> counter_block(GenerateCounterBlock(kIv));
  encrypted_i_frame_buffer_->SetDecryptConfig(scoped_ptr<DecryptConfig>(
      new DecryptConfig(
          kKeyId, arraysize(kKeyId),
          counter_block.get(), DecryptConfig::kDecryptionKeySize,
          kHmac, arraysize(kHmac),
          sizeof(kIv))));

  EXPECT_CALL(*decryptor_, Decrypt(encrypted_i_frame_buffer_, _))
      .WillRepeatedly(RunDecryptCB(Decryptor::kSuccess, i_frame_buffer_));

  VideoDecoder::DecoderStatus status;
  scoped_refptr<VideoFrame> video_frame;
  DecodeSingleFrame(encrypted_i_frame_buffer_, &status, &video_frame);

  EXPECT_EQ(status, VideoDecoder::kOk);
  ASSERT_TRUE(video_frame);
  EXPECT_FALSE(video_frame->IsEndOfStream());
}

// Test the case that the decryptor fails to decrypt the encrypted buffer.
TEST_F(FFmpegVideoDecoderTest, DecodeEncryptedFrame_DecryptError) {
  Initialize();

  // Simulate decoding a single encrypted frame.
  scoped_array<uint8> counter_block(GenerateCounterBlock(kIv));
  encrypted_i_frame_buffer_->SetDecryptConfig(scoped_ptr<DecryptConfig>(
      new DecryptConfig(
          kKeyId, arraysize(kKeyId),
          counter_block.get(), DecryptConfig::kDecryptionKeySize,
          kHmac, arraysize(kHmac),
          sizeof(kIv))));

  EXPECT_CALL(*demuxer_, Read(_))
      .WillRepeatedly(ReturnBuffer(encrypted_i_frame_buffer_));
  EXPECT_CALL(*decryptor_, Decrypt(encrypted_i_frame_buffer_, _))
      .WillRepeatedly(RunDecryptCB(Decryptor::kError,
                                   scoped_refptr<media::DecoderBuffer>()));

  // Our read should still get satisfied with end of stream frame during an
  // error.
  VideoDecoder::DecoderStatus status;
  scoped_refptr<VideoFrame> video_frame;
  Read(&status, &video_frame);
  EXPECT_EQ(VideoDecoder::kDecryptError, status);
  EXPECT_FALSE(video_frame);

  message_loop_.RunAllPending();
}

// Test the case that the decryptor fails to decrypt the encrypted buffer but
// cannot detect the decryption error and returns a corrupted buffer.
TEST_F(FFmpegVideoDecoderTest, DecodeEncryptedFrame_CorruptedBufferReturned) {
  Initialize();

  // Simulate decoding a single encrypted frame.
  scoped_array<uint8> counter_block(GenerateCounterBlock(kIv));
  encrypted_i_frame_buffer_->SetDecryptConfig(scoped_ptr<DecryptConfig>(
      new DecryptConfig(
          kKeyId, arraysize(kKeyId),
          counter_block.get(), DecryptConfig::kDecryptionKeySize,
          kHmac, arraysize(kHmac),
          sizeof(kIv))));

  EXPECT_CALL(*demuxer_, Read(_))
      .WillRepeatedly(ReturnBuffer(encrypted_i_frame_buffer_));
  EXPECT_CALL(*decryptor_, Decrypt(encrypted_i_frame_buffer_, _))
      .WillRepeatedly(RunDecryptCB(Decryptor::kSuccess,
                                   corrupt_i_frame_buffer_));

  // Our read should still get satisfied with end of stream frame during an
  // error.
  VideoDecoder::DecoderStatus status;
  scoped_refptr<VideoFrame> video_frame;
  Read(&status, &video_frame);
  EXPECT_EQ(VideoDecoder::kDecodeError, status);
  EXPECT_FALSE(video_frame);

  message_loop_.RunAllPending();
}

// Test resetting when decoder has initialized but not decoded.
TEST_F(FFmpegVideoDecoderTest, Reset_Initialized) {
  Initialize();
  Reset();
}

// Test resetting when decoder has decoded single frame.
TEST_F(FFmpegVideoDecoderTest, Reset_Decoding) {
  Initialize();
  EnterDecodingState();
  Reset();
}

// Test resetting when decoder has hit end of stream.
TEST_F(FFmpegVideoDecoderTest, Reset_EndOfStream) {
  Initialize();
  EnterDecodingState();
  EnterEndOfStreamState();
  Reset();
}

// Test resetting when there is a pending read on the demuxer.
TEST_F(FFmpegVideoDecoderTest, Reset_DuringPendingRead) {
  Initialize();

  DemuxerStream::ReadCB read_cb;
  EXPECT_CALL(*demuxer_, Read(_))
      .WillOnce(SaveArg<0>(&read_cb));

  decoder_->Read(read_cb_);
  message_loop_.RunAllPending();

  // Make sure the Read() on the decoder triggers a Read() on
  // the demuxer.
  EXPECT_FALSE(read_cb.is_null());

  // Reset the decoder.
  Reset();

  EXPECT_CALL(*this, FrameReady(VideoDecoder::kOk, IsNull()));

  read_cb.Run(DemuxerStream::kOk, i_frame_buffer_);
  message_loop_.RunAllPending();
}

// Test stopping when decoder has initialized but not decoded.
TEST_F(FFmpegVideoDecoderTest, Stop_Initialized) {
  Initialize();
  Stop();
}

// Test stopping when decoder has decoded single frame.
TEST_F(FFmpegVideoDecoderTest, Stop_Decoding) {
  Initialize();
  EnterDecodingState();
  Stop();
}

// Test stopping when decoder has hit end of stream.
TEST_F(FFmpegVideoDecoderTest, Stop_EndOfStream) {
  Initialize();
  EnterDecodingState();
  EnterEndOfStreamState();
  Stop();
}

// Test aborted read on the demuxer stream.
TEST_F(FFmpegVideoDecoderTest, AbortPendingRead) {
  Initialize();

  EXPECT_CALL(*demuxer_, Read(_))
      .WillOnce(ReturnBuffer(scoped_refptr<DecoderBuffer>()));

  VideoDecoder::DecoderStatus status;
  scoped_refptr<VideoFrame> video_frame;

  Read(&status, &video_frame);

  EXPECT_EQ(status, VideoDecoder::kOk);
  EXPECT_FALSE(video_frame);
}

// Test aborted read on the demuxer stream.
TEST_F(FFmpegVideoDecoderTest, AbortPendingReadDuringFlush) {
  Initialize();

  DemuxerStream::ReadCB read_cb;

  // Request a read on the decoder and run the MessageLoop to
  // ensure that the demuxer has been called.
  decoder_->Read(read_cb_);
  EXPECT_CALL(*demuxer_, Read(_))
      .WillOnce(SaveArg<0>(&read_cb));
  message_loop_.RunAllPending();
  ASSERT_FALSE(read_cb.is_null());

  // Flush while there is still an outstanding read on the demuxer.
  decoder_->Reset(NewExpectedClosure());
  message_loop_.RunAllPending();

  // Signal an aborted demuxer read.
  read_cb.Run(DemuxerStream::kAborted, NULL);

  // Make sure we get a NULL video frame returned.
  EXPECT_CALL(*this, FrameReady(VideoDecoder::kOk, IsNull()));
  message_loop_.RunAllPending();
}

}  // namespace media
