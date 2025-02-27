// Copyright 2020 The MediaPipe Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <vector>

#include "absl/memory/memory.h"
#include "absl/types/span.h"
#include "mediapipe/examples/desktop/autoflip/calculators/pad_lapped_tensor_buffer_calculator.pb.h"
#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/port/ret_check.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/framework/profiler/circular_buffer.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/framework/tensor_util.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/lib/core/status.h"

namespace mediapipe {

const char kBufferSize[] = "BUFFER_SIZE";
const char kOverlap[] = "OVERLAP";
const char kTimestampOffset[] = "TIMESTAMP_OFFSET";
const char kCalculatorOptions[] = "CALCULATOR_OPTIONS";
const int kNumOfPadding = 25;
const int kPaddingAdjust = 50;

namespace tf = tensorflow;

// This calculator is based on lapped_tensor_buffer_calculator, adds padding
// function and fixes buffer_size and overlap for TransNetV2
// https://github.com/soCzech/TransNetV2.
//
// Given an input stream of tensors, concatenates the tensors over timesteps.
// The concatenated output tensors can be specified to have overlap between
// output timesteps. The tensors are concatenated along the first dimension, and
// a flag controls whether a new first dimension is inserted before
// concatenation.
//
// The timestamp of the output batch will match the timestamp of the first
// tensor in that batch by default. (e.g. when buffer_size frames are added, the
// output tensor will have the timestamp of the first input.). This behavior can
// be adjusted by the timestamp_offset option.
//
// Original lapped_tensor_buffer_calculator does not have padding function. This
// calculator has the padding setting. It will pad before the video with the first
// frame and pad after the video with the last frame. 
//
// Example config:
// node {
//   calculator: "PadLappedTensorBufferCalculator"
//   input_stream: "input_tensor"
//   output_stream: "output_tensor"
//   output_stream: "output_timestamp"
//   options {
//     [mediapipe.LappedTensorBufferCalculatorOptions.ext] {
//       buffer_size: 100
//       overlap: 50
//       add_batch_dim_to_tensors: true
//       timestamp_offset: 25
//     }
//   }
// }

class PadLappedTensorBufferCalculator : public CalculatorBase {
 public:
  static ::mediapipe::Status GetContract(CalculatorContract* cc);

  ::mediapipe::Status Open(CalculatorContext* cc) override;
  ::mediapipe::Status Process(CalculatorContext* cc) override;
  ::mediapipe::Status Close(CalculatorContext* cc) override;

 private:
  // Adds a batch dimension to the input tensor if specified in the 
  // calculator options.
  ::mediapipe::Status AddBatchDimension(tf::Tensor* input_tensor);
  ::mediapipe::Status ProcessBuffer(CalculatorContext* cc);

  int steps_until_output_;
  int buffer_size_;
  int overlap_;
  int timestamp_offset_;
  int num_of_frames_;

