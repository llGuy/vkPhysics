#include "vk_descriptor.hpp"

#include "vk_context.hpp"

namespace vk {

void init_descriptor_pool() {
    VkDescriptorPoolSize sizes[11];
    sizes[0].descriptorCount = 1000; sizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    sizes[1].descriptorCount = 1000; sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[2].descriptorCount = 1000; sizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    sizes[3].descriptorCount = 1000; sizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    sizes[4].descriptorCount = 1000; sizes[4].type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    sizes[5].descriptorCount = 1000; sizes[5].type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    sizes[6].descriptorCount = 1000; sizes[6].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[7].descriptorCount = 1000; sizes[7].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    sizes[8].descriptorCount = 1000; sizes[8].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    sizes[9].descriptorCount = 1000; sizes[9].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    sizes[10].descriptorCount = 1000; sizes[10].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * 11;
    pool_info.poolSizeCount = 11;
    pool_info.pPoolSizes = sizes;

    VK_CHECK(vkCreateDescriptorPool(g_ctx->device, &pool_info, NULL, &g_ctx->descriptor_pool));
}

void init_descriptor_layouts() {
    memset(&g_ctx->descriptor_layouts, 0, sizeof(g_ctx->descriptor_layouts));
    
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &binding;

    VK_CHECK(vkCreateDescriptorSetLayout(g_ctx->device, &layout_info, NULL, &g_ctx->descriptor_layouts.sampler[0]));

    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    
    VK_CHECK(vkCreateDescriptorSetLayout(g_ctx->device, &layout_info, NULL, &g_ctx->descriptor_layouts.uniform_buffer[0]));

    binding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    VK_CHECK(vkCreateDescriptorSetLayout(g_ctx->device, &layout_info, NULL, &g_ctx->descriptor_layouts.input_attachment[0]));
}

VkDescriptorSetLayout get_descriptor_layout(
    VkDescriptorType type,
    uint32_t count) {
    if (count > MAX_DESCRIPTOR_SET_LAYOUTS_PER_TYPE) {
        LOG_ERROR("Descriptor count for layout is too high\n");
        exit(1);
        return VK_NULL_HANDLE;
    }
    
    switch (type) {
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
        if (g_ctx->descriptor_layouts.sampler[count - 1] == VK_NULL_HANDLE) {
            VkDescriptorSetLayoutBinding *bindings = ALLOCA(VkDescriptorSetLayoutBinding, count);
            memset(bindings, 0, sizeof(VkDescriptorSetLayoutBinding) * count);

            for (uint32_t i = 0; i < count; ++i) {
                bindings[i].binding = i;
                bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                bindings[i].descriptorCount = 1;
                bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            VkDescriptorSetLayoutCreateInfo layout_info = {};
            layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_info.bindingCount = count;
            layout_info.pBindings = bindings;

            VK_CHECK(vkCreateDescriptorSetLayout(g_ctx->device, &layout_info, NULL, &g_ctx->descriptor_layouts.sampler[count - 1]));
        }

        return g_ctx->descriptor_layouts.sampler[count - 1];
    }
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: {
        if (g_ctx->descriptor_layouts.sampler[count - 1] == VK_NULL_HANDLE) {
            VkDescriptorSetLayoutBinding *bindings = ALLOCA(VkDescriptorSetLayoutBinding, count);

            for (uint32_t i = 0; i < count; ++i) {
                bindings[i].binding = i;
                bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
                bindings[i].descriptorCount = 1;
                bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            VkDescriptorSetLayoutCreateInfo layout_info = {};
            layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_info.bindingCount = count;
            layout_info.pBindings = bindings;

            VK_CHECK(vkCreateDescriptorSetLayout(g_ctx->device, &layout_info, NULL, &g_ctx->descriptor_layouts.sampler[count - 1]));
        }
        
        return g_ctx->descriptor_layouts.input_attachment[count - 1];
    }
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
        if (g_ctx->descriptor_layouts.sampler[count - 1] == VK_NULL_HANDLE) {
            VkDescriptorSetLayoutBinding *bindings = ALLOCA(VkDescriptorSetLayoutBinding, count);

            for (uint32_t i = 0; i < count; ++i) {
                bindings[i].binding = i;
                bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
                bindings[i].descriptorCount = 1;
                bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            VkDescriptorSetLayoutCreateInfo layout_info = {};
            layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_info.bindingCount = count;
            layout_info.pBindings = bindings;

            VK_CHECK(vkCreateDescriptorSetLayout(g_ctx->device, &layout_info, NULL, &g_ctx->descriptor_layouts.sampler[count - 1]));
        }
        
        return g_ctx->descriptor_layouts.uniform_buffer[count - 1];
    }
    default:
        LOG_ERROR("Specified invalid descriptor type\n");
        return (VkDescriptorSetLayout)0;
    }
}

VkDescriptorSet create_buffer_descriptor_set(VkBuffer buffer, VkDeviceSize buffer_size, VkDescriptorType type) {
    VkDescriptorSetLayout descriptor_layout = get_descriptor_layout(type, 1);
    
    VkDescriptorSetAllocateInfo allocate_info = {};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool = g_ctx->descriptor_pool;
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &descriptor_layout;

    VkDescriptorSet set;
    
    vkAllocateDescriptorSets(g_ctx->device, &allocate_info, &set);

    VkDescriptorBufferInfo buffer_info = {};
    buffer_info.buffer = buffer;
    buffer_info.offset = 0;
    buffer_info.range = buffer_size;
    
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(g_ctx->device, 1, &write, 0, NULL);

    return set;
}

VkDescriptorSet create_image_descriptor_set(
    VkImageView image,
    VkSampler sampler,
    VkDescriptorType type) {
    VkDescriptorSetLayout descriptor_layout = get_descriptor_layout(type, 1);
    
    VkDescriptorSetAllocateInfo allocate_info = {};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool = g_ctx->descriptor_pool;
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &descriptor_layout;

    VkDescriptorSet set;
    
    vkAllocateDescriptorSets(g_ctx->device, &allocate_info, &set);

    VkDescriptorImageInfo image_info = {};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = image;
    image_info.sampler = sampler;
    
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(g_ctx->device, 1, &write, 0, NULL);

    return set;
}

VkDescriptorSet create_image_descriptor_set(
    VkImageView *images,
    VkSampler *samplers,
    uint32_t count,
    VkDescriptorType type) {
    VkDescriptorSetLayout descriptor_layout = get_descriptor_layout(type, count);
    
    VkDescriptorSetAllocateInfo allocate_info = {};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool = g_ctx->descriptor_pool;
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &descriptor_layout;

    VkDescriptorSet set;
    
    vkAllocateDescriptorSets(g_ctx->device, &allocate_info, &set);

    VkDescriptorImageInfo *image_info = ALLOCA(VkDescriptorImageInfo, count);
    VkWriteDescriptorSet *write = ALLOCA(VkWriteDescriptorSet, count);
    memset(image_info, 0, sizeof(VkDescriptorImageInfo) * count);
    memset(write, 0, sizeof(VkWriteDescriptorSet) * count);
    
    for (uint32_t i = 0; i < count; ++i) {
        image_info[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info[i].imageView = images[i];
        image_info[i].sampler = samplers[i];

        write[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write[i].dstSet = set;
        write[i].dstBinding = i;
        write[i].dstArrayElement = 0;
        write[i].descriptorCount = 1;
        write[i].descriptorType = type;
        write[i].pImageInfo = &image_info[i];
    }

    vkUpdateDescriptorSets(g_ctx->device, count, write, 0, NULL);

    return set;
}

}
