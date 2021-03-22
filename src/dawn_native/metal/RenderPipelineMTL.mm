// Copyright 2017 The Dawn Authors
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

#include "dawn_native/metal/RenderPipelineMTL.h"

#include "common/VertexFormatUtils.h"
#include "dawn_native/metal/DeviceMTL.h"
#include "dawn_native/metal/PipelineLayoutMTL.h"
#include "dawn_native/metal/ShaderModuleMTL.h"
#include "dawn_native/metal/TextureMTL.h"
#include "dawn_native/metal/UtilsMetal.h"

namespace dawn_native { namespace metal {

    namespace {
        MTLVertexFormat VertexFormatType(wgpu::VertexFormat format) {
            switch (format) {
                case wgpu::VertexFormat::Uint8x2:
                    return MTLVertexFormatUChar2;
                case wgpu::VertexFormat::Uint8x4:
                    return MTLVertexFormatUChar4;
                case wgpu::VertexFormat::Sint8x2:
                    return MTLVertexFormatChar2;
                case wgpu::VertexFormat::Sint8x4:
                    return MTLVertexFormatChar4;
                case wgpu::VertexFormat::Unorm8x2:
                    return MTLVertexFormatUChar2Normalized;
                case wgpu::VertexFormat::Unorm8x4:
                    return MTLVertexFormatUChar4Normalized;
                case wgpu::VertexFormat::Snorm8x2:
                    return MTLVertexFormatChar2Normalized;
                case wgpu::VertexFormat::Snorm8x4:
                    return MTLVertexFormatChar4Normalized;
                case wgpu::VertexFormat::Uint16x2:
                    return MTLVertexFormatUShort2;
                case wgpu::VertexFormat::Uint16x4:
                    return MTLVertexFormatUShort4;
                case wgpu::VertexFormat::Sint16x2:
                    return MTLVertexFormatShort2;
                case wgpu::VertexFormat::Sint16x4:
                    return MTLVertexFormatShort4;
                case wgpu::VertexFormat::Unorm16x2:
                    return MTLVertexFormatUShort2Normalized;
                case wgpu::VertexFormat::Unorm16x4:
                    return MTLVertexFormatUShort4Normalized;
                case wgpu::VertexFormat::Snorm16x2:
                    return MTLVertexFormatShort2Normalized;
                case wgpu::VertexFormat::Snorm16x4:
                    return MTLVertexFormatShort4Normalized;
                case wgpu::VertexFormat::Float16x2:
                    return MTLVertexFormatHalf2;
                case wgpu::VertexFormat::Float16x4:
                    return MTLVertexFormatHalf4;
                case wgpu::VertexFormat::Float32:
                    return MTLVertexFormatFloat;
                case wgpu::VertexFormat::Float32x2:
                    return MTLVertexFormatFloat2;
                case wgpu::VertexFormat::Float32x3:
                    return MTLVertexFormatFloat3;
                case wgpu::VertexFormat::Float32x4:
                    return MTLVertexFormatFloat4;
                case wgpu::VertexFormat::Uint32:
                    return MTLVertexFormatUInt;
                case wgpu::VertexFormat::Uint32x2:
                    return MTLVertexFormatUInt2;
                case wgpu::VertexFormat::Uint32x3:
                    return MTLVertexFormatUInt3;
                case wgpu::VertexFormat::Uint32x4:
                    return MTLVertexFormatUInt4;
                case wgpu::VertexFormat::Sint32:
                    return MTLVertexFormatInt;
                case wgpu::VertexFormat::Sint32x2:
                    return MTLVertexFormatInt2;
                case wgpu::VertexFormat::Sint32x3:
                    return MTLVertexFormatInt3;
                case wgpu::VertexFormat::Sint32x4:
                    return MTLVertexFormatInt4;
                default:
                    UNREACHABLE();
            }
        }

