void dot_883f0e() {
  float2 arg_0 = (0.0f).xx;
  float2 arg_1 = (0.0f).xx;
  float res = dot(arg_0, arg_1);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  dot_883f0e();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  dot_883f0e();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  dot_883f0e();
  return;
}