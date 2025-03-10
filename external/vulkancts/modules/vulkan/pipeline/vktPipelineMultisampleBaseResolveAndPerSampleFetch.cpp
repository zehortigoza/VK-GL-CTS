/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*
* \file vktPipelineMultisampleBaseResolveAndPerSampleFetch.cpp
* \brief Base class for tests that check results of multisample resolve
*          and/or values of individual samples
*//*--------------------------------------------------------------------*/

#include "vktPipelineMultisampleBaseResolveAndPerSampleFetch.hpp"
#include "vktPipelineMakeUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "tcuTestLog.hpp"
#include <vector>

namespace vkt
{
namespace pipeline
{
namespace multisample
{

using namespace vk;

void MSCaseBaseResolveAndPerSampleFetch::initPrograms(vk::SourceCollections &programCollection) const
{
    // Create vertex shader
    std::ostringstream vs;

    vs << "#version 440\n"
       << "layout(location = 0) in vec4 vs_in_position_ndc;\n"
       << "\n"
       << "out gl_PerVertex {\n"
       << "    vec4  gl_Position;\n"
       << "};\n"
       << "void main (void)\n"
       << "{\n"
       << "    gl_Position = vs_in_position_ndc;\n"
       << "}\n";

    programCollection.glslSources.add("per_sample_fetch_vs") << glu::VertexSource(vs.str());

    // Create fragment shader
    std::ostringstream fs;

    fs << "#version 440\n"
       << "\n"
       << "layout(location = 0) out vec4 fs_out_color;\n"
       << "\n"
       << "layout(set = 0, binding = 0, input_attachment_index = 0) uniform subpassInputMS imageMS;\n"
       << "\n"
       << "layout(set = 0, binding = 1, std140) uniform SampleBlock {\n"
       << "    int sampleNdx;\n"
       << "};\n"
       << "void main (void)\n"
       << "{\n"
       << "    fs_out_color = subpassLoad(imageMS, sampleNdx);\n"
       << "}\n";

    programCollection.glslSources.add("per_sample_fetch_fs") << glu::FragmentSource(fs.str());
}

MSInstanceBaseResolveAndPerSampleFetch::MSInstanceBaseResolveAndPerSampleFetch(Context &context,
                                                                               const ImageMSParams &imageMSParams)
    : MultisampleInstanceBase(context, imageMSParams)
{
}

VkPipelineMultisampleStateCreateInfo MSInstanceBaseResolveAndPerSampleFetch::getMSStateCreateInfo(
    const ImageMSParams &imageMSParams) const
{
    const VkPipelineMultisampleStateCreateInfo multisampleStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                                  // const void* pNext;
        (VkPipelineMultisampleStateCreateFlags)0u,                // VkPipelineMultisampleStateCreateFlags flags;
        imageMSParams.numSamples,                                 // VkSampleCountFlagBits rasterizationSamples;
        VK_TRUE,                                                  // VkBool32 sampleShadingEnable;
        imageMSParams.shadingRate,                                // float minSampleShading;
        DE_NULL,                                                  // const VkSampleMask* pSampleMask;
        VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
        VK_FALSE,                                                 // VkBool32 alphaToOneEnable;
    };