        MTLVertexStepFunction InputStepModeFunction(wgpu::InputStepMode mode) {
            switch (mode) {
                case wgpu::InputStepMode::Vertex:
                    return MTLVertexStepFunctionPerVertex;
                case wgpu::InputStepMode::Instance:
                    return MTLVertexStepFunctionPerInstance;
            }
        }

        MTLPrimitiveType MTLPrimitiveTopology(wgpu::PrimitiveTopology primitiveTopology) {
            switch (primitiveTopology) {
                case wgpu::PrimitiveTopology::PointList:
                    return MTLPrimitiveTypePoint;
                case wgpu::PrimitiveTopology::LineList:
                    return MTLPrimitiveTypeLine;
                case wgpu::PrimitiveTopology::LineStrip:
                    return MTLPrimitiveTypeLineStrip;
                case wgpu::PrimitiveTopology::TriangleList:
                    return MTLPrimitiveTypeTriangle;
                case wgpu::PrimitiveTopology::TriangleStrip:
                    return MTLPrimitiveTypeTriangleStrip;
            }
        }

        MTLPrimitiveTopologyClass MTLInputPrimitiveTopology(
            wgpu::PrimitiveTopology primitiveTopology) {
            switch (primitiveTopology) {
                case wgpu::PrimitiveTopology::PointList:
                    return MTLPrimitiveTopologyClassPoint;
                case wgpu::PrimitiveTopology::LineList:
                case wgpu::PrimitiveTopology::LineStrip:
                    return MTLPrimitiveTopologyClassLine;
                case wgpu::PrimitiveTopology::TriangleList:
                case wgpu::PrimitiveTopology::TriangleStrip:
                    return MTLPrimitiveTopologyClassTriangle;
            }
        }

        MTLBlendFactor MetalBlendFactor(wgpu::BlendFactor factor, bool alpha) {
            switch (factor) {
                case wgpu::BlendFactor::Zero:
                    return MTLBlendFactorZero;
                case wgpu::BlendFactor::One:
                    return MTLBlendFactorOne;
                case wgpu::BlendFactor::SrcColor:
                    return MTLBlendFactorSourceColor;
                case wgpu::BlendFactor::OneMinusSrcColor:
                    return MTLBlendFactorOneMinusSourceColor;
                case wgpu::BlendFactor::SrcAlpha:
                    return MTLBlendFactorSourceAlpha;
                case wgpu::BlendFactor::OneMinusSrcAlpha:
                    return MTLBlendFactorOneMinusSourceAlpha;
                case wgpu::BlendFactor::DstColor:
                    return MTLBlendFactorDestinationColor;
                case wgpu::BlendFactor::OneMinusDstColor:
                    return MTLBlendFactorOneMinusDestinationColor;
                case wgpu::BlendFactor::DstAlpha:
                    return MTLBlendFactorDestinationAlpha;
                case wgpu::BlendFactor::OneMinusDstAlpha:
                    return MTLBlendFactorOneMinusDestinationAlpha;
                case wgpu::BlendFactor::SrcAlphaSaturated:
                    return MTLBlendFactorSourceAlphaSaturated;
                case wgpu::BlendFactor::BlendColor:
                    return alpha ? MTLBlendFactorBlendAlpha : MTLBlendFactorBlendColor;
                case wgpu::BlendFactor::OneMinusBlendColor:
                    return alpha ? MTLBlendFactorOneMinusBlendAlpha
                                 : MTLBlendFactorOneMinusBlendColor;
            }
        }

        MTLBlendOperation MetalBlendOperation(wgpu::BlendOperation operation) {
            switch (operation) {
                case wgpu::BlendOperation::Add:
                    return MTLBlendOperationAdd;
                case wgpu::BlendOperation::Subtract:
                    return MTLBlendOperationSubtract;
                case wgpu::BlendOperation::ReverseSubtract:
                    return MTLBlendOperationReverseSubtract;
                case wgpu::BlendOperation::Min:
                    return MTLBlendOperationMin;
                case wgpu::BlendOperation::Max:
                    return MTLBlendOperationMax;
            }
        }

