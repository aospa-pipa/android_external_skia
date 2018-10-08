/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkPDFUtils.h"

#include "SkData.h"
#include "SkFixed.h"
#include "SkGeometry.h"
#include "SkImage_Base.h"
#include "SkPDFResourceDict.h"
#include "SkPDFTypes.h"
#include "SkStream.h"
#include "SkString.h"

#include <cmath>

const char* SkPDFUtils::BlendModeName(SkBlendMode mode) {
    // PDF32000.book section 11.3.5 "Blend Mode"
    switch (mode) {
        case SkBlendMode::kSrcOver:     return "Normal";
        case SkBlendMode::kXor:         return "Normal";  // (unsupported mode)
        case SkBlendMode::kPlus:        return "Normal";  // (unsupported mode)
        case SkBlendMode::kScreen:      return "Screen";
        case SkBlendMode::kOverlay:     return "Overlay";
        case SkBlendMode::kDarken:      return "Darken";
        case SkBlendMode::kLighten:     return "Lighten";
        case SkBlendMode::kColorDodge:  return "ColorDodge";
        case SkBlendMode::kColorBurn:   return "ColorBurn";
        case SkBlendMode::kHardLight:   return "HardLight";
        case SkBlendMode::kSoftLight:   return "SoftLight";
        case SkBlendMode::kDifference:  return "Difference";
        case SkBlendMode::kExclusion:   return "Exclusion";
        case SkBlendMode::kMultiply:    return "Multiply";
        case SkBlendMode::kHue:         return "Hue";
        case SkBlendMode::kSaturation:  return "Saturation";
        case SkBlendMode::kColor:       return "Color";
        case SkBlendMode::kLuminosity:  return "Luminosity";
        // Other blendmodes are handled in SkPDFDevice::setUpContentEntry.
        default:                        return nullptr;
    }
}

sk_sp<SkPDFArray> SkPDFUtils::RectToArray(const SkRect& r) {
    return SkPDFMakeArray(r.left(), r.top(), r.right(), r.bottom());
}

sk_sp<SkPDFArray> SkPDFUtils::MatrixToArray(const SkMatrix& matrix) {
    SkScalar a[6];
    if (!matrix.asAffine(a)) {
        SkMatrix::SetAffineIdentity(a);
    }
    return SkPDFMakeArray(a[0], a[1], a[2], a[3], a[4], a[5]);
}

void SkPDFUtils::MoveTo(SkScalar x, SkScalar y, SkWStream* content) {
    SkPDFUtils::AppendScalar(x, content);
    content->writeText(" ");
    SkPDFUtils::AppendScalar(y, content);
    content->writeText(" m\n");
}

void SkPDFUtils::AppendLine(SkScalar x, SkScalar y, SkWStream* content) {
    SkPDFUtils::AppendScalar(x, content);
    content->writeText(" ");
    SkPDFUtils::AppendScalar(y, content);
    content->writeText(" l\n");
}

static void append_cubic(SkScalar ctl1X, SkScalar ctl1Y,
                         SkScalar ctl2X, SkScalar ctl2Y,
                         SkScalar dstX, SkScalar dstY, SkWStream* content) {
    SkString cmd("y\n");
    SkPDFUtils::AppendScalar(ctl1X, content);
    content->writeText(" ");
    SkPDFUtils::AppendScalar(ctl1Y, content);
    content->writeText(" ");
    if (ctl2X != dstX || ctl2Y != dstY) {
        cmd.set("c\n");
        SkPDFUtils::AppendScalar(ctl2X, content);
        content->writeText(" ");
        SkPDFUtils::AppendScalar(ctl2Y, content);
        content->writeText(" ");
    }
    SkPDFUtils::AppendScalar(dstX, content);
    content->writeText(" ");
    SkPDFUtils::AppendScalar(dstY, content);
    content->writeText(" ");
    content->writeText(cmd.c_str());
}

static void append_quad(const SkPoint quad[], SkWStream* content) {
    SkPoint cubic[4];
    SkConvertQuadToCubic(quad, cubic);
    append_cubic(cubic[1].fX, cubic[1].fY, cubic[2].fX, cubic[2].fY,
                 cubic[3].fX, cubic[3].fY, content);
}

