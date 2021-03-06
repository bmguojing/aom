/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"

namespace {

const int kMaxErrorFrames = 12;
const int kMaxDroppableFrames = 12;
const int kMaxErrorResilientFrames = 12;
const int kMaxNoMFMVFrames = 12;
const int kCpuUsed = 1;

class ErrorResilienceTestLarge
    : public ::libaom_test::CodecTestWithParam<libaom_test::TestMode>,
      public ::libaom_test::EncoderTest {
 protected:
  ErrorResilienceTestLarge()
      : EncoderTest(GET_PARAM(0)), psnr_(0.0), nframes_(0), mismatch_psnr_(0.0),
        mismatch_nframes_(0), encoding_mode_(GET_PARAM(1)), allow_mismatch_(0) {
    Reset();
  }

  virtual ~ErrorResilienceTestLarge() {}

  void Reset() {
    error_nframes_ = 0;
    droppable_nframes_ = 0;
    error_resilient_nframes_ = 0;
    nomfmv_nframes_ = 0;
    pattern_switch_ = 0;
  }

  virtual void SetUp() {
    InitializeConfig();
    SetMode(encoding_mode_);
  }

  virtual void BeginPassHook(unsigned int /*pass*/) {
    psnr_ = 0.0;
    nframes_ = 0;
    decoded_nframes_ = 0;
    mismatch_psnr_ = 0.0;
    mismatch_nframes_ = 0;
  }

  virtual void PSNRPktHook(const aom_codec_cx_pkt_t *pkt) {
    psnr_ += pkt->data.psnr.psnr[0];
    nframes_++;
  }

  virtual void PreEncodeFrameHook(libaom_test::VideoSource *video,
                                  libaom_test::Encoder *encoder) {
    if (video->frame() == 0) encoder->Control(AOME_SET_CPUUSED, kCpuUsed);
    if (droppable_nframes_ > 0 &&
        (cfg_.g_pass == AOM_RC_LAST_PASS || cfg_.g_pass == AOM_RC_ONE_PASS)) {
      for (unsigned int i = 0; i < droppable_nframes_; ++i) {
        if (droppable_frames_[i] == video->frame()) {
          std::cout << "             Encoding droppable frame: "
                    << droppable_frames_[i] << "\n";
          frame_flags_ |= (AOM_EFLAG_NO_UPD_LAST | AOM_EFLAG_NO_UPD_GF |
                           AOM_EFLAG_NO_UPD_ARF);
          break;
        }
      }
    }
    encoder->Control(AV1E_SET_ERROR_RESILIENT_MODE, 0);
    if (error_resilient_nframes_ > 0 &&
        (cfg_.g_pass == AOM_RC_LAST_PASS || cfg_.g_pass == AOM_RC_ONE_PASS)) {
      for (unsigned int i = 0; i < error_resilient_nframes_; ++i) {
        if (error_resilient_frames_[i] == video->frame()) {
          std::cout << "             Encoding error_resilient frame: "
                    << error_resilient_frames_[i] << "\n";
          encoder->Control(AV1E_SET_ERROR_RESILIENT_MODE, 1);
          break;
        }
      }
    }
    encoder->Control(AV1E_SET_ALLOW_REF_FRAME_MVS, 1);
    if (nomfmv_nframes_ > 0 &&
        (cfg_.g_pass == AOM_RC_LAST_PASS || cfg_.g_pass == AOM_RC_ONE_PASS)) {
      for (unsigned int i = 0; i < nomfmv_nframes_; ++i) {
        if (nomfmv_frames_[i] == video->frame()) {
          std::cout << "             Encoding no mfmv frame: "
                    << nomfmv_frames_[i] << "\n";
          encoder->Control(AV1E_SET_ALLOW_REF_FRAME_MVS, 0);
          break;
        }
      }
    }
  }

  double GetAveragePsnr() const {
    if (nframes_) return psnr_ / nframes_;
    return 0.0;
  }

  double GetAverageMismatchPsnr() const {
    if (mismatch_nframes_) return mismatch_psnr_ / mismatch_nframes_;
    return 0.0;
  }

  virtual bool DoDecode() const {
    if (error_nframes_ > 0 &&
        (cfg_.g_pass == AOM_RC_LAST_PASS || cfg_.g_pass == AOM_RC_ONE_PASS)) {
      for (unsigned int i = 0; i < error_nframes_; ++i) {
        if (error_frames_[i] == nframes_ - 1) {
          std::cout << "             Skipping decoding frame: "
                    << error_frames_[i] << "\n";
          return 0;
        }
      }
    }
    return 1;
  }

  virtual void MismatchHook(const aom_image_t *img1, const aom_image_t *img2) {
    if (allow_mismatch_) {
      double mismatch_psnr = compute_psnr(img1, img2);
      mismatch_psnr_ += mismatch_psnr;
      ++mismatch_nframes_;
      // std::cout << "Mismatch frame psnr: " << mismatch_psnr << "\n";
    } else {
      ::libaom_test::EncoderTest::MismatchHook(img1, img2);
    }
  }

  virtual void DecompressedFrameHook(const aom_image_t &img,
                                     aom_codec_pts_t pts) {
    (void)img;
    (void)pts;
    ++decoded_nframes_;
  }

  void SetErrorFrames(int num, unsigned int *list) {
    if (num > kMaxErrorFrames)
      num = kMaxErrorFrames;
    else if (num < 0)
      num = 0;
    error_nframes_ = num;
    for (unsigned int i = 0; i < error_nframes_; ++i)
      error_frames_[i] = list[i];
  }

  void SetDroppableFrames(int num, unsigned int *list) {
    if (num > kMaxDroppableFrames)
      num = kMaxDroppableFrames;
    else if (num < 0)
      num = 0;
    droppable_nframes_ = num;
    for (unsigned int i = 0; i < droppable_nframes_; ++i)
      droppable_frames_[i] = list[i];
  }

  void SetErrorResilientFrames(int num, unsigned int *list) {
    if (num > kMaxErrorResilientFrames)
      num = kMaxErrorResilientFrames;
    else if (num < 0)
      num = 0;
    error_resilient_nframes_ = num;
    for (unsigned int i = 0; i < error_resilient_nframes_; ++i)
      error_resilient_frames_[i] = list[i];
  }

  void SetNoMFMVFrames(int num, unsigned int *list) {
    if (num > kMaxNoMFMVFrames)
      num = kMaxNoMFMVFrames;
    else if (num < 0)
      num = 0;
    nomfmv_nframes_ = num;
    for (unsigned int i = 0; i < nomfmv_nframes_; ++i)
      nomfmv_frames_[i] = list[i];
  }

  unsigned int GetMismatchFrames() { return mismatch_nframes_; }
  unsigned int GetEncodedFrames() { return nframes_; }
  unsigned int GetDecodedFrames() { return decoded_nframes_; }

  void SetAllowMismatch(int allow) { allow_mismatch_ = allow; }
  void SetPatternSwitch(int frame_switch) { pattern_switch_ = frame_switch; }

 private:
  double psnr_;
  unsigned int nframes_;
  unsigned int decoded_nframes_;
  unsigned int error_nframes_;
  unsigned int droppable_nframes_;
  unsigned int error_resilient_nframes_;
  unsigned int nomfmv_nframes_;
  unsigned int pattern_switch_;
  double mismatch_psnr_;
  unsigned int mismatch_nframes_;
  unsigned int error_frames_[kMaxErrorFrames];
  unsigned int droppable_frames_[kMaxDroppableFrames];
  unsigned int error_resilient_frames_[kMaxErrorResilientFrames];
  unsigned int nomfmv_frames_[kMaxNoMFMVFrames];
  libaom_test::TestMode encoding_mode_;
  int allow_mismatch_;
};

TEST_P(ErrorResilienceTestLarge, OnVersusOff) {
  const aom_rational timebase = { 33333333, 1000000000 };
  cfg_.g_timebase = timebase;
  cfg_.rc_target_bitrate = 2000;
  cfg_.g_lag_in_frames = 10;

  init_flags_ = AOM_CODEC_USE_PSNR;

  libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                     timebase.den, timebase.num, 0, 12);

