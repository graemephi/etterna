#include "Etterna/Globals/global.h"
#include "Display_VK.h"

#ifdef _WIN32
#include "archutils/Win32/GraphicsWindow.h"

// For development
#define DEBUG 1

#ifdef DEBUG
// Whoever neutered this in global.h is a bad person (morally)
#undef DEBUG_ASSERT
#define DEBUG_ASSERT(cond) ((cond) ? (void)0 : __debugbreak())
#endif

#define VK_USE_PLATFORM_WIN32_KHR
#else
#error todo
#endif

#define VOLK_IMPLEMENTATION
#include "volk.h"
#include "vulkan/vulkan.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "Display_VK_shaders.h"

struct Buffer
{
	static const size_t Size = 65536;

	VkBuffer buffer;
	VmaAllocation allocation;
	void* mappedMemory;
};

struct QuadsProgram
{
	struct Quad
	{
		RectF rect;
		RageColor colors[4];
		RectF tex;
	};

	std::vector<Quad> quads;
	std::vector<uint16_t> indices;

	VkShaderModule vertexShader;
	VkShaderModule fragmentShader;
	VkPipelineLayout pipelineLayout;
	VkDescriptorSet descriptorSet;
	Buffer quadsBuffer;
	Buffer indexBuffer;
};

struct Swapchain
{
	VkFormat format;
	VkExtent2D dim;

	VkSwapchainKHR swapchain;
	VkRenderPass renderPass;

	std::vector<VkImage> images;
	std::vector<VkImageView> imageViews;
	std::vector<VkFramebuffer> framebuffers;
	std::vector<VkPipeline> pipelines;
};

static struct VulkanState
{
	VkPhysicalDeviceProperties properties;

	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	VkDescriptorPool descriptorPool;
	uint32_t queueFamilyIndex;
	VkQueue queue;

	VmaAllocator allocator;

	VkSurfaceKHR surface;
	Swapchain swapchain;

	struct
	{
		VkSemaphore release;
		VkSemaphore acquire;
	} sem;

	QuadsProgram quads;

	uint32_t frameImageIndex;
} g_vk = {};

VkBool32
DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			  VkDebugUtilsMessageTypeFlagsEXT messageTypes,
			  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			  void* pUserData)
{
	DEBUG_ASSERT(
	  (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) == 0);
	// Filter "Device Extension" message spam
	if (strncmp(pCallbackData->pMessage, "Device Extension", 16) != 0) {
		printf("%s\n", pCallbackData->pMessage);
		// Flush immediately because of BAD DEBUGGERS THAT TRUNCATE STRINGS FOR
		// DISPLAY HELLO
		fflush(0);
	}
	return VK_FALSE;
}

auto
PhysicalDeviceQueueFamilyCanPresent(VkPhysicalDevice device,
									uint32_t queueFamilyIndex) -> bool
{
#ifdef VK_USE_PLATFORM_WIN32_KHR
	return bool(
	  vkGetPhysicalDeviceWin32PresentationSupportKHR(device, queueFamilyIndex));
#else
#error todo
#endif
}

void
DestroySwapchain(Swapchain& swapchain)
{
	for (size_t i = 0; i < swapchain.pipelines.size(); i++) {
		if (swapchain.pipelines[i]) {
			vkDestroyPipeline(g_vk.device, swapchain.pipelines[i], 0);
		}
	}
	swapchain.pipelines.clear();
	for (size_t i = 0; i < swapchain.framebuffers.size(); i++) {
		if (swapchain.framebuffers[i]) {
			vkDestroyFramebuffer(g_vk.device, swapchain.framebuffers[i], 0);
		}
	}
	swapchain.framebuffers.clear();
	for (size_t i = 0; i < swapchain.imageViews.size(); i++) {
		if (swapchain.imageViews[i]) {
			vkDestroyImageView(g_vk.device, swapchain.imageViews[i], 0);
		}
	}
	swapchain.imageViews.clear();
	if (swapchain.renderPass) {
		vkDestroyRenderPass(g_vk.device, swapchain.renderPass, 0);
		swapchain.renderPass = 0;
	}
	if (swapchain.swapchain) {
		vkDestroySwapchainKHR(g_vk.device, swapchain.swapchain, 0);
		swapchain.swapchain = 0;
	}
}

