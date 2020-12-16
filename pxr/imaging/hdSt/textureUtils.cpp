//
// Copyright 2020 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//

#include "pxr/imaging/hdSt/textureUtils.h"

#include "pxr/base/gf/half.h"
#include "pxr/base/gf/math.h"
#include "pxr/base/trace/trace.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace {

template<typename T>
constexpr T
_OpaqueAlpha() {
    return std::numeric_limits<T>::is_integer ?
        std::numeric_limits<T>::max() : T(1);
}

template<typename T>
void
_ConvertRGBToRGBA(
    const void * const src,
    const size_t numTexels,
    void * const dst)
{
    TRACE_FUNCTION();

    const T * const typedSrc = reinterpret_cast<const T*>(src);
    T * const typedDst = reinterpret_cast<T*>(dst);

    size_t i = numTexels;
    // Going backward so that we can convert in place.
    while (i--) {
        typedDst[4 * i + 0] = typedSrc[3 * i + 0];
        typedDst[4 * i + 1] = typedSrc[3 * i + 1];
        typedDst[4 * i + 2] = typedSrc[3 * i + 2];
        typedDst[4 * i + 3] = _OpaqueAlpha<T>();
    }
}

enum _ColorSpaceTransform
{
     _SRGBToLinear,
     _LinearToSRGB
};

// Convert a [0, 1] value between color spaces
template<_ColorSpaceTransform colorSpaceTransform>
float _ConvertColorSpace(const float in)
{
    float out = in;
    if (colorSpaceTransform == _SRGBToLinear) {
        if (in <= 0.04045) {
            out = in / 12.92;
        } else {
            out = pow((in + 0.055) / 1.055, 2.4);
        }
    } else if (colorSpaceTransform == _LinearToSRGB) {
        if (in <= 0.0031308) {
            out = 12.92 * in;
        } else {
            out = 1.055 * pow(in, 1.0 / 2.4) - 0.055;
        }
    }

    return GfClamp(out, 0.f, 1.f);
}

// Pre-multiply alpha function to be used for integral types
template<typename T, bool isSRGB>
void
_PremultiplyAlpha(
    const void * const src,
    const size_t numTexels,
    void * const dst)
{
    TRACE_FUNCTION();

    static_assert(std::numeric_limits<T>::is_integer, "Requires integral type");

    const T * const typedSrc = reinterpret_cast<const T*>(src);
    T * const typedDst = reinterpret_cast<T*>(dst);

    // Perform all operations using floats.
    constexpr float max = static_cast<float>(std::numeric_limits<T>::max());

    for (size_t i = 0; i < numTexels; i++) {
        const float alpha = static_cast<float>(typedSrc[4 * i + 3]) / max;

        for (size_t j = 0; j < 3; j++) {
            float p = static_cast<float>(typedSrc[4 * i + j]);

            if (isSRGB) {
                // Convert value from sRGB to linear.
                p = max * _ConvertColorSpace<_SRGBToLinear>(p / max);
            }  
            
            // Pre-multiply RGB values with alpha in linear space.
            p *= alpha;

            if (isSRGB) {
                // Convert value from linear to sRGB.
                p = max * _ConvertColorSpace<_LinearToSRGB>(p / max);
            }

            // Add 0.5 when converting float to integral type.
            typedDst[4 * i + j] = p + 0.5f;  
        }
        // Only necessary when not converting in place.
        typedDst[4 * i + 3] = typedSrc[4 * i + 3];
    }
}

// Pre-multiply alpha function to be used for floating point types
template<typename T>
void
_PremultiplyAlphaFloat(
    const void * const src,
    const size_t numTexels,
    void * const dst)
{
    TRACE_FUNCTION();

    static_assert(GfIsFloatingPoint<T>::value, "Requires floating point type");

    const T * const typedSrc = reinterpret_cast<const T*>(src);
    T * const typedDst = reinterpret_cast<T*>(dst);

    for (size_t i = 0; i < numTexels; i++) {
        const T alpha = typedSrc[4 * i + 3];

        // Pre-multiply RGB values with alpha.
        for (size_t j = 0; j < 3; j++) {
            typedDst[4 * i + j] = typedSrc[4 * i + j] * alpha;
        }
        typedDst[4 * i + 3] = typedSrc[4 * i + 3];
    }
}

} // anonymous namespace

