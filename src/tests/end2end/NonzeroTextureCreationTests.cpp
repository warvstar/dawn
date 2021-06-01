// Copyright 2019 The Dawn Authors
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

#include "tests/DawnTest.h"

#include "common/Constants.h"
#include "common/Math.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/TestUtils.h"
#include "utils/WGPUHelpers.h"

namespace {

    using Format = wgpu::TextureFormat;
    using Usage = wgpu::TextureUsage;
    using Dimension = wgpu::TextureDimension;
    using DepthOrArrayLayers = uint32_t;
    using Mip = uint32_t;

    DAWN_TEST_PARAM_STRUCT(Params, Format, Usage, Dimension, DepthOrArrayLayers, Mip)

    template <typename T>
    class ExpectNonZero : public detail::CustomTextureExpectation {
      public:
        uint32_t DataSize() override {
            return sizeof(T);
        }

        testing::AssertionResult Check(const void* data, size_t size) override {
            ASSERT(size % DataSize() == 0 && size > 0);
            const T* actual = static_cast<const T*>(data);
            T value = *actual;
            if (value == T(0)) {
                return testing::AssertionFailure()
                       << "Expected data to be non-zero, was " << value << std::endl;
            }
            for (size_t i = 0; i < size / DataSize(); ++i) {
                if (actual[i] != value) {
                    return testing::AssertionFailure()
                           << "Expected data[" << i << "] to be " << value << ", actual "
                           << actual[i] << std::endl;
                }
            }

            return testing::AssertionSuccess();
        }
    };

#define EXPECT_TEXTURE_NONZERO(T, ...) \
    AddTextureExpectation(__FILE__, __LINE__, new ExpectNonZero<T>(), __VA_ARGS__)

    class NonzeroTextureCreationTests : public DawnTestWithParams<Params> {
      protected:
        constexpr static uint32_t kSize = 128;
        constexpr static uint32_t kMipLevelCount = 4;

        std::vector<const char*> GetRequiredExtensions() override {
            if (GetParam().mFormat == wgpu::TextureFormat::BC1RGBAUnorm &&
                SupportsExtensions({"texture_compression_bc"})) {
                return {"texture_compression_bc"};
            }
            return {};
        }