        MTLColorWriteMask MetalColorWriteMask(wgpu::ColorWriteMask writeMask,
                                              bool isDeclaredInFragmentShader) {
            if (!isDeclaredInFragmentShader) {
                return MTLColorWriteMaskNone;
            }

            MTLColorWriteMask mask = MTLColorWriteMaskNone;

            if (writeMask & wgpu::ColorWriteMask::Red) {
                mask |= MTLColorWriteMaskRed;
            }
            if (writeMask & wgpu::ColorWriteMask::Green) {
                mask |= MTLColorWriteMaskGreen;
            }
            if (writeMask & wgpu::ColorWriteMask::Blue) {
                mask |= MTLColorWriteMaskBlue;
            }
            if (writeMask & wgpu::ColorWriteMask::Alpha) {
                mask |= MTLColorWriteMaskAlpha;
            }

            return mask;
        }

        void ComputeBlendDesc(MTLRenderPipelineColorAttachmentDescriptor* attachment,
                              const ColorTargetState* state,
                              bool isDeclaredInFragmentShader) {
            attachment.blendingEnabled = state->blend != nullptr;
            if (attachment.blendingEnabled) {
                attachment.sourceRGBBlendFactor =
                    MetalBlendFactor(state->blend->color.srcFactor, false);
                attachment.destinationRGBBlendFactor =
                    MetalBlendFactor(state->blend->color.dstFactor, false);
                attachment.rgbBlendOperation = MetalBlendOperation(state->blend->color.operation);
                attachment.sourceAlphaBlendFactor =
                    MetalBlendFactor(state->blend->alpha.srcFactor, true);
                attachment.destinationAlphaBlendFactor =
                    MetalBlendFactor(state->blend->alpha.dstFactor, true);
                attachment.alphaBlendOperation = MetalBlendOperation(state->blend->alpha.operation);
            }
            attachment.writeMask =
                MetalColorWriteMask(state->writeMask, isDeclaredInFragmentShader);
        }

        MTLStencilOperation MetalStencilOperation(wgpu::StencilOperation stencilOperation) {
            switch (stencilOperation) {
                case wgpu::StencilOperation::Keep:
                    return MTLStencilOperationKeep;
                case wgpu::StencilOperation::Zero:
                    return MTLStencilOperationZero;
                case wgpu::StencilOperation::Replace:
                    return MTLStencilOperationReplace;
                case wgpu::StencilOperation::Invert:
                    return MTLStencilOperationInvert;
                case wgpu::StencilOperation::IncrementClamp:
                    return MTLStencilOperationIncrementClamp;
                case wgpu::StencilOperation::DecrementClamp:
                    return MTLStencilOperationDecrementClamp;
                case wgpu::StencilOperation::IncrementWrap:
                    return MTLStencilOperationIncrementWrap;
                case wgpu::StencilOperation::DecrementWrap:
                    return MTLStencilOperationDecrementWrap;
            }
        }

