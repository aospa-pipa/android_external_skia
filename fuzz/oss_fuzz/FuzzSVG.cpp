/*
 * Copyright 2020 Google, LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "experimental/svg/model/SkSVGDOM.h"
#include "include/core/SkData.h"
#include "include/core/SkStream.h"
#include "include/core/SkSurface.h"

void FuzzSVG(sk_sp<SkData> bytes) {
    uint8_t w = 100;
    uint8_t h = 200;

    SkMemoryStream stream(bytes);
    sk_sp<SkSVGDOM> dom = SkSVGDOM::MakeFromStream(stream);
    if (!dom) {
        return;
    }

    auto s = SkSurface::MakeRasterN32Premul(128, 128);
    if (!s) {
        return;
    }
    SkSize winSize = SkSize::Make(w, h);
    dom->setContainerSize(winSize);
    dom->containerSize();
    dom->render(s->getCanvas());

}

#if defined(IS_FUZZING_WITH_LIBFUZZER)
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    auto bytes = SkData::MakeWithoutCopy(data, size);
    FuzzSVG(bytes);
    return 0;
}
#endif