    return multisampleStateInfo;
}

const VkDescriptorSetLayout *MSInstanceBaseResolveAndPerSampleFetch::createMSPassDescSetLayout(
    const ImageMSParams &imageMSParams)
{
    DE_UNREF(imageMSParams);

    return DE_NULL;
}

const VkDescriptorSet *MSInstanceBaseResolveAndPerSampleFetch::createMSPassDescSet(
    const ImageMSParams &imageMSParams, const VkDescriptorSetLayout *descSetLayout)
{
    DE_UNREF(imageMSParams);
    DE_UNREF(descSetLayout);

    return DE_NULL;
}

tcu::TestStatus MSInstanceBaseResolveAndPerSampleFetch::iterate(void)
{
    const InstanceInterface &instance      = m_context.getInstanceInterface();
    const DeviceInterface &deviceInterface = m_context.getDeviceInterface();
    const VkDevice device                  = m_context.getDevice();
    const VkPhysicalDevice physicalDevice  = m_context.getPhysicalDevice();
    Allocator &allocator                   = m_context.getDefaultAllocator();
    const VkQueue queue                    = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex        = m_context.getUniversalQueueFamilyIndex();

    VkImageCreateInfo imageMSInfo;
    VkImageCreateInfo imageRSInfo;
    const uint32_t firstSubpassAttachmentsCount = 2u;

    // Check if image size does not exceed device limits
    validateImageSize(instance, physicalDevice, m_imageType, m_imageMSParams.imageSize);

    // Check if device supports image format as color attachment
    validateImageFeatureFlags(instance, physicalDevice, mapTextureFormat(m_imageFormat),
                              VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);

    imageMSInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageMSInfo.pNext                 = DE_NULL;
    imageMSInfo.flags                 = 0u;
    imageMSInfo.imageType             = mapImageType(m_imageType);
    imageMSInfo.format                = mapTextureFormat(m_imageFormat);
    imageMSInfo.extent                = makeExtent3D(getLayerSize(m_imageType, m_imageMSParams.imageSize));
    imageMSInfo.arrayLayers           = getNumLayers(m_imageType, m_imageMSParams.imageSize);
    imageMSInfo.mipLevels             = 1u;
    imageMSInfo.samples               = m_imageMSParams.numSamples;
    imageMSInfo.tiling                = VK_IMAGE_TILING_OPTIMAL;
    imageMSInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMSInfo.usage                 = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    imageMSInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    imageMSInfo.queueFamilyIndexCount = 0u;
    imageMSInfo.pQueueFamilyIndices   = DE_NULL;

    if (m_imageType == IMAGE_TYPE_CUBE || m_imageType == IMAGE_TYPE_CUBE_ARRAY)
    {
        imageMSInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    validateImageInfo(instance, physicalDevice, imageMSInfo);

    const de::UniquePtr<ImageWithMemory> imageMS(
        new ImageWithMemory(deviceInterface, device, allocator, imageMSInfo, MemoryRequirement::Any));

    imageRSInfo         = imageMSInfo;
    imageRSInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageRSInfo.usage   = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    validateImageInfo(instance, physicalDevice, imageRSInfo);

    const de::UniquePtr<ImageWithMemory> imageRS(
        new ImageWithMemory(deviceInterface, device, allocator, imageRSInfo, MemoryRequirement::Any));

    const uint32_t numSamples = static_cast<uint32_t>(imageMSInfo.samples);

    std::vector<de::SharedPtr<ImageWithMemory>> imagesPerSampleVec(numSamples);

    for (uint32_t sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
    {
        imagesPerSampleVec[sampleNdx] = de::SharedPtr<ImageWithMemory>(
            new ImageWithMemory(deviceInterface, device, allocator, imageRSInfo, MemoryRequirement::Any));
    }

    // Create render pass
    std::vector<VkAttachmentDescription> attachments(firstSubpassAttachmentsCount + numSamples);

    {
        const VkAttachmentDescription attachmentMSDesc = {
            (VkAttachmentDescriptionFlags)0u,         // VkAttachmentDescriptionFlags flags;
            imageMSInfo.format,                       // VkFormat format;
            imageMSInfo.samples,                      // VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_CLEAR,              // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp stencilStoreOp;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL  // VkImageLayout finalLayout;
        };

        attachments[0] = attachmentMSDesc;

        const VkAttachmentDescription attachmentRSDesc = {
            (VkAttachmentDescriptionFlags)0u,         // VkAttachmentDescriptionFlags flags;
            imageRSInfo.format,                       // VkFormat format;
            imageRSInfo.samples,                      // VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_CLEAR,              // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp stencilStoreOp;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL  // VkImageLayout finalLayout;
        };

        attachments[1] = attachmentRSDesc;

        for (uint32_t sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
        {
            attachments[firstSubpassAttachmentsCount + sampleNdx] = attachmentRSDesc;
        }
    }

    const VkAttachmentReference attachmentMSColorRef = {
        0u,                                      // uint32_t attachment;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout layout;
    };

    const VkAttachmentReference attachmentMSInputRef = {
        0u,                                      // uint32_t attachment;
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL // VkImageLayout layout;
    };

    const VkAttachmentReference attachmentRSColorRef = {
        1u,                                      // uint32_t attachment;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout layout;
    };

    std::vector<VkAttachmentReference> perSampleAttachmentRef(numSamples);

    for (uint32_t sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
    {
        const VkAttachmentReference attachmentRef = {
            firstSubpassAttachmentsCount + sampleNdx, // uint32_t attachment;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL  // VkImageLayout layout;
        };

        perSampleAttachmentRef[sampleNdx] = attachmentRef;
    }

    std::vector<uint32_t> preserveAttachments(1u + numSamples);

    for (uint32_t attachNdx = 0u; attachNdx < 1u + numSamples; ++attachNdx)
    {
        preserveAttachments[attachNdx] = 1u + attachNdx;
    }

    std::vector<VkSubpassDescription> subpasses(1u + numSamples);
    std::vector<VkSubpassDependency> subpassDependencies;

    const VkSubpassDescription firstSubpassDesc = {
        (VkSubpassDescriptionFlags)0u,   // VkSubpassDescriptionFlags flags;
        VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint pipelineBindPoint;
        0u,                              // uint32_t inputAttachmentCount;
        DE_NULL,                         // const VkAttachmentReference* pInputAttachments;
        1u,                              // uint32_t colorAttachmentCount;
        &attachmentMSColorRef,           // const VkAttachmentReference* pColorAttachments;
        &attachmentRSColorRef,           // const VkAttachmentReference* pResolveAttachments;
        DE_NULL,                         // const VkAttachmentReference* pDepthStencilAttachment;
        0u,                              // uint32_t preserveAttachmentCount;
        DE_NULL                          // const uint32_t* pPreserveAttachments;
    };

    subpasses[0] = firstSubpassDesc;

    for (uint32_t sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
    {
        const VkSubpassDescription subpassDesc = {
            (VkSubpassDescriptionFlags)0u,      // VkSubpassDescriptionFlags flags;
            VK_PIPELINE_BIND_POINT_GRAPHICS,    // VkPipelineBindPoint pipelineBindPoint;
            1u,                                 // uint32_t inputAttachmentCount;
            &attachmentMSInputRef,              // const VkAttachmentReference* pInputAttachments;
            1u,                                 // uint32_t colorAttachmentCount;
            &perSampleAttachmentRef[sampleNdx], // const VkAttachmentReference* pColorAttachments;
            DE_NULL,                            // const VkAttachmentReference* pResolveAttachments;
            DE_NULL,                            // const VkAttachmentReference* pDepthStencilAttachment;
            1u + sampleNdx,                     // uint32_t preserveAttachmentCount;
            dataPointer(preserveAttachments)    // const uint32_t* pPreserveAttachments;
        };

        subpasses[1u + sampleNdx] = subpassDesc;

        if (sampleNdx == 0u)
        {
            // The second subpass will be in charge of transitioning the multisample attachment from
            // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
            const VkSubpassDependency subpassDependency = {
                0u,                                            // uint32_t                srcSubpass;
                1u + sampleNdx,                                // uint32_t                dstSubpass;
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // VkPipelineStageFlags    srcStageMask;
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,         // VkPipelineStageFlags    dstStageMask;
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // VkAccessFlags           srcAccessMask;
                VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,           // VkAccessFlags           dstAccessMask;
                0u,                                            // VkDependencyFlags       dependencyFlags;
            };

            subpassDependencies.push_back(subpassDependency);
        }
        else
        {
            // Make sure subpass reads are in order. This serializes subpasses to make sure there are no layout transition hazards
            // in the multisample image, from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            // caused by parallel execution of several subpasses.
            const VkSubpassDependency readDependency = {
                sampleNdx,                             // uint32_t                srcSubpass;
                1u + sampleNdx,                        // uint32_t                dstSubpass;
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // VkPipelineStageFlags    srcStageMask;
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // VkPipelineStageFlags    dstStageMask;
                VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,   // VkAccessFlags           srcAccessMask;
                VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,   // VkAccessFlags           dstAccessMask;
                0u,                                    // VkDependencyFlags       dependencyFlags;
            };

            subpassDependencies.push_back(readDependency);
        }
    }

    const VkRenderPassCreateInfo renderPassInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,         // VkStructureType sType;
        DE_NULL,                                           // const void* pNext;
        (VkRenderPassCreateFlags)0u,                       // VkRenderPassCreateFlags flags;
        static_cast<uint32_t>(attachments.size()),         // uint32_t attachmentCount;
        dataPointer(attachments),                          // const VkAttachmentDescription* pAttachments;
        static_cast<uint32_t>(subpasses.size()),           // uint32_t subpassCount;
        dataPointer(subpasses),                            // const VkSubpassDescription* pSubpasses;
        static_cast<uint32_t>(subpassDependencies.size()), // uint32_t dependencyCount;
        dataPointer(subpassDependencies)                   // const VkSubpassDependency* pDependencies;
    };

    RenderPassWrapper renderPass(m_imageMSParams.pipelineConstructionType, deviceInterface, device, &renderPassInfo);

    const VkImageSubresourceRange fullImageRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, imageMSInfo.mipLevels, 0u, imageMSInfo.arrayLayers);

    // Create color attachments image views
    typedef de::SharedPtr<Unique<VkImageView>> VkImageViewSp;
    std::vector<VkImageViewSp> imageViewsShPtrs(firstSubpassAttachmentsCount + numSamples);
    std::vector<VkImage> images(firstSubpassAttachmentsCount + numSamples);
    std::vector<VkImageView> imageViews(firstSubpassAttachmentsCount + numSamples);

    images[0] = **imageMS;
    images[1] = **imageRS;

    imageViewsShPtrs[0] = makeVkSharedPtr(makeImageView(
        deviceInterface, device, **imageMS, mapImageViewType(m_imageType), imageMSInfo.format, fullImageRange));
    imageViewsShPtrs[1] = makeVkSharedPtr(makeImageView(
        deviceInterface, device, **imageRS, mapImageViewType(m_imageType), imageRSInfo.format, fullImageRange));

    imageViews[0] = **imageViewsShPtrs[0];
    imageViews[1] = **imageViewsShPtrs[1];

    for (uint32_t sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
    {
        images[firstSubpassAttachmentsCount + sampleNdx] = **imagesPerSampleVec[sampleNdx];
        imageViewsShPtrs[firstSubpassAttachmentsCount + sampleNdx] =
            makeVkSharedPtr(makeImageView(deviceInterface, device, **imagesPerSampleVec[sampleNdx],
                                          mapImageViewType(m_imageType), imageRSInfo.format, fullImageRange));
        imageViews[firstSubpassAttachmentsCount + sampleNdx] =
            **imageViewsShPtrs[firstSubpassAttachmentsCount + sampleNdx];
    }

    // Create framebuffer
    const VkFramebufferCreateInfo framebufferInfo = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType    sType;
        DE_NULL,                                   // const void*                                 pNext;
        (VkFramebufferCreateFlags)0u,              // VkFramebufferCreateFlags                    flags;
        *renderPass,                               // VkRenderPass                                renderPass;
        static_cast<uint32_t>(imageViews.size()),  // uint32_t                                    attachmentCount;
        dataPointer(imageViews),                   // const VkImageView*                          pAttachments;
        imageMSInfo.extent.width,                  // uint32_t                                    width;
        imageMSInfo.extent.height,                 // uint32_t                                    height;
        imageMSInfo.arrayLayers,                   // uint32_t                                    layers;
    };

    renderPass.createFramebuffer(deviceInterface, device, &framebufferInfo, images);

    const VkDescriptorSetLayout *descriptorSetLayoutMSPass = createMSPassDescSetLayout(m_imageMSParams);

    // Create pipeline layout
    const VkPipelineLayoutCreateInfo pipelineLayoutMSPassParams = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                       // const void* pNext;
        (VkPipelineLayoutCreateFlags)0u,               // VkPipelineLayoutCreateFlags flags;
        descriptorSetLayoutMSPass ? 1u : 0u,           // uint32_t setLayoutCount;
        descriptorSetLayoutMSPass,                     // const VkDescriptorSetLayout* pSetLayouts;
        0u,                                            // uint32_t pushConstantRangeCount;
        DE_NULL,                                       // const VkPushConstantRange* pPushConstantRanges;
    };

    const PipelineLayoutWrapper pipelineLayoutMSPass(m_imageMSParams.pipelineConstructionType, deviceInterface, device,
                                                     &pipelineLayoutMSPassParams);

    // Create vertex attributes data
    const VertexDataDesc vertexDataDesc = getVertexDataDescripton();

    de::SharedPtr<BufferWithMemory> vertexBuffer = de::SharedPtr<BufferWithMemory>(
        new BufferWithMemory(deviceInterface, device, allocator,
                             makeBufferCreateInfo(vertexDataDesc.dataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
                             MemoryRequirement::HostVisible));
    const Allocation &vertexBufferAllocation = vertexBuffer->getAllocation();

    uploadVertexData(vertexBufferAllocation, vertexDataDesc);

    flushAlloc(deviceInterface, device, vertexBufferAllocation);

    const VkVertexInputBindingDescription vertexBinding = {
        0u,                         // uint32_t binding;
        vertexDataDesc.dataStride,  // uint32_t stride;
        VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputRate inputRate;
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType;
        DE_NULL,                                                   // const void*                                 pNext;
        (VkPipelineVertexInputStateCreateFlags)0u,                 // VkPipelineVertexInputStateCreateFlags       flags;
        1u,             // uint32_t                                    vertexBindingDescriptionCount;
        &vertexBinding, // const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
        static_cast<uint32_t>(
            vertexDataDesc.vertexAttribDescVec
                .size()), // uint32_t                                    vertexAttributeDescriptionCount;
        dataPointer(
            vertexDataDesc
                .vertexAttribDescVec), // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
    };

    const std::vector<VkViewport> viewports{makeViewport(imageMSInfo.extent)};
    const std::vector<VkRect2D> scissors{makeRect2D(imageMSInfo.extent)};

    const VkPipelineMultisampleStateCreateInfo multisampleStateInfo = getMSStateCreateInfo(m_imageMSParams);

    // Create graphics pipeline for multisample pass
    const ShaderWrapper vsMSPassModule(ShaderWrapper(
        deviceInterface, device, m_context.getBinaryCollection().get("vertex_shader"), (VkShaderModuleCreateFlags)0u));
    const ShaderWrapper fsMSPassModule(ShaderWrapper(deviceInterface, device,
                                                     m_context.getBinaryCollection().get("fragment_shader"),
                                                     (VkShaderModuleCreateFlags)0u));

    GraphicsPipelineWrapper graphicsPipelineMSPass(instance, deviceInterface, physicalDevice, device,
                                                   m_context.getDeviceExtensions(),
                                                   m_imageMSParams.pipelineConstructionType);
    graphicsPipelineMSPass.setDefaultColorBlendState()
        .setDefaultDepthStencilState()
        .setDefaultRasterizationState()
        .setDefaultTopology(vertexDataDesc.primitiveTopology)
        .setupVertexInputState(&vertexInputStateInfo)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayoutMSPass, *renderPass, 0u, vsMSPassModule)
        .setupFragmentShaderState(pipelineLayoutMSPass, *renderPass, 0u, fsMSPassModule, DE_NULL, &multisampleStateInfo)
        .setupFragmentOutputState(*renderPass, 0u, DE_NULL, &multisampleStateInfo)
        .setMonolithicPipelineLayout(pipelineLayoutMSPass)
        .buildPipeline();

    std::vector<GraphicsPipelineWrapper> graphicsPipelinesPerSampleFetch;
    graphicsPipelinesPerSampleFetch.reserve(numSamples);

    // Create descriptor set layout
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(deviceInterface, device));

    const PipelineLayoutWrapper pipelineLayoutPerSampleFetchPass(m_imageMSParams.pipelineConstructionType,
                                                                 deviceInterface, device, *descriptorSetLayout);

    const uint32_t bufferPerSampleFetchPassSize = 4u * (uint32_t)sizeof(tcu::Vec4);

    de::SharedPtr<BufferWithMemory> vertexBufferPerSampleFetchPass = de::SharedPtr<BufferWithMemory>(
        new BufferWithMemory(deviceInterface, device, allocator,
                             makeBufferCreateInfo(bufferPerSampleFetchPassSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
                             MemoryRequirement::HostVisible));

    // Create graphics pipelines for per sample texel fetch passes
    {
        const ShaderWrapper vsPerSampleFetchPassModule(
            ShaderWrapper(deviceInterface, device, m_context.getBinaryCollection().get("per_sample_fetch_vs"),
                          (VkShaderModuleCreateFlags)0u));
        const ShaderWrapper fsPerSampleFetchPassModule(
            ShaderWrapper(deviceInterface, device, m_context.getBinaryCollection().get("per_sample_fetch_fs"),
                          (VkShaderModuleCreateFlags)0u));

        std::vector<tcu::Vec4> vertices;

        vertices.push_back(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
        vertices.push_back(tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f));
        vertices.push_back(tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f));
        vertices.push_back(tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f));

        const Allocation &vertexAllocPerSampleFetchPass = vertexBufferPerSampleFetchPass->getAllocation();

        deMemcpy(vertexAllocPerSampleFetchPass.getHostPtr(), dataPointer(vertices),
                 static_cast<std::size_t>(bufferPerSampleFetchPassSize));

        flushAlloc(deviceInterface, device, vertexAllocPerSampleFetchPass);

        for (uint32_t sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
        {
            const uint32_t subpass = 1u + sampleNdx;
            graphicsPipelinesPerSampleFetch.emplace_back(instance, deviceInterface, physicalDevice, device,
                                                         m_context.getDeviceExtensions(),
                                                         m_imageMSParams.pipelineConstructionType);
            graphicsPipelinesPerSampleFetch.back()
                .setDefaultMultisampleState()
                .setDefaultColorBlendState()
                .setDefaultDepthStencilState()
                .setDefaultRasterizationState()
                .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
                .setupVertexInputState()
                .setupPreRasterizationShaderState(viewports, scissors, pipelineLayoutPerSampleFetchPass, *renderPass,
                                                  subpass, vsPerSampleFetchPassModule)
                .setupFragmentShaderState(pipelineLayoutPerSampleFetchPass, *renderPass, subpass,
                                          fsPerSampleFetchPassModule)
                .setupFragmentOutputState(*renderPass, subpass)
                .setMonolithicPipelineLayout(pipelineLayoutPerSampleFetchPass)
                .buildPipeline();
        }
    }

    // Create descriptor pool
    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1u)
            .addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1u)
            .build(deviceInterface, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    // Create descriptor set
    const Unique<VkDescriptorSet> descriptorSet(
        makeDescriptorSet(deviceInterface, device, *descriptorPool, *descriptorSetLayout));

    const VkPhysicalDeviceLimits deviceLimits = getPhysicalDeviceProperties(instance, physicalDevice).limits;

    VkDeviceSize uboOffsetAlignment = sizeof(int32_t) < deviceLimits.minUniformBufferOffsetAlignment ?
                                          deviceLimits.minUniformBufferOffsetAlignment :
                                          sizeof(int32_t);

    uboOffsetAlignment += (deviceLimits.minUniformBufferOffsetAlignment -
                           uboOffsetAlignment % deviceLimits.minUniformBufferOffsetAlignment) %
                          deviceLimits.minUniformBufferOffsetAlignment;

    const VkBufferCreateInfo bufferSampleIDInfo =
        makeBufferCreateInfo(uboOffsetAlignment * numSamples, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    const de::UniquePtr<BufferWithMemory> bufferSampleID(
        new BufferWithMemory(deviceInterface, device, allocator, bufferSampleIDInfo, MemoryRequirement::HostVisible));

    std::vector<uint32_t> sampleIDsOffsets(numSamples);

    {
        int8_t *sampleIDs = new int8_t[static_cast<uint32_t>(uboOffsetAlignment) * numSamples];

        for (int32_t sampleNdx = 0u; sampleNdx < static_cast<int32_t>(numSamples); ++sampleNdx)
        {
            sampleIDsOffsets[sampleNdx] = static_cast<uint32_t>(sampleNdx * uboOffsetAlignment);
            int8_t *samplePtr           = sampleIDs + sampleIDsOffsets[sampleNdx];

            deMemcpy(samplePtr, &sampleNdx, sizeof(int32_t));
        }

        deMemcpy(bufferSampleID->getAllocation().getHostPtr(), sampleIDs,
                 static_cast<uint32_t>(uboOffsetAlignment * numSamples));

        flushAlloc(deviceInterface, device, bufferSampleID->getAllocation());

        delete[] sampleIDs;
    }

    {
        const VkDescriptorImageInfo descImageInfo =
            makeDescriptorImageInfo(VK_NULL_HANDLE, imageViews[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        const VkDescriptorBufferInfo descBufferInfo = makeDescriptorBufferInfo(**bufferSampleID, 0u, sizeof(int32_t));

        DescriptorSetUpdateBuilder()
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &descImageInfo)
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &descBufferInfo)
            .update(deviceInterface, device);
    }

    // Create command buffer for compute and transfer oparations
    const Unique<VkCommandPool> commandPool(
        createCommandPool(deviceInterface, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    const Unique<VkCommandBuffer> commandBuffer(makeCommandBuffer(deviceInterface, device, *commandPool));

    // Start recording commands
    beginCommandBuffer(deviceInterface, *commandBuffer);

    {
        std::vector<VkImageMemoryBarrier> imageOutputAttachmentBarriers(firstSubpassAttachmentsCount + numSamples);

        imageOutputAttachmentBarriers[0] =
            makeImageMemoryBarrier(0u, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, **imageMS, fullImageRange);

        imageOutputAttachmentBarriers[1] =
            makeImageMemoryBarrier(0u, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, **imageRS, fullImageRange);

        for (uint32_t sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
        {
            imageOutputAttachmentBarriers[firstSubpassAttachmentsCount + sampleNdx] = makeImageMemoryBarrier(
                0u, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, **imagesPerSampleVec[sampleNdx], fullImageRange);
        }

        deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL,
                                           static_cast<uint32_t>(imageOutputAttachmentBarriers.size()),
                                           dataPointer(imageOutputAttachmentBarriers));
    }

    {
        const VkDeviceSize vertexStartOffset = 0u;

        std::vector<VkClearValue> clearValues(firstSubpassAttachmentsCount + numSamples);
        for (uint32_t attachmentNdx = 0u; attachmentNdx < firstSubpassAttachmentsCount + numSamples; ++attachmentNdx)
        {
            clearValues[attachmentNdx] = makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
        }

        renderPass.begin(deviceInterface, *commandBuffer,
                         makeRect2D(0, 0, imageMSInfo.extent.width, imageMSInfo.extent.height),
                         (uint32_t)clearValues.size(), dataPointer(clearValues));

        // Bind graphics pipeline
        graphicsPipelineMSPass.bind(*commandBuffer);

        const VkDescriptorSet *descriptorSetMSPass = createMSPassDescSet(m_imageMSParams, descriptorSetLayoutMSPass);

        if (descriptorSetMSPass)
        {
            // Bind descriptor set
            deviceInterface.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                  *pipelineLayoutMSPass, 0u, 1u, descriptorSetMSPass, 0u, DE_NULL);
        }

        // Bind vertex buffer
        deviceInterface.cmdBindVertexBuffers(*commandBuffer, 0u, 1u, &vertexBuffer->get(), &vertexStartOffset);

        // Perform a draw
        deviceInterface.cmdDraw(*commandBuffer, vertexDataDesc.verticesCount, 1u, 0u, 0u);

        for (uint32_t sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
        {
            renderPass.nextSubpass(deviceInterface, *commandBuffer, VK_SUBPASS_CONTENTS_INLINE);

            // Bind graphics pipeline
            graphicsPipelinesPerSampleFetch[sampleNdx].bind(*commandBuffer);

            // Bind descriptor set
            deviceInterface.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                  *pipelineLayoutPerSampleFetchPass, 0u, 1u, &descriptorSet.get(), 1u,
                                                  &sampleIDsOffsets[sampleNdx]);

            // Bind vertex buffer
            deviceInterface.cmdBindVertexBuffers(*commandBuffer, 0u, 1u, &vertexBufferPerSampleFetchPass->get(),
                                                 &vertexStartOffset);

            // Perform a draw
            deviceInterface.cmdDraw(*commandBuffer, 4u, 1u, 0u, 0u);
        }

        // End render pass
        renderPass.end(deviceInterface, *commandBuffer);
    }

    {
        const VkImageMemoryBarrier imageRSTransferBarrier = makeImageMemoryBarrier(
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **imageRS, fullImageRange);

        deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u,
                                           &imageRSTransferBarrier);
    }

    // Copy data from imageRS to buffer
    const uint32_t imageRSSizeInBytes =
        getImageSizeInBytes(imageRSInfo.extent, imageRSInfo.arrayLayers, m_imageFormat, imageRSInfo.mipLevels, 1u);

    const VkBufferCreateInfo bufferRSInfo = makeBufferCreateInfo(imageRSSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const de::UniquePtr<BufferWithMemory> bufferRS(
        new BufferWithMemory(deviceInterface, device, allocator, bufferRSInfo, MemoryRequirement::HostVisible));

    {
        const VkBufferImageCopy bufferImageCopy = {
            0u, // VkDeviceSize bufferOffset;
            0u, // uint32_t bufferRowLength;
            0u, // uint32_t bufferImageHeight;
            makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u,
                                       imageRSInfo.arrayLayers), // VkImageSubresourceLayers imageSubresource;
            makeOffset3D(0, 0, 0),                               // VkOffset3D imageOffset;
            imageRSInfo.extent,                                  // VkExtent3D imageExtent;
        };

        deviceInterface.cmdCopyImageToBuffer(*commandBuffer, **imageRS, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                             bufferRS->get(), 1u, &bufferImageCopy);
    }

    {
        const VkBufferMemoryBarrier bufferRSHostReadBarrier = makeBufferMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, bufferRS->get(), 0u, imageRSSizeInBytes);

        deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                           0u, 0u, DE_NULL, 1u, &bufferRSHostReadBarrier, 0u, DE_NULL);
    }

