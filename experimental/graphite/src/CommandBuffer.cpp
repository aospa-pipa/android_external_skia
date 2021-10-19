/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "experimental/graphite/src/CommandBuffer.h"

#include "src/core/SkTraceEvent.h"

namespace skgpu {

CommandBuffer::CommandBuffer() {}

void CommandBuffer::releaseResources() {
    TRACE_EVENT0("skia.gpu", TRACE_FUNC);

    fTrackedResources.reset();
}

} // namespace skgpu