auto
CreateSwapchain(Swapchain& swapchain, VkSwapchainKHR oldSwapchain = 0)
  -> VkResult
{
	VkSurfaceFormatKHR format = {};
	VkResult rc = VK_SUCCESS;
	VkSurfaceCapabilitiesKHR surfaceCapabilities = {};

	rc = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
	  g_vk.physicalDevice, g_vk.surface, &surfaceCapabilities);

	if (rc != VK_SUCCESS) {
		goto bail;
	}

	VkBool32 queueSupportsSurface = false;
	rc = vkGetPhysicalDeviceSurfaceSupportKHR(g_vk.physicalDevice,
											  g_vk.queueFamilyIndex,
											  g_vk.surface,
											  &queueSupportsSurface);

	if (rc != VK_SUCCESS || queueSupportsSurface == false) {
		goto bail;
	}

	swapchain.dim = surfaceCapabilities.currentExtent;
	const uint32_t& width = swapchain.dim.width;
	const uint32_t& height = swapchain.dim.height;

	// Get image format
	{
		std::vector<VkSurfaceFormatKHR> formats;
		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(
		  g_vk.physicalDevice, g_vk.surface, &formatCount, 0);
		formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(
		  g_vk.physicalDevice, g_vk.surface, &formatCount, formats.data());

		for (uint32_t i = 0; i < formatCount; i++) {
			bool spaceOk =
			  formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
			bool formatOk = (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM) ||
							(formats[i].format == VK_FORMAT_B8G8R8A8_UNORM);
			if (spaceOk && formatOk) {
				format = formats[i];
				break;
			}
		}

		if (format.format == VK_FORMAT_UNDEFINED) {
			goto bail;
		}

		swapchain.format = format.format;
	}

	// Create swap chain
	{
		VkCompositeAlphaFlagBitsKHR compositeAlpha =
		  VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
		const auto& scaFlags = surfaceCapabilities.supportedCompositeAlpha;
		if (scaFlags & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) {
			compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		} else if (scaFlags & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) {
			compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
		}

		VkSwapchainCreateInfoKHR swapchainInfo = {
			VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR
		};
		swapchainInfo.surface = g_vk.surface;
		swapchainInfo.minImageCount =
		  std::min(2u, surfaceCapabilities.minImageCount);
		swapchainInfo.imageFormat = format.format;
		swapchainInfo.imageColorSpace = format.colorSpace;
		swapchainInfo.imageExtent = swapchain.dim;
		swapchainInfo.imageArrayLayers = 1;
		swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		// TODO: If graphics and present queues are different, this must be
		// VK_SHARING_MODE_CONCURRENT
		swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainInfo.queueFamilyIndexCount = 1;
		swapchainInfo.pQueueFamilyIndices = &g_vk.queueFamilyIndex;
		swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		swapchainInfo.compositeAlpha = compositeAlpha;
		// TODO: Not guaranteed to be supported. We're latency sensitive and
		// very fast to render, so we want this, not FIFO (which is always
		// supported)
		swapchainInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
		swapchainInfo.clipped = VK_TRUE;
		swapchainInfo.oldSwapchain = oldSwapchain;

		rc = vkCreateSwapchainKHR(
		  g_vk.device, &swapchainInfo, 0, &swapchain.swapchain);
		DEBUG_ASSERT(swapchain.swapchain);

		if (rc != VK_SUCCESS) {
			goto bail;
		}
	}

	// Create swapchain images, image views
	{
		uint32_t swapchainImageCount = 0;
		vkGetSwapchainImagesKHR(
		  g_vk.device, swapchain.swapchain, &swapchainImageCount, 0);
		swapchain.images.resize(swapchainImageCount);
		vkGetSwapchainImagesKHR(g_vk.device,
								swapchain.swapchain,
								&swapchainImageCount,
								swapchain.images.data());

		swapchain.imageViews.resize(swapchainImageCount);
		for (size_t i = 0; i < swapchainImageCount; i++) {
			VkImageViewCreateInfo imageViewInfo = {
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO
			};
			imageViewInfo.image = swapchain.images[i];
			imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewInfo.format = format.format;
			imageViewInfo.subresourceRange.aspectMask =
			  VK_IMAGE_ASPECT_COLOR_BIT;
			imageViewInfo.subresourceRange.levelCount = 1;
			imageViewInfo.subresourceRange.layerCount = 1;

			rc = vkCreateImageView(
			  g_vk.device, &imageViewInfo, 0, &swapchain.imageViews[i]);
			DEBUG_ASSERT(swapchain.imageViews[i]);

			if (rc != VK_SUCCESS) {
				goto bail;
			}
		}
	}

	// Create render pass
	{
		VkAttachmentDescription attachment = {};
		attachment.format = format.format;
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference attachmentReference = {
			0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &attachmentReference;

		// https://web.archive.org/web/20201112014635if_/https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#swapchain-image-acquire-and-present
		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkRenderPassCreateInfo renderPassInfo = {
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO
		};
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &attachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		rc = vkCreateRenderPass(
		  g_vk.device, &renderPassInfo, 0, &swapchain.renderPass);
		DEBUG_ASSERT(swapchain.renderPass);

		if (rc != VK_SUCCESS) {
			goto bail;
		}
	}

	// Create framebuffers
	{
		swapchain.framebuffers.resize(swapchain.images.size());
		for (size_t i = 0; i < swapchain.framebuffers.size(); i++) {
			VkFramebufferCreateInfo framebufferInfo = {
				VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO
			};
			framebufferInfo.renderPass = swapchain.renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = &swapchain.imageViews[i];
			framebufferInfo.width = width;
			framebufferInfo.height = height;
			framebufferInfo.layers = 1;

			rc = vkCreateFramebuffer(
			  g_vk.device, &framebufferInfo, 0, &swapchain.framebuffers[i]);
			DEBUG_ASSERT(swapchain.framebuffers[i]);

			if (rc != VK_SUCCESS) {
				goto bail;
			}
		}
	}

	// Create graphics pipeline for quads
	{
		VkGraphicsPipelineCreateInfo pipelineInfo = {
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO
		};

		VkPipelineShaderStageCreateInfo shaderInfo[2] = {};
		auto& vertStageInfo = shaderInfo[0];
		auto& fragStageInfo = shaderInfo[1];

		vertStageInfo.sType =
		  VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertStageInfo.module = g_vk.quads.vertexShader;
		vertStageInfo.pName = "main";

		fragStageInfo.sType =
		  VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragStageInfo.module = g_vk.quads.fragmentShader;
		fragStageInfo.pName = "main";

		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderInfo;

		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
		};
		pipelineInfo.pVertexInputState = &vertexInputInfo;

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
		};
		inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;

		// Upside down viewport to match D3D
		VkViewport viewport = {};
		viewport.x = 0;
		viewport.y = float(height);
		viewport.width = float(width);
		viewport.height = -float(height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		VkRect2D scissor = { { 0, 1 }, { width, height } };

		VkPipelineViewportStateCreateInfo viewportInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO
		};
		viewportInfo.viewportCount = 1;
		viewportInfo.pViewports = &viewport;
		viewportInfo.scissorCount = 1;
		viewportInfo.pScissors = &scissor;
		pipelineInfo.pViewportState = &viewportInfo;

		VkPipelineRasterizationStateCreateInfo rasterizationInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO
		};
		rasterizationInfo.lineWidth = 1.0f;
		rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		pipelineInfo.pRasterizationState = &rasterizationInfo;

		VkPipelineMultisampleStateCreateInfo multisampleInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO
		};
		multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		pipelineInfo.pMultisampleState = &multisampleInfo;

		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		// Equivalent to sm's BLEND_NORMAL. Changing these doesn't require a new
		// pipeline
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor =
		  VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.colorWriteMask =
		  VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo colorBlendInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO
		};
		colorBlendInfo.attachmentCount = 1;
		colorBlendInfo.pAttachments = &colorBlendAttachment;
		pipelineInfo.pColorBlendState = &colorBlendInfo;

		VkPipelineDynamicStateCreateInfo dynamicInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO
		};
		pipelineInfo.pDynamicState = &dynamicInfo;

		pipelineInfo.layout = g_vk.quads.pipelineLayout;
		pipelineInfo.renderPass = swapchain.renderPass;

		swapchain.pipelines.resize(1);
		rc = vkCreateGraphicsPipelines(
		  g_vk.device, 0, 1, &pipelineInfo, 0, swapchain.pipelines.data());
		DEBUG_ASSERT(swapchain.pipelines[0]);

		if (rc != VK_SUCCESS) {
			goto bail;
		}
	}

	return rc;

