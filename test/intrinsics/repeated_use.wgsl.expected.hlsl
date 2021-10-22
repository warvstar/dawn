bool4 tint_isNormal(float4 param_0) {
  uint4 exponent = asuint(param_0) & 0x7f80000;
  uint4 clamped = clamp(exponent, 0x0080000, 0x7f00000);
  return clamped == exponent;
}

bool3 tint_isNormal_1(float3 param_0) {
  uint3 exponent = asuint(param_0) & 0x7f80000;
  uint3 clamped = clamp(exponent, 0x0080000, 0x7f00000);
  return clamped == exponent;
}

bool2 tint_isNormal_2(float2 param_0) {
  uint2 exponent = asuint(param_0) & 0x7f80000;
  uint2 clamped = clamp(exponent, 0x0080000, 0x7f00000);
  return clamped == exponent;
}

bool tint_isNormal_3(float param_0) {
  uint exponent = asuint(param_0) & 0x7f80000;
  uint clamped = clamp(exponent, 0x0080000, 0x7f00000);
  return clamped == exponent;
}

[numthreads(1, 1, 1)]
void main() {
  (void) tint_isNormal(float4(0.0f, 0.0f, 0.0f, 0.0f));
  (void) tint_isNormal(float4((1.0f).xxxx));
  (void) tint_isNormal(float4(1.0f, 2.0f, 3.0f, 4.0f));
  (void) tint_isNormal_1(float3(0.0f, 0.0f, 0.0f));
  (void) tint_isNormal_1(float3((1.0f).xxx));
  (void) tint_isNormal_1(float3(1.0f, 2.0f, 3.0f));
  (void) tint_isNormal_2(float2(0.0f, 0.0f));
  (void) tint_isNormal_2(float2((1.0f).xx));
  (void) tint_isNormal_2(float2(1.0f, 2.0f));
  (void) tint_isNormal_3(1.0f);
  (void) tint_isNormal_3(2.0f);
  (void) tint_isNormal_3(3.0f);
  return;
}
