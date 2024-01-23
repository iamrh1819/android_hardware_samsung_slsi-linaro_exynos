/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <android/hardware/graphics/allocator/2.0/IAllocator.h>
#include <android/hardware/graphics/allocator/4.0/IAllocator.h>
#include <android/hardware/graphics/mapper/2.0/IMapper.h>
#include <android/hardware/graphics/mapper/4.0/IMapper.h>

using namespace android::hardware::graphics::common;
using namespace android::hardware::graphics::mapper;

inline void translate(const V4_0::IMapper::BufferDescriptorInfo& in, V2_0::IMapper::BufferDescriptorInfo& out) {
    out.width = in.width;
    out.height = in.height;
    out.layerCount = in.layerCount;
    out.format = static_cast<V1_0::PixelFormat>(in.format);
    out.usage = in.usage;
}

inline void translate(const V2_0::IMapper::BufferDescriptorInfo& in, V4_0::IMapper::BufferDescriptorInfo& out) {
    out.width = in.width;
    out.height = in.height;
    out.layerCount = in.layerCount;
    out.format = static_cast<V1_2::PixelFormat>(in.format);
    out.usage = in.usage;
}

inline void translate(const V2_0::BufferDescriptor& in, V4_0::BufferDescriptor& out) {
    out.resize(in.size());
    for (size_t i = 0; i < in.size(); i++) {
        out[i] = static_cast<uint8_t>(in[i]);
    }
}

inline void translate(const V4_0::BufferDescriptor& in, V2_0::BufferDescriptor& out) {
    out.resize(in.size());
    for (size_t i = 0; i < in.size(); i++) {
        out[i] = static_cast<uint32_t>(in[i]);
    }
}

inline void translate(const V2_0::IMapper::Rect& in, V4_0::IMapper::Rect& out) {
    out.left = in.left;
    out.top = in.top;
    out.width = in.width;
    out.height = in.height;
}