        void Run() {
            DAWN_TEST_UNSUPPORTED_IF(GetParam().mFormat == wgpu::TextureFormat::BC1RGBAUnorm &&
                                     !SupportsExtensions({"texture_compression_bc"}));

            // TODO(crbug.com/dawn/667): Work around the fact that some platforms do not support
            // reading from Snorm textures.
            DAWN_TEST_UNSUPPORTED_IF(GetParam().mFormat == wgpu::TextureFormat::RGBA8Snorm &&
                                     HasToggleEnabled("disable_snorm_read"));

            // TODO(crbug.com/dawn/547): 3D texture copies not fully implemented on D3D12.
            // TODO(crbug.com/angleproject/5967): This texture readback hits an assert in ANGLE.
            DAWN_SUPPRESS_TEST_IF(GetParam().mDimension == wgpu::TextureDimension::e3D &&
                                  (IsANGLE() || IsD3D12()));

            // TODO(crbug.com/dawn/791): Determine Intel specific platforms this occurs on, and
            // implement a workaround on all backends (happens on Windows too, but not on our test
            // machines).
            DAWN_SUPPRESS_TEST_IF(GetParam().mFormat == wgpu::TextureFormat::Depth32Float &&
                                  IsMetal() && IsIntel() && GetParam().mMip != 0);

            // Copies from depth textures not fully supported on the OpenGL backend right now.
            DAWN_SUPPRESS_TEST_IF(GetParam().mFormat == wgpu::TextureFormat::Depth32Float &&
                                  (IsOpenGL() || IsOpenGLES()));

            // GL may support the extension, but reading data back is not implemented.
            DAWN_TEST_UNSUPPORTED_IF(GetParam().mFormat == wgpu::TextureFormat::BC1RGBAUnorm &&
                                     (IsOpenGL() || IsOpenGLES()));

            wgpu::TextureDescriptor descriptor;
            descriptor.dimension = GetParam().mDimension;
            descriptor.size.width = kSize;
            descriptor.size.height = kSize;
            descriptor.size.depthOrArrayLayers = GetParam().mDepthOrArrayLayers;
            descriptor.sampleCount = 1;
            descriptor.format = GetParam().mFormat;
            descriptor.usage = GetParam().mUsage;
            descriptor.mipLevelCount = kMipLevelCount;

            wgpu::Texture texture = device.CreateTexture(&descriptor);

            uint32_t mip = GetParam().mMip;
            uint32_t mipSize = std::max(kSize >> mip, 1u);
            uint32_t depthOrArrayLayers = GetParam().mDimension == wgpu::TextureDimension::e3D
                                              ? std::max(GetParam().mDepthOrArrayLayers >> mip, 1u)
                                              : GetParam().mDepthOrArrayLayers;

            switch (GetParam().mFormat) {
                case wgpu::TextureFormat::R8Unorm: {
                    EXPECT_TEXTURE_EQ(new ExpectNonZero<uint8_t>(), texture, {0, 0, 0},
                                      {mipSize, mipSize, depthOrArrayLayers}, mip);
                    break;
                }
                case wgpu::TextureFormat::RG8Unorm: {
                    EXPECT_TEXTURE_EQ(new ExpectNonZero<uint16_t>(), texture, {0, 0, 0},
                                      {mipSize, mipSize, depthOrArrayLayers}, mip);
                    break;
                }
                case wgpu::TextureFormat::RGBA8Unorm:
                case wgpu::TextureFormat::RGBA8Snorm: {
                    EXPECT_TEXTURE_EQ(new ExpectNonZero<uint32_t>(), texture, {0, 0, 0},
                                      {mipSize, mipSize, depthOrArrayLayers}, mip);
                    break;
                }
                case wgpu::TextureFormat::Depth32Float: {
                    EXPECT_TEXTURE_EQ(new ExpectNonZero<float>(), texture, {0, 0, 0},
                                      {mipSize, mipSize, depthOrArrayLayers}, mip);
                    break;
                }
                case wgpu::TextureFormat::BC1RGBAUnorm: {
                    // Set buffer with dirty data so we know it is cleared by the lazy cleared
                    // texture copy
                    uint32_t blockWidth = utils::GetTextureFormatBlockWidth(GetParam().mFormat);
                    uint32_t blockHeight = utils::GetTextureFormatBlockHeight(GetParam().mFormat);
                    wgpu::Extent3D copySize = {Align(mipSize, blockWidth),
                                               Align(mipSize, blockHeight), depthOrArrayLayers};

                    uint32_t bytesPerRow =
                        utils::GetMinimumBytesPerRow(GetParam().mFormat, copySize.width);
                    uint32_t rowsPerImage = copySize.height / blockHeight;

                    uint64_t bufferSize = utils::RequiredBytesInCopy(bytesPerRow, rowsPerImage,
                                                                     copySize, GetParam().mFormat);

                    std::vector<uint8_t> data(bufferSize, 100);
                    wgpu::Buffer bufferDst = utils::CreateBufferFromData(
                        device, data.data(), bufferSize, wgpu::BufferUsage::CopySrc);

                    wgpu::ImageCopyBuffer imageCopyBuffer =
                        utils::CreateImageCopyBuffer(bufferDst, 0, bytesPerRow, rowsPerImage);
                    wgpu::ImageCopyTexture imageCopyTexture =
                        utils::CreateImageCopyTexture(texture, mip, {0, 0, 0});

                    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
                    encoder.CopyTextureToBuffer(&imageCopyTexture, &imageCopyBuffer, &copySize);
                    wgpu::CommandBuffer commands = encoder.Finish();
                    queue.Submit(1, &commands);

                    uint32_t copiedWidthInBytes =
                        utils::GetTexelBlockSizeInBytes(GetParam().mFormat) * copySize.width /
                        blockWidth;
                    uint8_t* d = data.data();
                    for (uint32_t z = 0; z < depthOrArrayLayers; ++z) {
                        for (uint32_t row = 0; row < copySize.height / blockHeight; ++row) {
                            std::fill_n(d, copiedWidthInBytes, 1);
                            d += bytesPerRow;
                        }
                    }
                    EXPECT_BUFFER_U8_RANGE_EQ(data.data(), bufferDst, 0, bufferSize);
                    break;
                }
                default:
                    UNREACHABLE();
            }
        }
    };

    class NonzeroNonrenderableTextureCreationTests : public NonzeroTextureCreationTests {};
    class NonzeroCompressedTextureCreationTests : public NonzeroTextureCreationTests {};
    class NonzeroDepthTextureCreationTests : public NonzeroTextureCreationTests {};

}  // anonymous namespace

// Test that texture clears to a non-zero value because toggle is enabled.
TEST_P(NonzeroTextureCreationTests, TextureCreationClears) {
    Run();
}

// Test that texture clears to a non-zero value because toggle is enabled.
TEST_P(NonzeroNonrenderableTextureCreationTests, TextureCreationClears) {
    Run();
}

// Test that texture clears to a non-zero value because toggle is enabled.
TEST_P(NonzeroCompressedTextureCreationTests, TextureCreationClears) {
    Run();
}

// Test that texture clears to a non-zero value because toggle is enabled.
TEST_P(NonzeroDepthTextureCreationTests, TextureCreationClears) {
    Run();
}