void SkPDFUtils::AppendRectangle(const SkRect& rect, SkWStream* content) {
    // Skia has 0,0 at top left, pdf at bottom left.  Do the right thing.
    SkScalar bottom = SkMinScalar(rect.fBottom, rect.fTop);

    SkPDFUtils::AppendScalar(rect.fLeft, content);
    content->writeText(" ");
    SkPDFUtils::AppendScalar(bottom, content);
    content->writeText(" ");
    SkPDFUtils::AppendScalar(rect.width(), content);
    content->writeText(" ");
    SkPDFUtils::AppendScalar(rect.height(), content);
    content->writeText(" re\n");
}

void SkPDFUtils::EmitPath(const SkPath& path, SkPaint::Style paintStyle,
                          bool doConsumeDegerates, SkWStream* content,
                          SkScalar tolerance) {
    if (path.isEmpty() && SkPaint::kFill_Style == paintStyle) {
        SkPDFUtils::AppendRectangle({0, 0, 0, 0}, content);
        return;
    }
    // Filling a path with no area results in a drawing in PDF renderers but
    // Chrome expects to be able to draw some such entities with no visible
    // result, so we detect those cases and discard the drawing for them.
    // Specifically: moveTo(X), lineTo(Y) and moveTo(X), lineTo(X), lineTo(Y).

    SkRect rect;
    bool isClosed; // Both closure and direction need to be checked.
    SkPath::Direction direction;
    if (path.isRect(&rect, &isClosed, &direction) &&
        isClosed &&
        (SkPath::kCW_Direction == direction ||
         SkPath::kEvenOdd_FillType == path.getFillType()))
    {
        SkPDFUtils::AppendRectangle(rect, content);
        return;
    }

    enum SkipFillState {
        kEmpty_SkipFillState,
        kSingleLine_SkipFillState,
        kNonSingleLine_SkipFillState,
    };
    SkipFillState fillState = kEmpty_SkipFillState;
    //if (paintStyle != SkPaint::kFill_Style) {
    //    fillState = kNonSingleLine_SkipFillState;
    //}
    SkPoint lastMovePt = SkPoint::Make(0,0);
    SkDynamicMemoryWStream currentSegment;
    SkPoint args[4];
    SkPath::Iter iter(path, false);
    for (SkPath::Verb verb = iter.next(args, doConsumeDegerates);
         verb != SkPath::kDone_Verb;
         verb = iter.next(args, doConsumeDegerates)) {
        // args gets all the points, even the implicit first point.
        switch (verb) {
            case SkPath::kMove_Verb:
                MoveTo(args[0].fX, args[0].fY, &currentSegment);
                lastMovePt = args[0];
                fillState = kEmpty_SkipFillState;
                break;
            case SkPath::kLine_Verb:
                AppendLine(args[1].fX, args[1].fY, &currentSegment);
                if ((fillState == kEmpty_SkipFillState) && (args[0] != lastMovePt)) {
                    fillState = kSingleLine_SkipFillState;
                    break;
                }
                fillState = kNonSingleLine_SkipFillState;
                break;
            case SkPath::kQuad_Verb:
                append_quad(args, &currentSegment);
                fillState = kNonSingleLine_SkipFillState;
                break;
            case SkPath::kConic_Verb: {
                SkAutoConicToQuads converter;
                const SkPoint* quads = converter.computeQuads(args, iter.conicWeight(), tolerance);
                for (int i = 0; i < converter.countQuads(); ++i) {
                    append_quad(&quads[i * 2], &currentSegment);
                }
                fillState = kNonSingleLine_SkipFillState;
            } break;
            case SkPath::kCubic_Verb:
                append_cubic(args[1].fX, args[1].fY, args[2].fX, args[2].fY,
                             args[3].fX, args[3].fY, &currentSegment);
                fillState = kNonSingleLine_SkipFillState;
                break;
            case SkPath::kClose_Verb:
                ClosePath(&currentSegment);
                currentSegment.writeToStream(content);
                currentSegment.reset();
                break;
            default:
                SkASSERT(false);
                break;
        }
    }
    if (currentSegment.bytesWritten() > 0) {
        currentSegment.writeToStream(content);
    }
}

void SkPDFUtils::ClosePath(SkWStream* content) {
    content->writeText("h\n");
}