    // Copy data from per sample images to buffers
    std::vector<VkImageMemoryBarrier> imagesPerSampleTransferBarriers(numSamples);

    for (uint32_t sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
    {
        imagesPerSampleTransferBarriers[sampleNdx] = makeImageMemoryBarrier(
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **imagesPerSampleVec[sampleNdx], fullImageRange);
    }

    deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL,
                                       static_cast<uint32_t>(imagesPerSampleTransferBarriers.size()),
                                       dataPointer(imagesPerSampleTransferBarriers));

    std::vector<de::SharedPtr<BufferWithMemory>> buffersPerSample(numSamples);

    for (uint32_t sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
    {
        buffersPerSample[sampleNdx] = de::SharedPtr<BufferWithMemory>(
            new BufferWithMemory(deviceInterface, device, allocator, bufferRSInfo, MemoryRequirement::HostVisible));

        const VkBufferImageCopy bufferImageCopy = {
            0u, // VkDeviceSize bufferOffset;
            0u, // uint32_t bufferRowLength;
            0u, // uint32_t bufferImageHeight;
            makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u,
                                       imageRSInfo.arrayLayers), // VkImageSubresourceLayers imageSubresource;
            makeOffset3D(0, 0, 0),                               // VkOffset3D imageOffset;
            imageRSInfo.extent,                                  // VkExtent3D imageExtent;
        };

        deviceInterface.cmdCopyImageToBuffer(*commandBuffer, **imagesPerSampleVec[sampleNdx],
                                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **buffersPerSample[sampleNdx], 1u,
                                             &bufferImageCopy);
    }