  std::unique_ptr<CircularBuffer<Timestamp>> timestamp_buffer_;
  std::unique_ptr<CircularBuffer<tf::Tensor>> buffer_;
  PadLappedTensorBufferCalculatorOptions options_;
};

REGISTER_CALCULATOR(PadLappedTensorBufferCalculator);

::mediapipe::Status PadLappedTensorBufferCalculator::GetContract(
    CalculatorContract* cc) {
  RET_CHECK_EQ(cc->Inputs().NumEntries(), 1)
      << "Only one input stream is supported.";
  cc->Inputs().Index(0).Set<tf::Tensor>(
      // tensorflow::Tensor stream.
  );
  RET_CHECK_EQ(cc->Outputs().NumEntries(), 2)
      << "Only two outputs stream is supported.";

  if (cc->InputSidePackets().HasTag(kBufferSize)) {
    cc->InputSidePackets().Tag(kBufferSize).Set<int>();
  }
  if (cc->InputSidePackets().HasTag(kOverlap)) {
    cc->InputSidePackets().Tag(kOverlap).Set<int>();
  }
  if (cc->InputSidePackets().HasTag(kTimestampOffset)) {
    cc->InputSidePackets().Tag(kTimestampOffset).Set<int>();
  }
  if (cc->InputSidePackets().HasTag(kCalculatorOptions)) {
    cc->InputSidePackets()
        .Tag(kCalculatorOptions)
        .Set<PadLappedTensorBufferCalculatorOptions>();
  }
  cc->Outputs().Index(0).Set<tf::Tensor>(
      // Output tensorflow::Tensor stream with possibly overlapping steps.
  );
  cc->Outputs().Index(1).Set<std::vector<Timestamp>>(
      // Output timestamp stream with possibly overlapping steps.
  );
  return ::mediapipe::OkStatus();
}

::mediapipe::Status PadLappedTensorBufferCalculator::Open(CalculatorContext* cc) {
  options_ = cc->Options<PadLappedTensorBufferCalculatorOptions>();
  if (cc->InputSidePackets().HasTag(kCalculatorOptions)) {
    options_ = cc->InputSidePackets()
                   .Tag(kCalculatorOptions)
                   .Get<PadLappedTensorBufferCalculatorOptions>();
  }
  buffer_size_ = options_.buffer_size();
  if (cc->InputSidePackets().HasTag(kBufferSize)) {
    buffer_size_ = cc->InputSidePackets().Tag(kBufferSize).Get<int>();
  }
  overlap_ = options_.overlap();
  if (cc->InputSidePackets().HasTag(kOverlap)) {
    overlap_ = cc->InputSidePackets().Tag(kOverlap).Get<int>();
  }
  timestamp_offset_ = options_.timestamp_offset();
  if (cc->InputSidePackets().HasTag(kTimestampOffset)) {
    timestamp_offset_ = cc->InputSidePackets().Tag(kTimestampOffset).Get<int>();
  }

  RET_CHECK_LT(overlap_, buffer_size_);
  RET_CHECK_GE(timestamp_offset_, 0)
      << "Negative timestamp_offset is not allowed.";
  RET_CHECK_LT(timestamp_offset_, buffer_size_)
      << "output_frame_num_offset has to be less than buffer_size.";
  timestamp_buffer_ =
      absl::make_unique<CircularBuffer<Timestamp>>(buffer_size_);
  buffer_ = absl::make_unique<CircularBuffer<tf::Tensor>>(buffer_size_);
  steps_until_output_ = buffer_size_ - kNumOfPadding;
  num_of_frames_ = 0;

  return ::mediapipe::OkStatus();
}

::mediapipe::Status PadLappedTensorBufferCalculator::Process(
    CalculatorContext* cc) {
  // These are cheap, shallow copies.
  tensorflow::Tensor input_tensor(
      cc->Inputs().Index(0).Get<tensorflow::Tensor>());
  if (options_.add_batch_dim_to_tensors()) {
    RET_CHECK_OK(AddBatchDimension(&input_tensor));
  }
  // Pad frames at the beginning with the first frame.
  if (num_of_frames_ == 0) {
    for (int i = 0; i < kNumOfPadding; ++i) {
      buffer_->push_back(input_tensor);
      timestamp_buffer_->push_back(cc->InputTimestamp());
    }
  }
  
  buffer_->push_back(input_tensor);
  timestamp_buffer_->push_back(cc->InputTimestamp());
  --steps_until_output_;

  if (steps_until_output_ <= 0) {
    MP_RETURN_IF_ERROR(ProcessBuffer(cc));
  }

  num_of_frames_ ++;

  return ::mediapipe::OkStatus();
}

::mediapipe::Status PadLappedTensorBufferCalculator::Close(
    CalculatorContext* cc) {
    if (num_of_frames_ == 0)
      return ::mediapipe::OkStatus();
    
    int last_frame = buffer_size_ - steps_until_output_ - 1;
    const auto& pad_frame = buffer_->Get(last_frame);
    for (int i = 0; i < steps_until_output_; ++i) {
      buffer_->push_back(pad_frame);
      timestamp_buffer_->push_back(cc->InputTimestamp());
    }
    MP_RETURN_IF_ERROR(ProcessBuffer(cc));

    if (overlap_ < num_of_frames_ 
      && num_of_frames_ < buffer_size_ - kNumOfPadding) {
      for (int i = 0; i < overlap_; ++i) {
        buffer_->push_back(pad_frame);
        timestamp_buffer_->push_back(cc->InputTimestamp());
      }
      MP_RETURN_IF_ERROR(ProcessBuffer(cc));
    }

    return ::mediapipe::OkStatus();
}

// Adds a batch dimension to the input tensor if specified in the calculator
// options.
::mediapipe::Status PadLappedTensorBufferCalculator::AddBatchDimension(
    tf::Tensor* input_tensor) {
  if (options_.add_batch_dim_to_tensors()) {
    tf::TensorShape new_shape(input_tensor->shape());
    new_shape.InsertDim(0, 1);
    RET_CHECK(input_tensor->CopyFrom(*input_tensor, new_shape))
        << "Could not add 0th dimension to tensor without changing its shape."
        << " Current shape: " << input_tensor->shape().DebugString();
  }
  return ::mediapipe::OkStatus();
}

// Process buffer
::mediapipe::Status PadLappedTensorBufferCalculator::ProcessBuffer(
  CalculatorContext* cc) {
    auto concatenated = ::absl::make_unique<tf::Tensor>();
    auto output_timestamp = ::absl::make_unique<std::vector<Timestamp>>();

    const tf::Status concat_status = tf::tensor::Concat(
        std::vector<tf::Tensor>(buffer_->begin(), buffer_->end()),
        concatenated.get());
    RET_CHECK(concat_status.ok()) << concat_status.ToString();

    // Output cancatenated tensor.
    cc->Outputs().Index(0).Add(concatenated.release(),
                               timestamp_buffer_->Get(timestamp_offset_));

    // Output timestamp vector.
    *output_timestamp = std::vector<Timestamp>(timestamp_buffer_->begin(), timestamp_buffer_->end());
    RET_CHECK_EQ(output_timestamp->size(), buffer_size_)
      << "Output timestamp size is not correct.";
    cc->Outputs().Index(1).Add(output_timestamp.release(),
                               timestamp_buffer_->Get(timestamp_offset_));
    steps_until_output_ = buffer_size_ - overlap_;
  return ::mediapipe::OkStatus();
}

}  // namespace mediapipe