        NSRef<MTLDepthStencilDescriptor> MakeDepthStencilDesc(const DepthStencilState* descriptor) {
            NSRef<MTLDepthStencilDescriptor> mtlDepthStencilDescRef =
                AcquireNSRef([MTLDepthStencilDescriptor new]);
            MTLDepthStencilDescriptor* mtlDepthStencilDescriptor = mtlDepthStencilDescRef.Get();

            mtlDepthStencilDescriptor.depthCompareFunction =
                ToMetalCompareFunction(descriptor->depthCompare);
            mtlDepthStencilDescriptor.depthWriteEnabled = descriptor->depthWriteEnabled;

            if (StencilTestEnabled(descriptor)) {
                NSRef<MTLStencilDescriptor> backFaceStencilRef =
                    AcquireNSRef([MTLStencilDescriptor new]);
                MTLStencilDescriptor* backFaceStencil = backFaceStencilRef.Get();
                NSRef<MTLStencilDescriptor> frontFaceStencilRef =
                    AcquireNSRef([MTLStencilDescriptor new]);
                MTLStencilDescriptor* frontFaceStencil = frontFaceStencilRef.Get();

                backFaceStencil.stencilCompareFunction =
                    ToMetalCompareFunction(descriptor->stencilBack.compare);
                backFaceStencil.stencilFailureOperation =
                    MetalStencilOperation(descriptor->stencilBack.failOp);
                backFaceStencil.depthFailureOperation =
                    MetalStencilOperation(descriptor->stencilBack.depthFailOp);
                backFaceStencil.depthStencilPassOperation =
                    MetalStencilOperation(descriptor->stencilBack.passOp);
                backFaceStencil.readMask = descriptor->stencilReadMask;
                backFaceStencil.writeMask = descriptor->stencilWriteMask;

                frontFaceStencil.stencilCompareFunction =
                    ToMetalCompareFunction(descriptor->stencilFront.compare);
                frontFaceStencil.stencilFailureOperation =
                    MetalStencilOperation(descriptor->stencilFront.failOp);
                frontFaceStencil.depthFailureOperation =
                    MetalStencilOperation(descriptor->stencilFront.depthFailOp);
                frontFaceStencil.depthStencilPassOperation =
                    MetalStencilOperation(descriptor->stencilFront.passOp);
                frontFaceStencil.readMask = descriptor->stencilReadMask;
                frontFaceStencil.writeMask = descriptor->stencilWriteMask;

                mtlDepthStencilDescriptor.backFaceStencil = backFaceStencil;
                mtlDepthStencilDescriptor.frontFaceStencil = frontFaceStencil;
            }

            return mtlDepthStencilDescRef;
        }

        MTLWinding MTLFrontFace(wgpu::FrontFace face) {
            switch (face) {
                case wgpu::FrontFace::CW:
                    return MTLWindingClockwise;
                case wgpu::FrontFace::CCW:
                    return MTLWindingCounterClockwise;
            }
        }

        MTLCullMode ToMTLCullMode(wgpu::CullMode mode) {
            switch (mode) {
                case wgpu::CullMode::None:
                    return MTLCullModeNone;
                case wgpu::CullMode::Front:
                    return MTLCullModeFront;
                case wgpu::CullMode::Back:
                    return MTLCullModeBack;
            }
        }

    }  // anonymous namespace

    // static
    ResultOrError<RenderPipeline*> RenderPipeline::Create(
        Device* device,
        const RenderPipelineDescriptor* descriptor) {
        Ref<RenderPipeline> pipeline = AcquireRef(new RenderPipeline(device, descriptor));
        DAWN_TRY(pipeline->Initialize(descriptor));
        return pipeline.Detach();
    }

