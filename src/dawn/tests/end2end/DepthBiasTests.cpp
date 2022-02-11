// Copyright 2020 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dawn/common/Constants.h"
#include "dawn/common/Math.h"
#include "dawn/tests/DawnTest.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/TextureUtils.h"
#include "dawn/utils/WGPUHelpers.h"

constexpr static unsigned int kRTSize = 2;

enum class QuadAngle { Flat, TiltedX };

class DepthBiasTests : public DawnTest {
  protected:
    void RunDepthBiasTest(wgpu::TextureFormat depthFormat,
                          float depthClear,
                          QuadAngle quadAngle,
                          int32_t bias,
                          float biasSlopeScale,
                          float biasClamp) {
        const char* vertexSource = nullptr;
        switch (quadAngle) {
            case QuadAngle::Flat:
                // Draw a square at z = 0.25
                vertexSource = R"(
    @stage(vertex)
    fn main(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4<f32> {
        var pos = array<vec2<f32>, 6>(
            vec2<f32>(-1.0, -1.0),
            vec2<f32>( 1.0, -1.0),
            vec2<f32>(-1.0,  1.0),
            vec2<f32>(-1.0,  1.0),
            vec2<f32>( 1.0, -1.0),
            vec2<f32>( 1.0,  1.0));
        return vec4<f32>(pos[VertexIndex], 0.25, 1.0);
    })";
                break;

            case QuadAngle::TiltedX:
                // Draw a square ranging from 0 to 0.5, bottom to top
                vertexSource = R"(
    @stage(vertex)
    fn main(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4<f32> {
        var pos = array<vec3<f32>, 6>(
            vec3<f32>(-1.0, -1.0, 0.0),
            vec3<f32>( 1.0, -1.0, 0.0),
            vec3<f32>(-1.0,  1.0, 0.5),
            vec3<f32>(-1.0,  1.0, 0.5),
            vec3<f32>( 1.0, -1.0, 0.0),
            vec3<f32>( 1.0,  1.0, 0.5));
        return vec4<f32>(pos[VertexIndex], 1.0);
    })";
                break;
        }

        wgpu::ShaderModule vertexModule = utils::CreateShaderModule(device, vertexSource);

        wgpu::ShaderModule fragmentModule = utils::CreateShaderModule(device, R"(
    @stage(fragment) fn main() -> @location(0) vec4<f32> {
        return vec4<f32>(1.0, 0.0, 0.0, 1.0);
    })");

        {
            wgpu::TextureDescriptor descriptor;
            descriptor.size = {kRTSize, kRTSize, 1};
            descriptor.format = depthFormat;
            descriptor.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
            mDepthTexture = device.CreateTexture(&descriptor);
        }

        {
            wgpu::TextureDescriptor descriptor;
            descriptor.size = {kRTSize, kRTSize, 1};
            descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
            descriptor.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
            mRenderTarget = device.CreateTexture(&descriptor);
        }

        // Create a render pass which clears depth to depthClear
        utils::ComboRenderPassDescriptor renderPassDesc({mRenderTarget.CreateView()},
                                                        mDepthTexture.CreateView());
        renderPassDesc.cDepthStencilAttachmentInfo.clearDepth = depthClear;

        // Create a render pipeline to render the quad
        utils::ComboRenderPipelineDescriptor renderPipelineDesc;

        renderPipelineDesc.vertex.module = vertexModule;
        renderPipelineDesc.cFragment.module = fragmentModule;
        wgpu::DepthStencilState* depthStencil = renderPipelineDesc.EnableDepthStencil(depthFormat);
        depthStencil->depthWriteEnabled = true;
        depthStencil->depthBias = bias;
        depthStencil->depthBiasSlopeScale = biasSlopeScale;
        depthStencil->depthBiasClamp = biasClamp;

        if (depthFormat != wgpu::TextureFormat::Depth32Float) {
            depthStencil->depthCompare = wgpu::CompareFunction::Greater;
        }

        wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&renderPipelineDesc);

