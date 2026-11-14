#include <metal_stdlib>
#include <SwiftUI/SwiftUI_Metal.h>
using namespace metal;

[[ stitchable ]] float2 hexRippleDistort(float2 position, float2 origin, float2 size, float progress, float band) {
    float far = max(max(distance(float2(0.0, 0.0), origin), distance(float2(size.x, 0.0), origin)),
                    max(distance(float2(0.0, size.y), origin), distance(size, origin)));
    float reach = far + band;
    float eased = 1.0 - (1.0 - progress) * (1.0 - progress);
    float front = reach * eased;
    float2 delta = position - origin;
    float d = length(delta);
    float behind = front - d;                                   // how far the leading edge is past this pixel
    float ring = exp(-pow((behind - 30.0) / 38.0, 2.0)) * smoothstep(-10.0, 6.0, behind);
    float amplitude = 14.0 * ring * (1.0 - progress);
    float wave = sin(behind * 0.16);
    float2 direction = d > 0.001 ? delta / d : float2(0.0, 0.0);
    return position - direction * wave * amplitude;
}