    MaybeError RenderPipeline::Initialize(const RenderPipelineDescriptor* descriptor) {
        mMtlPrimitiveTopology = MTLPrimitiveTopology(GetPrimitiveTopology());
        mMtlFrontFace = MTLFrontFace(GetFrontFace());
        mMtlCullMode = ToMTLCullMode(GetCullMode());
        auto mtlDevice = ToBackend(GetDevice())->GetMTLDevice();

        NSRef<MTLRenderPipelineDescriptor> descriptorMTLRef =
            AcquireNSRef([MTLRenderPipelineDescriptor new]);
        MTLRenderPipelineDescriptor* descriptorMTL = descriptorMTLRef.Get();

        // TODO: MakeVertexDesc should be const in the future, so we don't need to call it here when
        // vertex pulling is enabled
        NSRef<MTLVertexDescriptor> vertexDesc = MakeVertexDesc();

        // Calling MakeVertexDesc first is important since it sets indices for packed bindings
        if (GetDevice()->IsToggleEnabled(Toggle::MetalEnableVertexPulling)) {
            vertexDesc = AcquireNSRef([MTLVertexDescriptor new]);
        }
        descriptorMTL.vertexDescriptor = vertexDesc.Get();

        ShaderModule* vertexModule = ToBackend(descriptor->vertexStage.module);
        const char* vertexEntryPoint = descriptor->vertexStage.entryPoint;
        ShaderModule::MetalFunctionData vertexData;

        const VertexStateDescriptor* vertexStatePtr = descriptor->vertexState;
        VertexStateDescriptor vertexState;
        if (vertexStatePtr == nullptr) {
            vertexState = {};
            vertexStatePtr = &vertexState;
        }

        DAWN_TRY(vertexModule->CreateFunction(vertexEntryPoint, SingleShaderStage::Vertex,
                                              ToBackend(GetLayout()), &vertexData, 0xFFFFFFFF, this,
                                              vertexStatePtr));

        descriptorMTL.vertexFunction = vertexData.function.Get();
        if (vertexData.needsStorageBufferLength) {
            mStagesRequiringStorageBufferLength |= wgpu::ShaderStage::Vertex;
        }

        ShaderModule* fragmentModule = ToBackend(descriptor->fragmentStage->module);
        const char* fragmentEntryPoint = descriptor->fragmentStage->entryPoint;
        ShaderModule::MetalFunctionData fragmentData;
        DAWN_TRY(fragmentModule->CreateFunction(fragmentEntryPoint, SingleShaderStage::Fragment,
                                                ToBackend(GetLayout()), &fragmentData,
                                                descriptor->sampleMask));

        descriptorMTL.fragmentFunction = fragmentData.function.Get();
        if (fragmentData.needsStorageBufferLength) {
            mStagesRequiringStorageBufferLength |= wgpu::ShaderStage::Fragment;
        }

        if (HasDepthStencilAttachment()) {
            wgpu::TextureFormat depthStencilFormat = GetDepthStencilFormat();
            const Format& internalFormat = GetDevice()->GetValidInternalFormat(depthStencilFormat);
            MTLPixelFormat metalFormat = MetalPixelFormat(depthStencilFormat);

            if (internalFormat.HasDepth()) {
                descriptorMTL.depthAttachmentPixelFormat = metalFormat;
            }
            if (internalFormat.HasStencil()) {
                descriptorMTL.stencilAttachmentPixelFormat = metalFormat;
            }
        }

        const auto& fragmentOutputsWritten =
            GetStage(SingleShaderStage::Fragment).metadata->fragmentOutputsWritten;
        for (ColorAttachmentIndex i : IterateBitSet(GetColorAttachmentsMask())) {
            descriptorMTL.colorAttachments[static_cast<uint8_t>(i)].pixelFormat =
                MetalPixelFormat(GetColorAttachmentFormat(i));
            const ColorTargetState* descriptor = GetColorTargetState(i);
            ComputeBlendDesc(descriptorMTL.colorAttachments[static_cast<uint8_t>(i)], descriptor,
                             fragmentOutputsWritten[i]);
        }

        descriptorMTL.inputPrimitiveTopology = MTLInputPrimitiveTopology(GetPrimitiveTopology());
        descriptorMTL.sampleCount = GetSampleCount();
        descriptorMTL.alphaToCoverageEnabled = descriptor->alphaToCoverageEnabled;

        {
            NSError* error = nullptr;
            mMtlRenderPipelineState =
                AcquireNSPRef([mtlDevice newRenderPipelineStateWithDescriptor:descriptorMTL
                                                                        error:&error]);
            if (error != nullptr) {
                NSLog(@" error => %@", error);
                return DAWN_INTERNAL_ERROR("Error creating rendering pipeline state");
            }
        }

        // Create depth stencil state and cache it, fetch the cached depth stencil state when we
        // call setDepthStencilState() for a given render pipeline in CommandEncoder, in order to
        // improve performance.
        NSRef<MTLDepthStencilDescriptor> depthStencilDesc =
            MakeDepthStencilDesc(GetDepthStencilState());
        mMtlDepthStencilState =
            AcquireNSPRef([mtlDevice newDepthStencilStateWithDescriptor:depthStencilDesc.Get()]);

        return {};
    }

