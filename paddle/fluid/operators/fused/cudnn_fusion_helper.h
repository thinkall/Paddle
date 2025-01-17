/* Copyright (c) 2021 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#pragma once

#include <string>
#include <vector>
#include "paddle/fluid/platform/cudnn_desc.h"
#include "paddle/fluid/platform/cudnn_helper.h"
#include "paddle/fluid/platform/dynload/cudnn.h"
#include "paddle/fluid/platform/enforce.h"

namespace paddle {
namespace operators {

namespace dynload = platform::dynload;

#if CUDNN_VERSION >= 8000

// A wrapper for cuDNN fused_op API.
class CudnnFusionOp {
 public:
  explicit CudnnFusionOp(cudnnFusedOps_t op_id) : plan_created_(false) {
    // New 'fused op' descriptor creation
    PADDLE_ENFORCE_CUDA_SUCCESS(dynload::cudnnCreateFusedOpsPlan(&op_, op_id));
    PADDLE_ENFORCE_CUDA_SUCCESS(
        dynload::cudnnCreateFusedOpsConstParamPack(&op_const_params_, op_id));
    PADDLE_ENFORCE_CUDA_SUCCESS(dynload::cudnnCreateFusedOpsVariantParamPack(
        &op_variant_params_, op_id));
  }

  ~CudnnFusionOp() {
    // New 'fused op' descriptor destruction
    PADDLE_ENFORCE_CUDA_SUCCESS(
        dynload::cudnnDestroyFusedOpsVariantParamPack(op_variant_params_));
    PADDLE_ENFORCE_CUDA_SUCCESS(
        dynload::cudnnDestroyFusedOpsConstParamPack(op_const_params_));
    PADDLE_ENFORCE_CUDA_SUCCESS(dynload::cudnnDestroyFusedOpsPlan(op_));
  }

  // Execute fused op
  void Execute(cudnnHandle_t cudnn_handle) {
    PADDLE_ENFORCE_EQ(
        plan_created_, true,
        platform::errors::Fatal(
            "CudnnFusionOp exec requested without a valid 'plan', need: "
            "<set const params>, GetWorkspaceSizeBytes(), Execute()."));
    PADDLE_ENFORCE_CUDA_SUCCESS(
        dynload::cudnnFusedOpsExecute(cudnn_handle, op_, op_variant_params_));
  }

  // Set const param pack attribute given a descriptor.
  template <typename T>
  void SetOpConstParamDesc(cudnnFusedOpsConstParamLabel_t param_label,
                           T *param_ptr) {
    PADDLE_ENFORCE_CUDA_SUCCESS(
        dynload::cudnnSetFusedOpsConstParamPackAttribute(
            op_const_params_, param_label, param_ptr));
    plan_created_ = false;
  }

  // Set multiple const param pack attribute given a descriptor.
  template <typename T>
  void SetOpConstParamDesc(
      const std::vector<cudnnFusedOpsConstParamLabel_t> &param_labels,
      T *param_ptr) {
    for (auto param_label : param_labels) {
      SetOpConstParamDesc(param_label, param_ptr);
    }
  }

  // Set const param pack attribute given a value of param.
  template <typename T>
  void SetOpConstParamAttr(cudnnFusedOpsConstParamLabel_t param_label,
                           T param) {
    PADDLE_ENFORCE_CUDA_SUCCESS(
        dynload::cudnnSetFusedOpsConstParamPackAttribute(op_const_params_,
                                                         param_label, &param));
    plan_created_ = false;
  }

  // Set multiple const param pack attribute given a value of param.
  template <typename T>
  void SetOpConstParamAttr(
      const std::vector<cudnnFusedOpsConstParamLabel_t> &param_labels,
      T param) {
    for (auto param_label : param_labels) {
      SetOpConstParamAttr(param_label, param);
    }
  }

  // Set a variant param pack attribute given a reference to a param.
  template <typename T>
  void SetOpVariantParamAttrPtr(cudnnFusedOpsVariantParamLabel_t param_label,
                                T *param_ptr) {
    PADDLE_ENFORCE_CUDA_SUCCESS(
        dynload::cudnnSetFusedOpsVariantParamPackAttribute(
            op_variant_params_, param_label, param_ptr));
  }

  // Set multiple const param pack attributes given a reference to a param.
  template <typename T>
  void SetOpVariantParamAttrPtr(
      const std::vector<cudnnFusedOpsVariantParamLabel_t> &param_labels,
      const T *param_ptr) {
    for (auto param_label : param_labels) {
      SetOpVariantParamAttrPtr(param_label, param_ptr);
    }
  }

  // Get the workspace, which is required before Execute().
  size_t GetWorkspaceSizeInBytes(cudnnHandle_t cudnn_handle) {
    size_t workspace_bytes = 0U;
    PADDLE_ENFORCE_CUDA_SUCCESS(dynload::cudnnMakeFusedOpsPlan(
        cudnn_handle, op_, op_const_params_, &workspace_bytes));
    plan_created_ = true;
    return workspace_bytes;
  }

 private:
  bool plan_created_;

  cudnnFusedOpsPlan_t op_;
  cudnnFusedOpsConstParamPack_t op_const_params_;
  cudnnFusedOpsVariantParamPack_t op_variant_params_;
};

static inline std::vector<int> GetStrides(const std::vector<int> &shape) {
  if (shape.size() < 1) {
    return {};
  }
  int dim = static_cast<int>(shape.size());
  std::vector<int> pro_shape(shape);
  std::vector<int> strides(dim);
  int temp = pro_shape[1];
  pro_shape.erase(pro_shape.begin() + 1);
  pro_shape.push_back(temp);
  strides.back() = 1;
  for (int i = dim - 2; i >= 0; --i) {
    strides[i] = strides[i + 1] * pro_shape[i + 1];
  }
  strides.pop_back();
  strides.insert(strides.begin() + 1, 1);
  return strides;
}

static inline int64_t AlignUp(int64_t a, int64_t b) { return (a + b - 1) / b; }

#endif  // CUDNN_VERSION >= 8000
}  // namespace operators
}  // namespace paddle