        // Draw the quad (two triangles)
        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPassDesc);
        pass.SetPipeline(pipeline);
        pass.Draw(6);
        pass.End();

        wgpu::CommandBuffer commands = commandEncoder.Finish();
        queue.Submit(1, &commands);
    }

    // Floating point depth buffers use the following formula to calculate bias
    // bias = depthBias * 2 ** (exponent(max z of primitive) - number of bits in mantissa) +
    //        slopeScale * maxSlope
    // https://docs.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-output-merger-stage-depth-bias
    // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetDepthBias.html
    // https://developer.apple.com/documentation/metal/mtlrendercommandencoder/1516269-setdepthbias
    //
    // To get a final bias of 0.25 for primitives with z = 0.25, we can use
    // depthBias = 0.25 / (2 ** (-2 - 23)) = 8388608
    static constexpr int32_t kPointTwoFiveBiasForPointTwoFiveZOnFloat = 8388608;

    wgpu::Texture mDepthTexture;
    wgpu::Texture mRenderTarget;
};

// Test adding positive bias to output
TEST_P(DepthBiasTests, PositiveBiasOnFloat) {
    // NVIDIA GPUs under Vulkan seem to be using a different scale than everyone else.
    DAWN_SUPPRESS_TEST_IF(IsVulkan() && IsNvidia());

    // OpenGL uses a different scale than the other APIs
    DAWN_TEST_UNSUPPORTED_IF(IsOpenGL());
    DAWN_TEST_UNSUPPORTED_IF(IsOpenGLES());

    // Draw quad flat on z = 0.25 with 0.25 bias
    RunDepthBiasTest(wgpu::TextureFormat::Depth32Float, 0, QuadAngle::Flat,
                     kPointTwoFiveBiasForPointTwoFiveZOnFloat, 0, 0);

    // Quad at z = 0.25 + 0.25 bias = 0.5
    std::vector<float> expected = {
        0.5, 0.5,  //
        0.5, 0.5,  //
    };

    EXPECT_TEXTURE_EQ(expected.data(), mDepthTexture, {0, 0}, {kRTSize, kRTSize}, 0,
                      wgpu::TextureAspect::DepthOnly);
}

// Test adding positive bias to output with a clamp
TEST_P(DepthBiasTests, PositiveBiasOnFloatWithClamp) {
    // Clamping support in OpenGL is spotty
    DAWN_TEST_UNSUPPORTED_IF(IsOpenGL());
    DAWN_TEST_UNSUPPORTED_IF(IsOpenGLES());

    // Draw quad flat on z = 0.25 with 0.25 bias clamped at 0.125.
    RunDepthBiasTest(wgpu::TextureFormat::Depth32Float, 0, QuadAngle::Flat,
                     kPointTwoFiveBiasForPointTwoFiveZOnFloat, 0, 0.125);

    // Quad at z = 0.25 + min(0.25 bias, 0.125 clamp) = 0.375
    std::vector<float> expected = {
        0.375, 0.375,  //
        0.375, 0.375,  //
    };

    EXPECT_TEXTURE_EQ(expected.data(), mDepthTexture, {0, 0}, {kRTSize, kRTSize}, 0,
                      wgpu::TextureAspect::DepthOnly);
}

// Test adding negative bias to output
TEST_P(DepthBiasTests, NegativeBiasOnFloat) {
    // NVIDIA GPUs seems to be using a different scale than everyone else
    DAWN_SUPPRESS_TEST_IF(IsVulkan() && IsNvidia());

    // OpenGL uses a different scale than the other APIs
    DAWN_TEST_UNSUPPORTED_IF(IsOpenGL());

    // Draw quad flat on z = 0.25 with -0.25 bias, depth clear of 0.125
    RunDepthBiasTest(wgpu::TextureFormat::Depth32Float, 0.125, QuadAngle::Flat,
                     -kPointTwoFiveBiasForPointTwoFiveZOnFloat, 0, 0);

    // Quad at z = 0.25 - 0.25 bias = 0
    std::vector<float> expected = {
        0.0, 0.0,  //
        0.0, 0.0,  //
    };

    EXPECT_TEXTURE_EQ(expected.data(), mDepthTexture, {0, 0}, {kRTSize, kRTSize}, 0,
                      wgpu::TextureAspect::DepthOnly);
}