HgiFormat
HdStTextureUtils::GetHgiFormat(
    const HioFormat hioFormat,
    const bool premultiplyAlpha,
    const bool avoidThreeComponentFormats,
    ConversionFunction * const conversionFunction)
{
    // Format dispatch, mostly we can just use the CPU buffer from
    // the texture data provided.
    switch(hioFormat) {

        // UNorm 8.
        case HioFormatUNorm8:
            return HgiFormatUNorm8;
        case HioFormatUNorm8Vec2:
            return HgiFormatUNorm8Vec2;
        case HioFormatUNorm8Vec3:
            // RGB (24bit) is not supported on MTL, so we need to
            // always convert it.
            *conversionFunction =
                _ConvertRGBToRGBA<unsigned char>;
            return HgiFormatUNorm8Vec4;
        case HioFormatUNorm8Vec4: 
            if (premultiplyAlpha) {
                *conversionFunction =
                    _PremultiplyAlpha<unsigned char, /* isSRGB = */ false>;
            }
            return HgiFormatUNorm8Vec4;
            
        // SNorm8
        case HioFormatSNorm8:
            return HgiFormatSNorm8;
        case HioFormatSNorm8Vec2:
            return HgiFormatSNorm8Vec2;
        case HioFormatSNorm8Vec3:
            *conversionFunction =
                _ConvertRGBToRGBA<signed char>;
            return HgiFormatSNorm8Vec4;
        case HioFormatSNorm8Vec4:
            if (premultiplyAlpha) {
                // Pre-multiplying only makes sense for RGBA colors and
                // the signed integers do not make sense for RGBA.
                //
                // However, for consistency, we do premultiply here so
                // that one can tell from the material network topology
                // only whether premultiplication is happening.
                //
                *conversionFunction =
                    _PremultiplyAlpha<signed char, /* isSRGB = */ false>;
            }
            return HgiFormatSNorm8Vec4;

        // Float16
        case HioFormatFloat16:
            return HgiFormatFloat16;
        case HioFormatFloat16Vec2:
            return HgiFormatFloat16Vec2;
        case HioFormatFloat16Vec3:
            if (avoidThreeComponentFormats) {
                *conversionFunction =
                    _ConvertRGBToRGBA<GfHalf>;
                return HgiFormatFloat16Vec4;
            }
            return HgiFormatFloat16Vec3;
        case HioFormatFloat16Vec4:
            if (premultiplyAlpha) {
                *conversionFunction =
                    _PremultiplyAlphaFloat<GfHalf>;
            }
            return HgiFormatFloat16Vec4;

        // Float32
        case HioFormatFloat32:
            return HgiFormatFloat32;
        case HioFormatFloat32Vec2:
            return HgiFormatFloat32Vec2;
        case HioFormatFloat32Vec3:
            if (avoidThreeComponentFormats) {
                *conversionFunction =
                    _ConvertRGBToRGBA<float>;
                return HgiFormatFloat32Vec4;
            }
            return HgiFormatFloat32Vec3;
        case HioFormatFloat32Vec4:
            if (premultiplyAlpha) {
                *conversionFunction =
                    _PremultiplyAlphaFloat<float>;
            }
            return HgiFormatFloat32Vec4;

        // Double64
        case HioFormatDouble64:
        case HioFormatDouble64Vec2:
        case HioFormatDouble64Vec3:
        case HioFormatDouble64Vec4:
            TF_WARN("Double texture formats not supported by Storm");
            return HgiFormatInvalid;
            
        // UInt16
        case HioFormatUInt16:
            return HgiFormatUInt16;
        case HioFormatUInt16Vec2:
            return HgiFormatUInt16Vec2;
        case HioFormatUInt16Vec3:
            if (avoidThreeComponentFormats) {
                *conversionFunction =
                    _ConvertRGBToRGBA<uint16_t>;
                return HgiFormatUInt16Vec4;
            }
            return HgiFormatUInt16Vec3;
        case HioFormatUInt16Vec4:
            if (premultiplyAlpha) {
                // Pre-multiplying only makes sense for RGBA colors and
                // the signed integers do not make sense for RGBA.
                //
                // However, for consistency, we do premultiply here so
                // that one can tell from the material network topology
                // only whether premultiplication is happening.
                //
                *conversionFunction =
                    _PremultiplyAlpha<uint16_t, /* isSRGB = */ false>;
            }
            return HgiFormatUInt16Vec4;
            
        // Int16
        case HioFormatInt16:
        case HioFormatInt16Vec2:
        case HioFormatInt16Vec3:
        case HioFormatInt16Vec4:
            TF_WARN("Signed 16-bit integer texture formats "
                    "not supported by Storm");
            return HgiFormatInvalid;

        // UInt32
        case HioFormatUInt32:
        case HioFormatUInt32Vec2:
        case HioFormatUInt32Vec3:
        case HioFormatUInt32Vec4:
            TF_WARN("Unsigned 32-bit integer texture formats "
                    "not supported by Storm");
            return HgiFormatInvalid;

        // Int32
        case HioFormatInt32:
            return HgiFormatInt32;
        case HioFormatInt32Vec2:
            return HgiFormatInt32Vec2;
        case HioFormatInt32Vec3:
            if (avoidThreeComponentFormats) {
                *conversionFunction =
                    _ConvertRGBToRGBA<int32_t>;
                return HgiFormatInt32Vec4;
            }
            return HgiFormatInt32Vec3;
        case HioFormatInt32Vec4:
            // Pre-multiplying only makes sense for RGBA colors and
            // the signed integers do not make sense for RGBA.
            //
            // However, for consistency, we do premultiply here so
            // that one can tell from the material network topology
            // only whether premultiplication is happening.
            //
            if (premultiplyAlpha) {
                *conversionFunction =
                    _PremultiplyAlpha<int32_t, /* isSRGB = */ false>;
            }
            return HgiFormatInt32Vec4;

        // UNorm8 SRGB
        case HioFormatUNorm8srgb:
        case HioFormatUNorm8Vec2srgb:
            TF_WARN("One and two channel srgb texture formats "
                    "not supported by Storm");
            return HgiFormatInvalid;
        case HioFormatUNorm8Vec3srgb:
            // RGB (24bit) is not supported on MTL, so we need to convert it.
            *conversionFunction =
                _ConvertRGBToRGBA<unsigned char>;
            return HgiFormatUNorm8Vec4srgb;
        case HioFormatUNorm8Vec4srgb:
            if (premultiplyAlpha) {
                *conversionFunction =
                    _PremultiplyAlpha<unsigned char, /* isSRGB = */ true>;
            }
            return HgiFormatUNorm8Vec4srgb;

        // BPTC compressed
        case HioFormatBC6FloatVec3:
            return HgiFormatBC6FloatVec3;
        case HioFormatBC6UFloatVec3:
            return HgiFormatBC6UFloatVec3;
        case HioFormatBC7UNorm8Vec4:
            return HgiFormatBC7UNorm8Vec4;
        case HioFormatBC7UNorm8Vec4srgb:
            // Pre-multiplying alpha would require decompressing and
            // recompressing, so not doing it here.
            return HgiFormatBC7UNorm8Vec4srgb;

        // S3TC/DXT compressed
        case HioFormatBC1UNorm8Vec4:
            return HgiFormatBC1UNorm8Vec4;
        case HioFormatBC3UNorm8Vec4:
            // Pre-multiplying alpha would require decompressing and
            // recompressing, so not doing it here.
            return HgiFormatBC3UNorm8Vec4;

        case HioFormatInvalid:
            return HgiFormatInvalid;
        case HioFormatCount:
            TF_CODING_ERROR("HioFormatCount passed to function");
            return HgiFormatInvalid;
    }

    TF_CODING_ERROR("Invalid HioFormat enum value");
    return HgiFormatInvalid;
}

PXR_NAMESPACE_CLOSE_SCOPE