SKIP: FAILED

#version 310 es
precision mediump float;


layout(binding = 1) buffer SB_RO_1 {
  uint arg_0[];
} sb_ro;

void arrayLength_cfca0a() {
  uint tint_symbol_2 = 0u;
  sb_ro.GetDimensions(tint_symbol_2);
  uint tint_symbol_3 = ((tint_symbol_2 - 0u) / 4u);
  uint res = tint_symbol_3;
}

struct tint_symbol {
  vec4 value;
};

vec4 vertex_main_inner() {
  arrayLength_cfca0a();
  return vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  vec4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = tint_symbol(vec4(0.0f, 0.0f, 0.0f, 0.0f));
  wrapper_result.value = inner_result;
  return wrapper_result;
}
void main() {
  tint_symbol outputs;
  outputs = vertex_main();
  gl_Position = outputs.value;
  gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
  gl_Position.y = -gl_Position.y;
}


Error parsing GLSL shader:
ERROR: 0:11: 'GetDimensions' : no such field in structure 
ERROR: 0:11: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



#version 310 es
precision mediump float;


layout(binding = 1) buffer SB_RO_1 {
  uint arg_0[];
} sb_ro;

void arrayLength_cfca0a() {
  uint tint_symbol_2 = 0u;
  sb_ro.GetDimensions(tint_symbol_2);
  uint tint_symbol_3 = ((tint_symbol_2 - 0u) / 4u);
  uint res = tint_symbol_3;
}

struct tint_symbol {
  vec4 value;
};

void fragment_main() {
  arrayLength_cfca0a();
  return;
}
void main() {
  fragment_main();
}


Error parsing GLSL shader:
ERROR: 0:11: 'GetDimensions' : no such field in structure 
ERROR: 0:11: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



#version 310 es
precision mediump float;


layout(binding = 1) buffer SB_RO_1 {
  uint arg_0[];
} sb_ro;

void arrayLength_cfca0a() {
  uint tint_symbol_2 = 0u;
  sb_ro.GetDimensions(tint_symbol_2);
  uint tint_symbol_3 = ((tint_symbol_2 - 0u) / 4u);
  uint res = tint_symbol_3;
}

struct tint_symbol {
  vec4 value;
};

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void compute_main() {
  arrayLength_cfca0a();
  return;
}
void main() {
  compute_main();
}


Error parsing GLSL shader:
ERROR: 0:11: 'GetDimensions' : no such field in structure 
ERROR: 0:11: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



