#pragma once

#include "vget_device.hpp"

// std
#include <string>
#include <vector>

namespace vget
{
	// Структура для хранения данных для конфигурирования пайплайна. Структура доступна
	// слою приложения, чтобы в его коде можно было конфигурировать графический пайплайн полностью.
	struct PipelineConfigInfo
	{
		PipelineConfigInfo() = default;

		// "resource acquisition is initialization"
		PipelineConfigInfo(const PipelineConfigInfo&) = delete;
		PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;

		std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

		VkPipelineViewportStateCreateInfo viewportInfo;			   // информация об области просмотра
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;  // информация по этапу входной сборки "Input Assembly"
		VkPipelineRasterizationStateCreateInfo rasterizationInfo;  // информация об этапе растеризации
		VkPipelineMultisampleStateCreateInfo multisampleInfo;	   // информация о этапе мультисемплирования
		VkPipelineColorBlendAttachmentState colorBlendAttachment;
		VkPipelineColorBlendStateCreateInfo colorBlendInfo;
		VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
		std::vector<VkDynamicState> dynamicStateEnables;
		VkPipelineDynamicStateCreateInfo dynamicStateInfo;
		VkPipelineLayout pipelineLayout = nullptr;
		VkRenderPass renderPass = nullptr;				// определяет структуру Frame Buffer (состав его вложений (attachments))
		uint32_t subpass = 0;
	};

	class VgetPipeline
	{
	public:
		VgetPipeline(
			VgetDevice& device,
			const std::string& vertFilepath,
			const std::string& fragFilepath,
			const PipelineConfigInfo& configInfo);

		~VgetPipeline();

		// принцип "resource acquisition is initialization"
		VgetPipeline(const VgetPipeline&) = delete;
		VgetPipeline& operator=(const VgetPipeline&) = delete;

		void bind(VkCommandBuffer commandBuffer);

		static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo);
		static void enableAlphaBlending(PipelineConfigInfo& configInfo);

	private:
		static std::vector<char> readFile(const std::string& filepath);

		void createGraphicsPipeline(const std::string& vertFilepath, const std::string& fragFilepath, const PipelineConfigInfo& configInfo);

		void createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);

		VgetDevice& vgetDevice;				// девайс
		VkPipeline graphicsPipeline;		// Vulkan Graphics Pipeline (это указатель, т.к. тип создан через typedef)
		VkShaderModule vertShaderModule;	// шейдерный модуль для шейдера вершины (это указатель, т.к. тип создан через typedef)
		VkShaderModule fragShaderModule;	// шейдерный модуль для шейдера фрагмента (это указатель, т.к. тип создан через typedef)
	};
} // namespace vget