bail:
	DestroySwapchain(swapchain);
	return rc;
}

void
RecreateSwapchain(Swapchain& swapchain)
{
	// Create a new swapchain first so the driver can reuse resources, then
	// destroy the old swapchain.
	Swapchain newSwapchain{};
	VkResult rc = CreateSwapchain(newSwapchain, swapchain.swapchain);

	if (rc != VK_SUCCESS) {
		FAIL_M("Display_VK: failed to recreate swapchain");
	}

	vkDeviceWaitIdle(g_vk.device);
	DestroySwapchain(swapchain);
	swapchain = newSwapchain;
}

auto
CreatePersistentlyMappedBuffer(Buffer& buffer, VkBufferUsageFlags bufferUsage)
  -> VkResult
{
	VmaAllocationCreateInfo allocCreateInfo = {};
	allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	allocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

	VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.size = Buffer::Size;
	bufferInfo.usage = bufferUsage;

	VmaAllocationInfo allocInfo = {};
	VkResult rc = vmaCreateBuffer(g_vk.allocator,
								  &bufferInfo,
								  &allocCreateInfo,
								  &buffer.buffer,
								  &buffer.allocation,
								  &allocInfo);
	DEBUG_ASSERT(buffer.buffer);

	buffer.mappedMemory = allocInfo.pMappedData;
	return rc;
}