    std::vector<VkBufferMemoryBarrier> buffersPerSampleHostReadBarriers(numSamples);

    for (uint32_t sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
    {
        buffersPerSampleHostReadBarriers[sampleNdx] =
            makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                                    **buffersPerSample[sampleNdx], 0u, imageRSSizeInBytes);
    }

    deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
                                       0u, DE_NULL, static_cast<uint32_t>(buffersPerSampleHostReadBarriers.size()),
                                       dataPointer(buffersPerSampleHostReadBarriers), 0u, DE_NULL);

    // End recording commands
    endCommandBuffer(deviceInterface, *commandBuffer);

    // Submit commands for execution and wait for completion
    submitCommandsAndWait(deviceInterface, device, queue, *commandBuffer);

    // Retrieve data from bufferRS to host memory
    const Allocation &bufferRSAlloc = bufferRS->getAllocation();

    invalidateAlloc(deviceInterface, device, bufferRSAlloc);

    const tcu::ConstPixelBufferAccess bufferRSData(m_imageFormat, imageRSInfo.extent.width, imageRSInfo.extent.height,
                                                   imageRSInfo.extent.depth * imageRSInfo.arrayLayers,
                                                   bufferRSAlloc.getHostPtr());

    std::stringstream resolveName;
    resolveName << "Resolve image " << getImageTypeName(m_imageType) << "_" << bufferRSData.getWidth() << "_"
                << bufferRSData.getHeight() << "_" << bufferRSData.getDepth() << std::endl;

