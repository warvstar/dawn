[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static const float3x3 m = float3x3(float3(0.0f, 1.0f, 2.0f), float3(3.0f, 4.0f, 5.0f), float3(6.0f, 7.0f, 8.0f));

float3x3 f() {
  const float3x3 m_1 = float3x3(float3(0.0f, 1.0f, 2.0f), float3(3.0f, 4.0f, 5.0f), float3(6.0f, 7.0f, 8.0f));
  return m_1;
}