auto
CreateQuadsProgram(QuadsProgram& quads) -> VkResult
{
	// Shaders
	VkShaderModuleCreateInfo vertexInfo = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO
	};
	vertexInfo.codeSize = sizeof(Display_VK_quads_vert);
	vertexInfo.pCode = (uint32_t*)Display_VK_quads_vert;

	VkResult rc =
	  vkCreateShaderModule(g_vk.device, &vertexInfo, 0, &quads.vertexShader);
	DEBUG_ASSERT(quads.vertexShader);

	if (rc != VK_SUCCESS) {
		return rc;
	}

	VkShaderModuleCreateInfo fragmentInfo = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO
	};
	fragmentInfo.codeSize = sizeof(Display_VK_quads_frag);
	fragmentInfo.pCode = (uint32_t*)Display_VK_quads_frag;

	rc = vkCreateShaderModule(
	  g_vk.device, &fragmentInfo, 0, &quads.fragmentShader);
	DEBUG_ASSERT(quads.fragmentShader);

	if (rc != VK_SUCCESS) {
		return rc;
	}

	// Pipeline layout
	VkDescriptorSetLayoutBinding setLayoutBinding = {};
	setLayoutBinding.binding = 0;
	setLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	setLayoutBinding.descriptorCount = 1;
	setLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutCreateInfo setLayoutInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO
	};
	setLayoutInfo.bindingCount = 1;
	setLayoutInfo.pBindings = &setLayoutBinding;

	VkDescriptorSetLayout setLayout = 0;
	rc =
	  vkCreateDescriptorSetLayout(g_vk.device, &setLayoutInfo, 0, &setLayout);

	if (rc != VK_SUCCESS) {
		return rc;
	}

	VkDescriptorSetAllocateInfo setAllocateInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO
	};
	setAllocateInfo.descriptorPool = g_vk.descriptorPool;
	setAllocateInfo.descriptorSetCount = 1;
	setAllocateInfo.pSetLayouts = &setLayout;

	rc = vkAllocateDescriptorSets(
	  g_vk.device, &setAllocateInfo, &quads.descriptorSet);

	if (rc != VK_SUCCESS) {
		return rc;
	}

	VkPipelineLayoutCreateInfo layoutInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
	};
	layoutInfo.pSetLayouts = &setLayout;
	layoutInfo.setLayoutCount = 1;
	rc = vkCreatePipelineLayout(
	  g_vk.device, &layoutInfo, 0, &quads.pipelineLayout);
	DEBUG_ASSERT(quads.pipelineLayout);

	if (rc != VK_SUCCESS) {
		return rc;
	}

	// Buffers
	rc = CreatePersistentlyMappedBuffer(quads.quadsBuffer,
										VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	DEBUG_ASSERT(quads.quadsBuffer.buffer);

	if (rc != VK_SUCCESS) {
		return rc;
	}

	rc = CreatePersistentlyMappedBuffer(quads.indexBuffer,
										VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	DEBUG_ASSERT(quads.indexBuffer.buffer);

	if (rc != VK_SUCCESS) {
		return rc;
	}

	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer = g_vk.quads.quadsBuffer.buffer;
	bufferInfo.range = VK_WHOLE_SIZE;

	VkWriteDescriptorSet writeDescriptorSet = {
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET
	};
	writeDescriptorSet.dstSet = g_vk.quads.descriptorSet;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writeDescriptorSet.pBufferInfo = &bufferInfo;

	vkUpdateDescriptorSets(g_vk.device, 1, &writeDescriptorSet, 0, 0);

	vkDestroyDescriptorSetLayout(g_vk.device, setLayout, 0);

	return rc;
}

auto
Display_VK::Init(const VideoModeParams& p, bool bAllowUnacceleratedRenderer)
  -> std::string
{
#ifdef _WIN32
	// Can't use LowLevelWindow because it's OpenGL
	// TODO: Verify the boolean here still applies for Vulkan
	GraphicsWindow::Initialize(false);
#else
#error todo. glfw maybe
#endif

	std::vector<char const*> layers;
	std::vector<char const*> extensions;
	std::vector<char const*> deviceExtensions;

	VkResult rc = volkInitialize();

	if (rc != VK_SUCCESS) {
		goto bail;
	}

#ifdef DEBUG
	// Although we can compile without the SDK, this validation layer requires
	// it, and Vulkan will fail to initialize without it.
	// TODO: detect SDK is installed from cmake and add define? Or query for
	// extensions
	layers.push_back("VK_LAYER_KHRONOS_validation");
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

	extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef VK_USE_PLATFORM_WIN32_KHR
	extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
#error todo
#endif

	// Create instace
	{
		// Dunno what a good version to target is. Gonna stick 1.1 in here for
		// now, 1.2 is quite new
		VkApplicationInfo applicationInfo = {
			VK_STRUCTURE_TYPE_APPLICATION_INFO
		};
		applicationInfo.apiVersion = VK_API_VERSION_1_1;

		VkInstanceCreateInfo instanceInfo = {
			VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
		};

		if (layers.size() > 0) {
			instanceInfo.ppEnabledLayerNames = layers.data();
			instanceInfo.enabledLayerCount = layers.size();
		}

		if (extensions.size() > 0) {
			instanceInfo.ppEnabledExtensionNames = extensions.data();
			instanceInfo.enabledExtensionCount = extensions.size();
		}

		instanceInfo.pApplicationInfo = &applicationInfo;
		rc = vkCreateInstance(&instanceInfo, 0, &g_vk.instance);
		DEBUG_ASSERT(g_vk.instance);

		if (rc != VK_SUCCESS) {
			goto bail;
		}
	}

	volkLoadInstance(g_vk.instance);

#ifdef DEBUG
	// Create debug messenger
	{
		VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo = {
			VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT
		};
		debugMessengerInfo.messageSeverity =
		  VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		  VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		  VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugMessengerInfo.messageType =
		  VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugMessengerInfo.pfnUserCallback = DebugCallback;
		vkCreateDebugUtilsMessengerEXT(
		  g_vk.instance, &debugMessengerInfo, 0, &g_vk.debugMessenger);
	}
#endif

	// Create device
	{
		std::vector<VkPhysicalDevice> devices;
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(g_vk.instance, &deviceCount, 0);
		devices.resize(deviceCount);
		vkEnumeratePhysicalDevices(g_vk.instance, &deviceCount, devices.data());

		bool haveDiscreteGPU = false;
		bool haveIntegratedGPU = false;
		for (size_t i = 0; i < devices.size(); i++) {
			VkPhysicalDeviceProperties properties = {};
			vkGetPhysicalDeviceProperties(devices[i], &properties);

			bool isDiscrete =
			  properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
			bool isIntegrated =
			  properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;

			if (!isDiscrete && !isIntegrated) {
				continue;
			}

			std::vector<VkQueueFamilyProperties> queueProperties;
			uint32_t queueFamilyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(
			  devices[i], &queueFamilyCount, 0);
			queueProperties.resize(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(
			  devices[i], &queueFamilyCount, queueProperties.data());

			// TODO: Graphics queue and present queue can be different
			// apparently
			uint32_t queueFamilyIndex = -1;
			for (uint32_t i = 0; i < queueFamilyCount; i++) {
				if (PhysicalDeviceQueueFamilyCanPresent(devices[i], i) &&
					(queueProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
					queueFamilyIndex = i;
					break;
				}
			}

			if (isDiscrete) {
				haveDiscreteGPU = true;
			}

			if (isIntegrated) {
				haveIntegratedGPU = true;
			}

			if (isDiscrete || (isIntegrated && !haveDiscreteGPU)) {
				g_vk.properties = properties;
				g_vk.physicalDevice = devices[i];
				g_vk.queueFamilyIndex = queueFamilyIndex;
			}

			if (haveDiscreteGPU) {
				break;
			}
		}

		if (!haveDiscreteGPU && !haveIntegratedGPU) {
			goto bail;
		}

		float queuePriorites[] = { 0.0f };
		VkDeviceQueueCreateInfo queueInfo = {
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO
		};
		queueInfo.queueFamilyIndex = g_vk.queueFamilyIndex;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = queuePriorites;

		deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

		VkDeviceCreateInfo deviceInfo = {
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO
		};
		deviceInfo.queueCreateInfoCount = 1;
		deviceInfo.pQueueCreateInfos = &queueInfo;
		deviceInfo.enabledExtensionCount = deviceExtensions.size();
		deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

		rc = vkCreateDevice(g_vk.physicalDevice, &deviceInfo, 0, &g_vk.device);
		DEBUG_ASSERT(g_vk.device);

		if (rc != VK_SUCCESS) {
			goto bail;
		}

		volkLoadDevice(g_vk.device);

		vkGetDeviceQueue(g_vk.device, g_vk.queueFamilyIndex, 0, &g_vk.queue);
		DEBUG_ASSERT(g_vk.queue);
	}

	// Initialize VulkanMemoryAllocator
	{
		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_1;
		allocatorInfo.physicalDevice = g_vk.physicalDevice;
		allocatorInfo.device = g_vk.device;
		allocatorInfo.instance = g_vk.instance;

		vmaCreateAllocator(&allocatorInfo, &g_vk.allocator);
	}

	// Create semaphores
	{
		VkSemaphoreCreateInfo semaphoreInfo = {
			VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
		};
		rc =
		  vkCreateSemaphore(g_vk.device, &semaphoreInfo, 0, &g_vk.sem.release);
		DEBUG_ASSERT(g_vk.sem.release);

		if (rc != VK_SUCCESS) {
			goto bail;
		}

		rc =
		  vkCreateSemaphore(g_vk.device, &semaphoreInfo, 0, &g_vk.sem.acquire);
		DEBUG_ASSERT(g_vk.sem.acquire);

		if (rc != VK_SUCCESS) {
			goto bail;
		}
	}

	// Create command pool, buffer
	{
		VkCommandPoolCreateInfo commandPoolInfo = {
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO
		};
		commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		commandPoolInfo.queueFamilyIndex = g_vk.queueFamilyIndex;

		rc = vkCreateCommandPool(
		  g_vk.device, &commandPoolInfo, 0, &g_vk.commandPool);
		DEBUG_ASSERT(g_vk.commandPool);

		if (rc != VK_SUCCESS) {
			goto bail;
		}

		VkCommandBufferAllocateInfo allocateInfo = {
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO
		};
		allocateInfo.commandPool = g_vk.commandPool;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandBufferCount = 1;

		rc = vkAllocateCommandBuffers(
		  g_vk.device, &allocateInfo, &g_vk.commandBuffer);
		DEBUG_ASSERT(g_vk.commandBuffer);

		if (rc != VK_SUCCESS) {
			goto bail;
		}
	}

	// Create descriptor pool
	{
		VkDescriptorPoolSize poolSizes[1] = {};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSizes[0].descriptorCount = 1;

		VkDescriptorPoolCreateInfo descriptorPoolInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO
		};
		descriptorPoolInfo.poolSizeCount = 1;
		descriptorPoolInfo.pPoolSizes = poolSizes;
		descriptorPoolInfo.maxSets = 1;

		rc = vkCreateDescriptorPool(
		  g_vk.device, &descriptorPoolInfo, 0, &g_vk.descriptorPool);

		if (rc != VK_SUCCESS) {
			goto bail;
		}
	}

	rc = CreateQuadsProgram(g_vk.quads);

	if (rc != VK_SUCCESS) {
		goto bail;
	}

	// Rest of the initialization happens in TryVideoMode

	bool ignored = false;
	return SetVideoMode(p, ignored);

bail:
	// "All child objects created using instance must have been destroyed prior
	// to destroying instance" whats the worse that could happen
	if (g_vk.quads.indexBuffer.buffer) {
		vmaDestroyBuffer(g_vk.allocator,
						 g_vk.quads.indexBuffer.buffer,
						 g_vk.quads.indexBuffer.allocation);
		g_vk.quads.indexBuffer.buffer = 0;
		g_vk.quads.indexBuffer.allocation = 0;
	}
	if (g_vk.quads.quadsBuffer.buffer) {
		vmaDestroyBuffer(g_vk.allocator,
						 g_vk.quads.quadsBuffer.buffer,
						 g_vk.quads.quadsBuffer.allocation);
		g_vk.quads.quadsBuffer.buffer = 0;
		g_vk.quads.quadsBuffer.allocation = 0;
	}
	if (g_vk.quads.pipelineLayout) {
		vkDestroyPipelineLayout(g_vk.device, g_vk.quads.pipelineLayout, 0);
		g_vk.quads.pipelineLayout = 0;
	}
	if (g_vk.quads.vertexShader) {
		vkDestroyShaderModule(g_vk.device, g_vk.quads.vertexShader, 0);
		g_vk.quads.vertexShader = 0;
	}
	if (g_vk.quads.fragmentShader) {
		vkDestroyShaderModule(g_vk.device, g_vk.quads.fragmentShader, 0);
		g_vk.quads.fragmentShader = 0;
	}
	if (g_vk.descriptorPool) {
		vkDestroyDescriptorPool(g_vk.device, g_vk.descriptorPool, 0);
		g_vk.descriptorPool = 0;
	}
	if (g_vk.commandPool) {
		vkDestroyCommandPool(g_vk.device, g_vk.commandPool, 0);
		g_vk.commandPool = 0;
	}
	if (g_vk.sem.release) {
		vkDestroySemaphore(g_vk.device, g_vk.sem.release, 0);
		g_vk.sem.release = 0;
	}
	if (g_vk.sem.acquire) {
		vkDestroySemaphore(g_vk.device, g_vk.sem.acquire, 0);
		g_vk.sem.acquire = 0;
	}
	if (g_vk.allocator) {
		vmaDestroyAllocator(g_vk.allocator);
		g_vk.allocator = 0;
	}
	if (g_vk.device) {
		vkDestroyDevice(g_vk.device, 0);
		g_vk.device = 0;
	}
	if (g_vk.physicalDevice) {
		g_vk.physicalDevice = 0;
	}
	if (g_vk.debugMessenger) {
		vkDestroyDebugUtilsMessengerEXT(g_vk.instance, g_vk.debugMessenger, 0);
		g_vk.debugMessenger = 0;
	}
	if (g_vk.instance) {
		vkDestroyInstance(g_vk.instance, 0);
		g_vk.instance = 0;
	}
	if (rc == VK_ERROR_INITIALIZATION_FAILED) {
		return "Vulkan is broken or not installed";
	} else if (rc == VK_ERROR_INCOMPATIBLE_DRIVER) {
		return "Required vulkan version is not supported";
	}
	return ssprintf("Vulkan failed to initialize (0x%x)", rc);
}

auto
Display_VK::TryVideoMode(const VideoModeParams& _p, bool& bNewDeviceOut)
  -> std::string
{
	DestroySwapchain(g_vk.swapchain);

	// ?????
#ifdef _WIN32
	GraphicsWindow::CreateGraphicsWindow(_p);
#else
#error todo
#endif

	VkResult rc = VK_SUCCESS;

	// Create surface (platform dependent)
	{
#ifdef VK_USE_PLATFORM_WIN32_KHR
		VkWin32SurfaceCreateInfoKHR surfaceInfo = {
			VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR
		};
		surfaceInfo.hinstance = GetModuleHandle(0);
		surfaceInfo.hwnd = GraphicsWindow::GetHwnd();
		rc = vkCreateWin32SurfaceKHR(
		  g_vk.instance, &surfaceInfo, NULL, &g_vk.surface);
#else
#error todo
#endif

		DEBUG_ASSERT(g_vk.surface);
		if (rc != VK_SUCCESS) {
			goto bail;
		}
	}

	rc = CreateSwapchain(g_vk.swapchain);

	if (rc != VK_SUCCESS) {
		goto bail;
	}

	return std::string{};

bail:
	DestroySwapchain(g_vk.swapchain);
	return ssprintf("Vulkan failed to initialize (0x%x)", rc);
}

void
Display_VK::GetDisplaySpecs(DisplaySpecs& out) const
{
	out.clear();
	// Todo
	// This is for listing display modes in the options menu
}

auto
Display_VK::GetActualVideoModeParams() const -> const ActualVideoModeParams*
{
	// ?????
#ifdef _WIN32
	return GraphicsWindow::GetParams();
#else
#error todo?
#endif
}

void
Display_VK::ResolutionChanged()
{
	RecreateSwapchain(g_vk.swapchain);
}

auto
Display_VK::GetMaxTextureSize() const -> int
{
	return g_vk.properties.limits.maxImageDimension2D;
}

auto
Display_VK::CreateTexture(RagePixelFormat pixfmt,
						  RageSurface* img,
						  bool bGenerateMipMaps) -> intptr_t
{
	return 1;
}

void
Display_VK::DeleteTexture(intptr_t iTexHandle)
{
}

auto
Display_VK::BeginFrame() -> bool
{
	// ?????
#ifdef _WIN32
	GraphicsWindow::Update();
#else
#error todo?
#endif

	VkResult rc = vkAcquireNextImageKHR(g_vk.device,
										g_vk.swapchain.swapchain,
										~0ULL,
										g_vk.sem.acquire,
										VK_NULL_HANDLE,
										&g_vk.frameImageIndex);
	DEBUG_ASSERT(rc == VK_SUCCESS);

	if (rc != VK_SUCCESS) {
		return false;
	}

	rc = vkResetCommandPool(g_vk.device, g_vk.commandPool, 0);
	DEBUG_ASSERT(rc == VK_SUCCESS);

	if (rc != VK_SUCCESS) {
		return false;
	}

	VkCommandBufferBeginInfo cmdBeginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
	};
	cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	rc = vkBeginCommandBuffer(g_vk.commandBuffer, &cmdBeginInfo);
	DEBUG_ASSERT(rc == VK_SUCCESS);

	if (rc != VK_SUCCESS) {
		return false;
	}

	VkClearColorValue color = { .1f, 0, .1f, 1 };
	VkClearValue clearValue = { color };

	VkRenderPassBeginInfo passBeginInfo = {
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO
	};
	passBeginInfo.renderPass = g_vk.swapchain.renderPass;
	passBeginInfo.framebuffer =
	  g_vk.swapchain.framebuffers[g_vk.frameImageIndex];
	passBeginInfo.renderArea.extent = g_vk.swapchain.dim;
	passBeginInfo.clearValueCount = 1;
	passBeginInfo.pClearValues = &clearValue;

	vkCmdBeginRenderPass(
	  g_vk.commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(g_vk.commandBuffer,
					  VK_PIPELINE_BIND_POINT_GRAPHICS,
					  g_vk.swapchain.pipelines[0]);

	return RageDisplay::BeginFrame();
}

void
Display_VK::EndFrame()
{
	VkResult rc = VK_SUCCESS;

	if (g_vk.quads.quads.size() > 0) {
		DEBUG_ASSERT((g_vk.quads.quads.size() * 6) ==
					 g_vk.quads.indices.size());
		memcpy(g_vk.quads.quadsBuffer.mappedMemory,
			   g_vk.quads.quads.data(),
			   g_vk.quads.quads.size() * sizeof(QuadsProgram::Quad));
		memcpy(g_vk.quads.indexBuffer.mappedMemory,
			   g_vk.quads.indices.data(),
			   g_vk.quads.indices.size() * sizeof(uint16_t));

		vkCmdBindDescriptorSets(g_vk.commandBuffer,
								VK_PIPELINE_BIND_POINT_GRAPHICS,
								g_vk.quads.pipelineLayout,
								0,
								1,
								&g_vk.quads.descriptorSet,
								0,
								0);
		vkCmdBindIndexBuffer(g_vk.commandBuffer,
							 g_vk.quads.indexBuffer.buffer,
							 0,
							 VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(
		  g_vk.commandBuffer, uint32_t(g_vk.quads.indices.size()), 1, 0, 0, 0);

		g_vk.quads.quads.clear();
		g_vk.quads.indices.clear();
	}

	vkCmdEndRenderPass(g_vk.commandBuffer);
	vkEndCommandBuffer(g_vk.commandBuffer);

	{
		VkPipelineStageFlags submitStageMask =
		  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &g_vk.sem.acquire;
		submitInfo.pWaitDstStageMask = &submitStageMask;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &g_vk.commandBuffer;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &g_vk.sem.release;
		rc = vkQueueSubmit(g_vk.queue, 1, &submitInfo, VK_NULL_HANDLE);
		DEBUG_ASSERT(rc == VK_SUCCESS);

		if (rc != VK_SUCCESS) {
			goto bail;
		}

		VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &g_vk.sem.release;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &g_vk.swapchain.swapchain;
		presentInfo.pImageIndices = &g_vk.frameImageIndex;

		// Can use RageDisplay's frame pacing stuff
		// TODO: find out if Vulkan has better stats for this stuff
		FrameLimitBeforeVsync();

		const auto beforePresent = std::chrono::steady_clock::now();
		rc = vkQueuePresentKHR(g_vk.queue, &presentInfo);
		DEBUG_ASSERT(rc == VK_SUCCESS);

		if (rc != VK_SUCCESS) {
			goto bail;
		}

		vkQueueWaitIdle(g_vk.queue);
		const auto afterPresent = std::chrono::steady_clock::now();
		SetPresentTime(afterPresent - beforePresent);
		FrameLimitAfterVsync(GetActualVideoModeParams()->rate);
	}

bail:
	return RageDisplay::EndFrame();
}

void
Display_VK::ClearZBuffer()
{
}

void
Display_VK::SetAlphaTest(bool b)
{
}

void
Display_VK::SetBlendMode(BlendMode mode)
{
}

void
Display_VK::SetCullMode(CullMode mode)
{
}

void
Display_VK::SetZBias(float f)
{
}

void
Display_VK::SetZWrite(bool b)
{
}

void
Display_VK::SetZTestMode(ZTestMode mode)
{
}

void
Display_VK::SetTexture(TextureUnit tu, intptr_t iTexture)
{
}

void
Display_VK::SetTextureMode(TextureUnit tu, TextureMode tm)
{
}

void
Display_VK::SetTextureFiltering(TextureUnit tu, bool b)
{
}

void
Display_VK::SetTextureWrapping(TextureUnit tu, bool b)
{
}

void
Display_VK::DrawQuadsInternal(const RageSpriteVertex v[], int iNumVerts)
{
}

void
Display_VK::DrawQuadStripInternal(const RageSpriteVertex v[], int iNumVerts)
{
}

void
Display_VK::DrawSymmetricQuadStripInternal(const RageSpriteVertex v[],
										   int iNumVerts)
{
}

void
Display_VK::DrawFanInternal(const RageSpriteVertex v[], int iNumVerts)
{
}

void
Display_VK::DrawStripInternal(const RageSpriteVertex v[], int iNumVerts)
{
}

void
Display_VK::DrawTrianglesInternal(const RageSpriteVertex v[], int iNumVerts)
{
}

auto
Display_VK::IsZWriteEnabled() const -> bool
{
	FAIL_M("Never called?");
}

auto
Display_VK::IsZTestEnabled() const -> bool
{
	FAIL_M("Never called?");
}

void
Display_VK::PushQuads(RenderQuad q[], size_t numQuads)
{
	if ((g_vk.quads.quads.size() + numQuads) * sizeof(QuadsProgram::Quad) >
		Buffer::Size) {
		DEBUG_ASSERT(!"Hit max quad limit");
		// Todo: log
		return;
	}

	size_t quadsOffset = g_vk.quads.quads.size();
	size_t indicesOffset = g_vk.quads.indices.size();

	g_vk.quads.quads.resize(quadsOffset + numQuads);
	g_vk.quads.indices.resize(indicesOffset + 6 * numQuads);

	ASSERT(g_vk.quads.indices.size() * sizeof(uint16_t) < Buffer::Size);

	for (size_t i = 0; i < numQuads; i++) {
		g_vk.quads.quads[quadsOffset + i].rect = q[i].rect;
		g_vk.quads.quads[quadsOffset + i].tex = q[i].texCoords;
		memcpy(g_vk.quads.quads[quadsOffset + i].colors,
			   q[i].colors,
			   sizeof(q[i].colors));
	}

	uint16_t indices_index = quadsOffset * 4;
	for (size_t i = 0; i < numQuads * 6; i += 6, indices_index += 4) {
		g_vk.quads.indices[indicesOffset + i + 0] = indices_index + 0;
		g_vk.quads.indices[indicesOffset + i + 1] = indices_index + 1;
		g_vk.quads.indices[indicesOffset + i + 2] = indices_index + 2;
		g_vk.quads.indices[indicesOffset + i + 3] = indices_index + 2;
		g_vk.quads.indices[indicesOffset + i + 4] = indices_index + 3;
		g_vk.quads.indices[indicesOffset + i + 5] = indices_index + 0;
	}
}

auto
Display_VK::SupportsThreadedRendering() -> bool
{
	return false;
}

auto
Display_VK::CreateScreenshot() -> RageSurface*
{
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Needs to be supported for movie bgs

void
Display_VK::UpdateTexture(intptr_t uTexHandle,
						  RageSurface* img,
						  int xoffset,
						  int yoffset,
						  int width,
						  int height)
{
	FAIL_M("Not implemented");
}

auto
Display_VK::CreateRenderTarget(const RenderTargetParam& param,
							   int& iTextureWidthOut,
							   int& iTextureHeightOut) -> intptr_t
{
	FAIL_M("Not implemented");
}

auto
Display_VK::GetRenderTarget() -> intptr_t
{
	FAIL_M("Not implemented");
	return 0;
}

void
Display_VK::SetRenderTarget(intptr_t uTexHandle, bool bPreserveTexture)
{
	FAIL_M("Not implemented");
}

auto
Display_VK::GetPixelFormatDesc(RagePixelFormat pf) const
  -> const RagePixelFormatDesc*
{
	FAIL_M("Not implemented");
	return 0;
}

auto
Display_VK::SupportsTextureFormat(RagePixelFormat pixfmt, bool realtime) -> bool
{
	RagePixelFormat whateverTheHardwareLikes = RagePixelFormat_Invalid;
	switch (g_vk.swapchain.format) {
		case VK_FORMAT_R8G8B8A8_UNORM: {
			whateverTheHardwareLikes = RagePixelFormat_RGBA8;
		} break;
		case VK_FORMAT_B8G8R8A8_UNORM: {
			whateverTheHardwareLikes = RagePixelFormat_BGRA8;
		} break;
	}
	return pixfmt == whateverTheHardwareLikes;
}

///////////////////////////////////////////////////////////////////////////////
// Should probably be supported or the functionality removed from the rest of
// the code

auto
Display_VK::CreateCompiledGeometry() -> RageCompiledGeometry*
{
	FAIL_M("Never called?");
	return 0;
}

void
Display_VK::DeleteCompiledGeometry(RageCompiledGeometry* p)
{
	FAIL_M("Never called?");
}

void
Display_VK::DrawCompiledGeometryInternal(const RageCompiledGeometry* p,
										 int iMeshIndex)
{
	FAIL_M("Never called?");
}

void
Display_VK::ClearAllTextures()
{
	FAIL_M("Never called?");
}

auto
Display_VK::GetNumTextureUnits() -> int
{
	FAIL_M("Never called?");
}

///////////////////////////////////////////////////////////////////////////////
// Will never be supported, only make sense for ancient APIs

void
Display_VK::SetMaterial(const RageColor& emissive,
						const RageColor& ambient,
						const RageColor& diffuse,
						const RageColor& specular,
						float shininess)
{
	FAIL_M("Never called?");
}

void
Display_VK::SetLighting(bool b)
{
	ASSERT(b == false);
}

void
Display_VK::SetLightOff(int index)
{
}

void
Display_VK::SetLightDirectional(int index,
								const RageColor& ambient,
								const RageColor& diffuse,
								const RageColor& specular,
								const RageVector3& dir)
{
}

void
Display_VK::SetSphereEnvironmentMapping(TextureUnit tu, bool b)
{
	FAIL_M("Never called?");
}

void
Display_VK::SetCelShaded(int stage)
{
}

auto
Display_VK::IsD3DInternal() -> bool
{
	FAIL_M("Never called?");
}