    MTLPrimitiveType RenderPipeline::GetMTLPrimitiveTopology() const {
        return mMtlPrimitiveTopology;
    }

    MTLWinding RenderPipeline::GetMTLFrontFace() const {
        return mMtlFrontFace;
    }

    MTLCullMode RenderPipeline::GetMTLCullMode() const {
        return mMtlCullMode;
    }

    void RenderPipeline::Encode(id<MTLRenderCommandEncoder> encoder) {
        [encoder setRenderPipelineState:mMtlRenderPipelineState.Get()];
    }

    id<MTLDepthStencilState> RenderPipeline::GetMTLDepthStencilState() {
        return mMtlDepthStencilState.Get();
    }

    uint32_t RenderPipeline::GetMtlVertexBufferIndex(VertexBufferSlot slot) const {
        ASSERT(slot < kMaxVertexBuffersTyped);
        return mMtlVertexBufferIndices[slot];
    }

    wgpu::ShaderStage RenderPipeline::GetStagesRequiringStorageBufferLength() const {
        return mStagesRequiringStorageBufferLength;
    }

    MTLVertexDescriptor* RenderPipeline::MakeVertexDesc() {
        MTLVertexDescriptor* mtlVertexDescriptor = [MTLVertexDescriptor new];

        // Vertex buffers are packed after all the buffers for the bind groups.
        uint32_t mtlVertexBufferIndex =
            ToBackend(GetLayout())->GetBufferBindingCount(SingleShaderStage::Vertex);

        for (VertexBufferSlot slot : IterateBitSet(GetVertexBufferSlotsUsed())) {
            const VertexBufferInfo& info = GetVertexBuffer(slot);

            MTLVertexBufferLayoutDescriptor* layoutDesc = [MTLVertexBufferLayoutDescriptor new];
            if (info.arrayStride == 0) {
                // For MTLVertexStepFunctionConstant, the stepRate must be 0,
                // but the arrayStride must NOT be 0, so we made up it with
                // max(attrib.offset + sizeof(attrib) for each attrib)
                size_t maxArrayStride = 0;
                for (VertexAttributeLocation loc : IterateBitSet(GetAttributeLocationsUsed())) {
                    const VertexAttributeInfo& attrib = GetAttribute(loc);
                    // Only use the attributes that use the current input
                    if (attrib.vertexBufferSlot != slot) {
                        continue;
                    }
                    maxArrayStride =
                        std::max(maxArrayStride,
                                 dawn::VertexFormatSize(attrib.format) + size_t(attrib.offset));
                }
                layoutDesc.stepFunction = MTLVertexStepFunctionConstant;
                layoutDesc.stepRate = 0;
                // Metal requires the stride must be a multiple of 4 bytes, align it with next
                // multiple of 4 if it's not.
                layoutDesc.stride = Align(maxArrayStride, 4);
            } else {
                layoutDesc.stepFunction = InputStepModeFunction(info.stepMode);
                layoutDesc.stepRate = 1;
                layoutDesc.stride = info.arrayStride;
            }

            mtlVertexDescriptor.layouts[mtlVertexBufferIndex] = layoutDesc;
            [layoutDesc release];

            mMtlVertexBufferIndices[slot] = mtlVertexBufferIndex;
            mtlVertexBufferIndex++;
        }

        for (VertexAttributeLocation loc : IterateBitSet(GetAttributeLocationsUsed())) {
            const VertexAttributeInfo& info = GetAttribute(loc);

            auto attribDesc = [MTLVertexAttributeDescriptor new];
            attribDesc.format = VertexFormatType(info.format);
            attribDesc.offset = info.offset;
            attribDesc.bufferIndex = mMtlVertexBufferIndices[info.vertexBufferSlot];
            mtlVertexDescriptor.attributes[static_cast<uint8_t>(loc)] = attribDesc;
            [attribDesc release];
        }

        return mtlVertexDescriptor;
    }

}}  // namespace dawn_native::metal