void SkPDFUtils::PaintPath(SkPaint::Style style, SkPath::FillType fill,
                           SkWStream* content) {
    if (style == SkPaint::kFill_Style) {
        content->writeText("f");
    } else if (style == SkPaint::kStrokeAndFill_Style) {
        content->writeText("B");
    } else if (style == SkPaint::kStroke_Style) {
        content->writeText("S");
    }

    if (style != SkPaint::kStroke_Style) {
        NOT_IMPLEMENTED(fill == SkPath::kInverseEvenOdd_FillType, false);
        NOT_IMPLEMENTED(fill == SkPath::kInverseWinding_FillType, false);
        if (fill == SkPath::kEvenOdd_FillType) {
            content->writeText("*");
        }
    }
    content->writeText("\n");
}

void SkPDFUtils::StrokePath(SkWStream* content) {
    SkPDFUtils::PaintPath(
        SkPaint::kStroke_Style, SkPath::kWinding_FillType, content);
}

void SkPDFUtils::ApplyGraphicState(int objectIndex, SkWStream* content) {
    SkPDFWriteResourceName(content, SkPDFResourceType::kExtGState, objectIndex);
    content->writeText(" gs\n");
}

void SkPDFUtils::ApplyPattern(int objectIndex, SkWStream* content) {
    // Select Pattern color space (CS, cs) and set pattern object as current
    // color (SCN, scn)
    content->writeText("/Pattern CS/Pattern cs");
    SkPDFWriteResourceName(content, SkPDFResourceType::kPattern, objectIndex);
    content->writeText(" SCN");
    SkPDFWriteResourceName(content, SkPDFResourceType::kPattern, objectIndex);
    content->writeText(" scn\n");
}

size_t SkPDFUtils::ColorToDecimal(uint8_t value, char result[5]) {
    if (value == 255 || value == 0) {
        result[0] = value ? '1' : '0';
        result[1] = '\0';
        return 1;
    }
    // int x = 0.5 + (1000.0 / 255.0) * value;
    int x = SkFixedRoundToInt((SK_Fixed1 * 1000 / 255) * value);
    result[0] = '.';
    for (int i = 3; i > 0; --i) {
        result[i] = '0' + x % 10;
        x /= 10;
    }
    int j;
    for (j = 3; j > 1; --j) {
        if (result[j] != '0') {
            break;
        }
    }
    result[j + 1] = '\0';
    return j + 1;
}


bool SkPDFUtils::InverseTransformBBox(const SkMatrix& matrix, SkRect* bbox) {
    SkMatrix inverse;
    if (!matrix.invert(&inverse)) {
        return false;
    }
    inverse.mapRect(bbox);
    return true;
}

void SkPDFUtils::PopulateTilingPatternDict(SkPDFDict* pattern,
                                           SkRect& bbox,
                                           sk_sp<SkPDFDict> resources,
                                           const SkMatrix& matrix) {
    const int kTiling_PatternType = 1;
    const int kColoredTilingPattern_PaintType = 1;
    const int kConstantSpacing_TilingType = 1;

    pattern->insertName("Type", "Pattern");
    pattern->insertInt("PatternType", kTiling_PatternType);
    pattern->insertInt("PaintType", kColoredTilingPattern_PaintType);
    pattern->insertInt("TilingType", kConstantSpacing_TilingType);
    pattern->insertObject("BBox", SkPDFUtils::RectToArray(bbox));
    pattern->insertScalar("XStep", bbox.width());
    pattern->insertScalar("YStep", bbox.height());
    pattern->insertObject("Resources", std::move(resources));
    if (!matrix.isIdentity()) {
        pattern->insertObject("Matrix", SkPDFUtils::MatrixToArray(matrix));
    }
}

bool SkPDFUtils::ToBitmap(const SkImage* img, SkBitmap* dst) {
    SkASSERT(img);
    SkASSERT(dst);
    SkBitmap bitmap;
    if(as_IB(img)->getROPixels(&bitmap, nullptr)) {
        SkASSERT(bitmap.dimensions() == img->dimensions());
        SkASSERT(!bitmap.drawsNothing());
        *dst = std::move(bitmap);
        return true;
    }
    return false;
}