  // Global error resilient mode OFF.
  cfg_.g_error_resilient = 0;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  const double psnr_resilience_off = GetAveragePsnr();
  EXPECT_GT(psnr_resilience_off, 25.0);

  Reset();
  // Error resilient mode ON for certain frames
  unsigned int num_error_resilient_frames = 5;
  unsigned int error_resilient_frame_list[] = { 3, 5, 6, 9, 11 };
  SetErrorResilientFrames(num_error_resilient_frames,
                          error_resilient_frame_list);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  const double psnr_resilience_on = GetAveragePsnr();
  EXPECT_GT(psnr_resilience_on, 25.0);

  // Test that turning on error resilient mode hurts by 10% at most.
  if (psnr_resilience_off > 0.0) {
    const double psnr_ratio = psnr_resilience_on / psnr_resilience_off;
    EXPECT_GE(psnr_ratio, 0.9);
    EXPECT_LE(psnr_ratio, 1.1);
  }
}

// Check for successful decoding and no encoder/decoder mismatch
// if we lose (i.e., drop before decoding) a set of droppable
// frames (i.e., frames that don't update any reference buffers).
TEST_P(ErrorResilienceTestLarge, DropFramesWithoutRecovery) {
  const aom_rational timebase = { 33333333, 1000000000 };
  cfg_.g_timebase = timebase;
  cfg_.rc_target_bitrate = 500;
  cfg_.g_lag_in_frames = 10;

  init_flags_ = AOM_CODEC_USE_PSNR;

  libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                     timebase.den, timebase.num, 0, 20);

  cfg_.kf_mode = AOM_KF_DISABLED;

  // Set an arbitrary set of error frames same as droppable frames.
  unsigned int num_droppable_frames = 3;
  unsigned int droppable_frame_list[] = { 5, 10, 13 };
  SetDroppableFrames(num_droppable_frames, droppable_frame_list);
  SetErrorFrames(num_droppable_frames, droppable_frame_list);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  // Test that no mismatches have been found
  std::cout << "             Encoded frames: " << GetEncodedFrames() << "\n";
  std::cout << "             Decoded frames: " << GetDecodedFrames() << "\n";
  std::cout << "             Mismatch frames: " << GetMismatchFrames() << "\n";
  EXPECT_EQ(GetEncodedFrames() - GetDecodedFrames(), num_droppable_frames);
}

