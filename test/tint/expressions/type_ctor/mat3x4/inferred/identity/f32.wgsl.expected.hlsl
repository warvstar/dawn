[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static const float3x4 m = float3x4(float4(0.0f, 1.0f, 2.0f, 3.0f), float4(4.0f, 5.0f, 6.0f, 7.0f), float4(8.0f, 9.0f, 10.0f, 11.0f));

float3x4 f() {
  const float3x4 m_1 = float3x4(float4(0.0f, 1.0f, 2.0f, 3.0f), float4(4.0f, 5.0f, 6.0f, 7.0f), float4(8.0f, 9.0f, 10.0f, 11.0f));
  return m_1;
}