// Test adding negative bias to output with a clamp
TEST_P(DepthBiasTests, NegativeBiasOnFloatWithClamp) {
    // Clamping support in OpenGL is spotty
    DAWN_TEST_UNSUPPORTED_IF(IsOpenGL());
    DAWN_TEST_UNSUPPORTED_IF(IsOpenGLES());

    // Draw quad flat on z = 0.25 with -0.25 bias clamped at -0.125.
    RunDepthBiasTest(wgpu::TextureFormat::Depth32Float, 0, QuadAngle::Flat,
                     -kPointTwoFiveBiasForPointTwoFiveZOnFloat, 0, -0.125);

    // Quad at z = 0.25 + max(-0.25 bias, -0.125 clamp) = 0.125
    std::vector<float> expected = {
        0.125, 0.125,  //
        0.125, 0.125,  //
    };

    EXPECT_TEXTURE_EQ(expected.data(), mDepthTexture, {0, 0}, {kRTSize, kRTSize}, 0,
                      wgpu::TextureAspect::DepthOnly);
}

// Test adding positive infinite slope bias to output
TEST_P(DepthBiasTests, PositiveInfinitySlopeBiasOnFloat) {
    // NVIDIA GPUs do not clamp values to 1 when using Inf slope bias.
    DAWN_SUPPRESS_TEST_IF(IsVulkan() && IsNvidia());

    // Draw quad with z from 0 to 0.5 with inf slope bias
    RunDepthBiasTest(wgpu::TextureFormat::Depth32Float, 0.125, QuadAngle::TiltedX, 0,
                     std::numeric_limits<float>::infinity(), 0);

    // Value at the center of the pixel + (0.25 slope * Inf slope bias) = 1 (clamped)
    std::vector<float> expected = {
        1.0, 1.0,  //
        1.0, 1.0,  //
    };

    EXPECT_TEXTURE_EQ(expected.data(), mDepthTexture, {0, 0}, {kRTSize, kRTSize}, 0,
                      wgpu::TextureAspect::DepthOnly);
}

// Test adding positive infinite slope bias to output
TEST_P(DepthBiasTests, NegativeInfinityBiasOnFloat) {
    // NVIDIA GPUs do not clamp values to 0 when using -Inf slope bias.
    DAWN_SUPPRESS_TEST_IF(IsVulkan() && IsNvidia());

    // Draw quad with z from 0 to 0.5 with -inf slope bias
    RunDepthBiasTest(wgpu::TextureFormat::Depth32Float, 0.125, QuadAngle::TiltedX, 0,
                     -std::numeric_limits<float>::infinity(), 0);

    // Value at the center of the pixel + (0.25 slope * -Inf slope bias) = 0 (clamped)
    std::vector<float> expected = {
        0.0, 0.0,  //
        0.0, 0.0,  //
    };

    EXPECT_TEXTURE_EQ(expected.data(), mDepthTexture, {0, 0}, {kRTSize, kRTSize}, 0,
                      wgpu::TextureAspect::DepthOnly);
}

// Test tiledX quad with no bias
TEST_P(DepthBiasTests, NoBiasTiltedXOnFloat) {
    // Draw quad with z from 0 to 0.5 with no bias
    RunDepthBiasTest(wgpu::TextureFormat::Depth32Float, 0, QuadAngle::TiltedX, 0, 0, 0);

    // Depth values of TiltedX quad. Values at the center of the pixels.
    std::vector<float> expected = {
        0.375, 0.375,  //
        0.125, 0.125,  //
    };

    EXPECT_TEXTURE_EQ(expected.data(), mDepthTexture, {0, 0}, {kRTSize, kRTSize}, 0,
                      wgpu::TextureAspect::DepthOnly);
}