// Check for ParseAbility property of an error-resilient frame.
// Encode a frame in error-resilient mode (E-frame), and disallow all
// subsequent frames from using MFMV. If frames are dropped before the
// E frame, all frames starting from the E frame should be parse-able.
TEST_P(ErrorResilienceTestLarge, ParseAbilityTest) {
  const aom_rational timebase = { 33333333, 1000000000 };
  cfg_.g_timebase = timebase;
  cfg_.rc_target_bitrate = 500;
  // TODO(sarahparker, debargha): Make control setting work correctly for
  // lag_in_frames > 0
  cfg_.g_lag_in_frames = 0;

  init_flags_ = AOM_CODEC_USE_PSNR;

  libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                     timebase.den, timebase.num, 0, 15);

  cfg_.kf_mode = AOM_KF_DISABLED;

  SetAllowMismatch(1);

  // Set an arbitrary error resilient (E) frame
  unsigned int num_error_resilient_frames = 1;
  unsigned int error_resilient_frame_list[] = { 6 };
  SetErrorResilientFrames(num_error_resilient_frames,
                          error_resilient_frame_list);
  // Set all frames after the error resilient frame to not allow MFMV
  unsigned int num_nomfmv_frames = 8;
  unsigned int nomfmv_frame_list[] = { 7, 8, 9, 10, 11, 12, 13, 14 };
  SetNoMFMVFrames(num_nomfmv_frames, nomfmv_frame_list);

  // Set a few frames before the E frame that are lost (not decoded)
  unsigned int num_error_frames = 3;
  unsigned int error_frame_list[] = { 3, 4, 5 };
  SetErrorFrames(num_error_frames, error_frame_list);

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  std::cout << "             Encoded frames: " << GetEncodedFrames() << "\n";
  std::cout << "             Decoded frames: " << GetDecodedFrames() << "\n";
  std::cout << "             Mismatch frames: " << GetMismatchFrames() << "\n";
  EXPECT_EQ(GetEncodedFrames() - GetDecodedFrames(), num_error_frames);
  // All frames following the E-frame and the E-frame are expected to have
  // mismatches, but still be parse-able.
  EXPECT_EQ(GetMismatchFrames(), num_nomfmv_frames + 1);
}

AV1_INSTANTIATE_TEST_CASE(ErrorResilienceTestLarge, NONREALTIME_TEST_MODES);
}  // namespace
