/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_GSMFR_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_GSMFR_H_

#include "modules/audio_coding/main/source/acm_generic_codec.h"

// forward declaration
struct GSMFR_encinst_t_;
struct GSMFR_decinst_t_;

namespace webrtc {

namespace acm1 {

class ACMGSMFR : public ACMGenericCodec {
 public:
  explicit ACMGSMFR(int16_t codec_id);
  ~ACMGSMFR();

  // for FEC
  ACMGenericCodec* CreateInstance(void);

  int16_t InternalEncode(uint8_t* bitstream,
                         int16_t* bitstream_len_byte);

  int16_t InternalInitEncoder(WebRtcACMCodecParams *codec_params);

  int16_t InternalInitDecoder(WebRtcACMCodecParams *codec_params);

 protected:
  int16_t DecodeSafe(uint8_t* bitstream,
                     int16_t bitstream_len_byte,
                     int16_t* audio,
                     int16_t* audio_samples,
                     int8_t* speech_type);

  int32_t CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                   const CodecInst& codec_inst);

  void DestructEncoderSafe();

  void DestructDecoderSafe();

  int16_t InternalCreateEncoder();

  int16_t InternalCreateDecoder();

  void InternalDestructEncoderInst(void* ptr_inst);

  int16_t EnableDTX();

  int16_t DisableDTX();

  GSMFR_encinst_t_* encoder_inst_ptr_;
  GSMFR_decinst_t_* decoder_inst_ptr_;
};

}  // namespace acm1

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_GSMFR_H_