    m_context.getTestContext().getLog() << tcu::TestLog::Section(resolveName.str(), resolveName.str())
                                        << tcu::LogImage("resolve", "", bufferRSData) << tcu::TestLog::EndSection;

    std::vector<tcu::ConstPixelBufferAccess> buffersPerSampleData(numSamples);

    // Retrieve data from per sample buffers to host memory
    for (uint32_t sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
    {
        const Allocation &bufferAlloc = buffersPerSample[sampleNdx]->getAllocation();

        invalidateAlloc(deviceInterface, device, bufferAlloc);

        buffersPerSampleData[sampleNdx] =
            tcu::ConstPixelBufferAccess(m_imageFormat, imageRSInfo.extent.width, imageRSInfo.extent.height,
                                        imageRSInfo.extent.depth * imageRSInfo.arrayLayers, bufferAlloc.getHostPtr());

        std::stringstream sampleName;
        sampleName << "Sample " << sampleNdx << " image" << std::endl;

        m_context.getTestContext().getLog()
            << tcu::TestLog::Section(sampleName.str(), sampleName.str())
            << tcu::LogImage("sample", "", buffersPerSampleData[sampleNdx]) << tcu::TestLog::EndSection;
    }

    return verifyImageData(imageMSInfo, imageRSInfo, buffersPerSampleData, bufferRSData);
}

} // namespace multisample
} // namespace pipeline
} // namespace vkt