// Test adding positive slope bias to output
TEST_P(DepthBiasTests, PositiveSlopeBiasOnFloat) {
    // Draw quad with z from 0 to 0.5 with a slope bias of 1
    RunDepthBiasTest(wgpu::TextureFormat::Depth32Float, 0, QuadAngle::TiltedX, 0, 1, 0);

    // Value at the center of the pixel + (0.25 slope * 1.0 slope bias)
    std::vector<float> expected = {
        0.625, 0.625,  //
        0.375, 0.375,  //
    };

    EXPECT_TEXTURE_EQ(expected.data(), mDepthTexture, {0, 0}, {kRTSize, kRTSize}, 0,
                      wgpu::TextureAspect::DepthOnly);
}

// Test adding negative half slope bias to output
TEST_P(DepthBiasTests, NegativeHalfSlopeBiasOnFloat) {
    // Draw quad with z from 0 to 0.5 with a slope bias of -0.5
    RunDepthBiasTest(wgpu::TextureFormat::Depth32Float, 0, QuadAngle::TiltedX, 0, -0.5, 0);

    // Value at the center of the pixel + (0.25 slope * -0.5 slope bias)
    std::vector<float> expected = {
        0.25, 0.25,  //
        0.0, 0.0,    //
    };

    EXPECT_TEXTURE_EQ(expected.data(), mDepthTexture, {0, 0}, {kRTSize, kRTSize}, 0,
                      wgpu::TextureAspect::DepthOnly);
}

// Test adding positive bias to output
TEST_P(DepthBiasTests, PositiveBiasOn24bit) {
    // Draw quad flat on z = 0.25 with 0.25 bias
    RunDepthBiasTest(wgpu::TextureFormat::Depth24PlusStencil8, 0.4f, QuadAngle::Flat,
                     0.25f * (1 << 25), 0, 0);

    // Only the bottom left quad has colors. 0.5 quad > 0.4 clear.
    // TODO(crbug.com/dawn/820): Switch to depth sampling once feature has been enabled.
    std::vector<RGBA8> expected = {
        RGBA8::kRed, RGBA8::kRed,  //
        RGBA8::kRed, RGBA8::kRed,  //
    };

    EXPECT_TEXTURE_EQ(expected.data(), mRenderTarget, {0, 0}, {kRTSize, kRTSize});
}

// Test adding positive bias to output with a clamp
TEST_P(DepthBiasTests, PositiveBiasOn24bitWithClamp) {
    // Clamping support in OpenGL is spotty
    DAWN_TEST_UNSUPPORTED_IF(IsOpenGL());
    DAWN_TEST_UNSUPPORTED_IF(IsOpenGLES());

    // Draw quad flat on z = 0.25 with 0.25 bias clamped at 0.125.
    RunDepthBiasTest(wgpu::TextureFormat::Depth24PlusStencil8, 0.4f, QuadAngle::Flat,
                     0.25f * (1 << 25), 0, 0.1f);

    // Since we cleared with a depth of 0.4 and clamped bias at 0.4, the depth test will fail. 0.25
    // + 0.125 < 0.4 clear.
    // TODO(crbug.com/dawn/820): Switch to depth sampling once feature has been enabled.
    std::vector<RGBA8> zero = {
        RGBA8::kZero, RGBA8::kZero,  //
        RGBA8::kZero, RGBA8::kZero,  //
    };

    EXPECT_TEXTURE_EQ(zero.data(), mRenderTarget, {0, 0}, {kRTSize, kRTSize});
}

// Test adding positive bias to output
TEST_P(DepthBiasTests, PositiveSlopeBiasOn24bit) {
    // Draw quad with z from 0 to 0.5 with a slope bias of 1
    RunDepthBiasTest(wgpu::TextureFormat::Depth24PlusStencil8, 0.4f, QuadAngle::TiltedX, 0, 1, 0);

    // Only the top half of the quad has a depth > 0.4 clear
    // TODO(crbug.com/dawn/820): Switch to depth sampling once feature has been enabled.
    std::vector<RGBA8> expected = {
        RGBA8::kRed, RGBA8::kRed,    //
        RGBA8::kZero, RGBA8::kZero,  //
    };

    EXPECT_TEXTURE_EQ(expected.data(), mRenderTarget, {0, 0}, {kRTSize, kRTSize});
}

DAWN_INSTANTIATE_TEST(DepthBiasTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());