// TODO(crbug.com/794): Test/implement texture initialization for multisampled textures.

DAWN_INSTANTIATE_TEST_P(
    NonzeroTextureCreationTests,
    {D3D12Backend({"nonzero_clear_resources_on_creation_for_testing"},
                  {"lazy_clear_resource_on_first_use"}),
     MetalBackend({"nonzero_clear_resources_on_creation_for_testing"},
                  {"lazy_clear_resource_on_first_use"}),
     OpenGLBackend({"nonzero_clear_resources_on_creation_for_testing"},
                   {"lazy_clear_resource_on_first_use"}),
     OpenGLESBackend({"nonzero_clear_resources_on_creation_for_testing"},
                     {"lazy_clear_resource_on_first_use"}),
     VulkanBackend({"nonzero_clear_resources_on_creation_for_testing"},
                   {"lazy_clear_resource_on_first_use"})},
    {wgpu::TextureFormat::R8Unorm, wgpu::TextureFormat::RG8Unorm, wgpu::TextureFormat::RGBA8Unorm},
    {wgpu::TextureUsage(wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc),
     wgpu::TextureUsage::CopySrc},
    {wgpu::TextureDimension::e2D, wgpu::TextureDimension::e3D},
    {1u, 7u},
    {0u, 1u, 2u, 3u});

DAWN_INSTANTIATE_TEST_P(NonzeroNonrenderableTextureCreationTests,
                        {D3D12Backend({"nonzero_clear_resources_on_creation_for_testing"},
                                      {"lazy_clear_resource_on_first_use"}),
                         MetalBackend({"nonzero_clear_resources_on_creation_for_testing"},
                                      {"lazy_clear_resource_on_first_use"}),
                         OpenGLBackend({"nonzero_clear_resources_on_creation_for_testing"},
                                       {"lazy_clear_resource_on_first_use"}),
                         OpenGLESBackend({"nonzero_clear_resources_on_creation_for_testing"},
                                         {"lazy_clear_resource_on_first_use"}),
                         VulkanBackend({"nonzero_clear_resources_on_creation_for_testing"},
                                       {"lazy_clear_resource_on_first_use"})},
                        {wgpu::TextureFormat::RGBA8Snorm},
                        {wgpu::TextureUsage::CopySrc},
                        {wgpu::TextureDimension::e2D, wgpu::TextureDimension::e3D},
                        {1u, 7u},
                        {0u, 1u, 2u, 3u});

DAWN_INSTANTIATE_TEST_P(NonzeroCompressedTextureCreationTests,
                        {D3D12Backend({"nonzero_clear_resources_on_creation_for_testing"},
                                      {"lazy_clear_resource_on_first_use"}),
                         MetalBackend({"nonzero_clear_resources_on_creation_for_testing"},
                                      {"lazy_clear_resource_on_first_use"}),
                         OpenGLBackend({"nonzero_clear_resources_on_creation_for_testing"},
                                       {"lazy_clear_resource_on_first_use"}),
                         OpenGLESBackend({"nonzero_clear_resources_on_creation_for_testing"},
                                         {"lazy_clear_resource_on_first_use"}),
                         VulkanBackend({"nonzero_clear_resources_on_creation_for_testing"},
                                       {"lazy_clear_resource_on_first_use"})},
                        {wgpu::TextureFormat::BC1RGBAUnorm},
                        {wgpu::TextureUsage::CopySrc},
                        {wgpu::TextureDimension::e2D},
                        {1u, 7u},
                        {0u, 1u, 2u, 3u});

DAWN_INSTANTIATE_TEST_P(NonzeroDepthTextureCreationTests,
                        {D3D12Backend({"nonzero_clear_resources_on_creation_for_testing"},
                                      {"lazy_clear_resource_on_first_use"}),
                         MetalBackend({"nonzero_clear_resources_on_creation_for_testing"},
                                      {"lazy_clear_resource_on_first_use"}),
                         OpenGLBackend({"nonzero_clear_resources_on_creation_for_testing"},
                                       {"lazy_clear_resource_on_first_use"}),
                         OpenGLESBackend({"nonzero_clear_resources_on_creation_for_testing"},
                                         {"lazy_clear_resource_on_first_use"}),
                         VulkanBackend({"nonzero_clear_resources_on_creation_for_testing"},
                                       {"lazy_clear_resource_on_first_use"})},
                        {wgpu::TextureFormat::Depth32Float},
                        {wgpu::TextureUsage(wgpu::TextureUsage::RenderAttachment |
                                            wgpu::TextureUsage::CopySrc),
                         wgpu::TextureUsage::CopySrc},
                        {wgpu::TextureDimension::e2D},
                        {1u, 7u},
                        {0u, 1u, 2u, 3u});
