#include <metal_stdlib>
#include <SwiftUI/SwiftUI_Metal.h>
using namespace metal;

constant float lumpMax = 60.0;

float rippleLump(float angle, float front) {
    float shape = 0.5 * sin(3.0 * angle + 0.7) + 0.3 * sin(5.0 * angle + 2.1) + 0.2 * sin(8.0 * angle + 4.0);
    return shape * min(0.08 * front, lumpMax);
}

[[ stitchable ]] float2 hexRippleDistort(float2 position, float2 origin, float2 size, float progress, float band) {
    float far = max(max(distance(float2(0.0, 0.0), origin), distance(float2(size.x, 0.0), origin)),
                    max(distance(float2(0.0, size.y), origin), distance(size, origin)));
    float reach = far + band + lumpMax;
    float eased = 1.0 - (1.0 - progress) * (1.0 - progress);
    float front = reach * eased;
    float2 delta = position - origin;
    float d = length(delta);
    float angle = atan2(delta.y, delta.x);
    float behind = front + rippleLump(angle, front) - d;
    float k = (behind - 30.0) / 52.0;
    float ring = exp(-k * k) * smoothstep(-10.0, 6.0, behind);
    float amplitude = 36.0 * ring * pow(max(1.0 - progress, 0.0), 0.7);
    float wave = 0.65 * sin(behind * 0.16) + 0.35 * sin(behind * 0.41 + angle * 4.0);
    float swirl = 0.35 * sin(behind * 0.09 + angle * 6.0);
    float2 direction = d > 0.001 ? delta / d : float2(0.0, 0.0);
    float2 tangent = float2(-direction.y, direction.x);
    return position - (direction * wave + tangent * swirl) * amplitude;
}
