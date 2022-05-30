#include "simple_render_system.hpp"
#include "../vget_buffer.hpp"

// libs
#define GLM_FORCE_RADIANS			  // Функции GLM будут работать с радианами, а не градусами
#define GLM_FORCE_DEPTH_ZERO_TO_ONE   // GLM будет ожидать интервал нашего буфера глубины от 0 до 1 (например, для OpenGL используется интервал от -1 до 1)
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <stdexcept>
#include <cassert>
#include <array>
#include <iostream>

namespace vget
{
	// структура пуш-константы здесь объявлена временно
	struct SimplePushConstantData
	{
		glm::mat4 modelMatrix{ 1.f }; // такой конструктор создаёт единичную матрицу
		glm::mat4 normalMatrix{1.f};
		// далее идёт превышение минимально возможного размера данных для пуш константы => в потенциале нужно исправить этот момент
		int textureIndex;
		alignas(16) glm::vec3 diffuseColor{};
	};

	SimpleRenderSystem::SimpleRenderSystem(VgetDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout, FrameInfo frameInfo)
		: vgetDevice{ device }
	{
		createUboBuffers();
		fillModelsIds(frameInfo.gameObjects);
		createDescriptorSets(frameInfo);
		createPipelineLayout(globalSetLayout);
		createPipeline(renderPass);
	}

	SimpleRenderSystem::~SimpleRenderSystem()
	{
		vkDestroyPipelineLayout(vgetDevice.device(), pipelineLayout, nullptr);
	}

	void SimpleRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
	{
		// описание диапазона пуш-констант
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // доступ к данным констант из обоих шейдеров
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(SimplePushConstantData);

		// вектор используемых схем для наборов дескрипторов
		std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout, simpleSystemSetLayout->getDescriptorSetLayout()};

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		// DescriptorSetLayout объекты используются для передачи данных, отличных от данных о вершинах, в шейдеры.
		// Это могут быть текстуры или Uniform Buffer объекты.
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
		pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
		// PushConstant'ы используются для передачи в шейдерные программы небольшого количества данных.
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(vgetDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline layout!");
		}
	}

	void SimpleRenderSystem::createPipeline(VkRenderPass renderPass)
	{
		assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

		PipelineConfigInfo pipelineConfig{};
		VgetPipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;

		vgetPipeline = std::make_unique<VgetPipeline>(
			vgetDevice,
			"./shaders/simple_shader.vert.spv",
			"./shaders/simple_shader.frag.spv",
			pipelineConfig);
	}

	void SimpleRenderSystem::createUboBuffers()
	{
		for (int i = 0; i < uboBuffers.size(); ++i)
		{
			uboBuffers[i] = std::make_unique<VgetBuffer>(
				vgetDevice,
				sizeof(SimpleSystemUbo),
				1,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
				);
			uboBuffers[i]->map();
		}
	}

	int SimpleRenderSystem::fillModelsIds(VgetGameObject::Map& gameObjects)
	{
		modelObjectsIds.clear();
		for (auto& kv : gameObjects)
		{
			auto& obj = kv.second; // ссылка на объект из мапы
			if (obj.model != nullptr) {
				modelObjectsIds.push_back(kv.first); // в этой системе рендерятся только объекты с моделью
			}
		}
		return static_cast<int>(modelObjectsIds.size());
	}

	void SimpleRenderSystem::createDescriptorSets(FrameInfo& frameInfo)
	{
		int texturesCount = 0;
		std::vector<VkDescriptorImageInfo> descriptorImageInfos;

		for (auto& id : modelObjectsIds)
		{
			texturesCount += frameInfo.gameObjects[id].model->getTextures().size();

			// Заполнение инфорамации по дескрипторам текстур для каждой модели
			// todo сделать рефактор
			for (auto& texture : frameInfo.gameObjects.at(id).model->getTextures())
			{
				if (texture != nullptr)
				{
					auto& imageInfo = texture->descriptorInfo();
					descriptorImageInfos.push_back(imageInfo);
				}
				else
				{
					// todo: добавление объектов не будет работать, пока не будет исправлен этот момент
					descriptorImageInfos.push_back(frameInfo.gameObjects.at(id).model->getTextures().at(0)->descriptorInfo());
				}
			}
		}

		simpleSystemPool = VgetDescriptorPool::Builder(vgetDevice)
			.setMaxSets(VgetSwapChain::MAX_FRAMES_IN_FLIGHT)
			.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VgetSwapChain::MAX_FRAMES_IN_FLIGHT)
			.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VgetSwapChain::MAX_FRAMES_IN_FLIGHT * texturesCount)
			//.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VgetSwapChain::MAX_FRAMES_IN_FLIGHT * 1000)
			.build();

		simpleSystemSetLayout = VgetDescriptorSetLayout::Builder(vgetDevice)
			.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, texturesCount)
			//.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1000)
			.build();

		for (int i = 0; i < simpleSystemSets.size(); ++i)
		{
			// Запись количества текстур для каждого ubo буфера
			SimpleSystemUbo ubo{};
			ubo.texturesCount = texturesCount;
			uboBuffers[i]->writeToBuffer(&ubo);

			auto bufferInfo = uboBuffers[i]->descriptorInfo();

			VgetDescriptorWriter(*simpleSystemSetLayout, *simpleSystemPool)
				.writeBuffer(0, &bufferInfo)
				.writeImage(1, descriptorImageInfos.data(), texturesCount)
				.build(simpleSystemSets[i]);
		}
	}

	void SimpleRenderSystem::update(FrameInfo& frameInfo, SimpleSystemUbo& ubo)
	{
		uboBuffers[frameInfo.frameIndex]->writeToBuffer(&ubo);
	}

	void SimpleRenderSystem::renderGameObjects(FrameInfo& frameInfo)
	{
		vgetPipeline->bind(frameInfo.commandBuffer);  // прикрепление графического пайплайна к буферу команд

		// Заполняется вектор id'шников объектов с моделью и
		// если их кол-во изменилось, то наборы дескрипторов с текстурами для этих
		// объектов пересоздаются.
		if (prevModelCount != fillModelsIds(frameInfo.gameObjects)) {
			createDescriptorSets(frameInfo);
		}
		prevModelCount = modelObjectsIds.size();

		std::vector<VkDescriptorSet> descriptorSets{ frameInfo.globalDescriptorSet, simpleSystemSets[frameInfo.frameIndex] };
		// Привязываем наборы дескрипторов к пайплайну
		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			0,
			2,
			descriptorSets.data(),
			0,
			nullptr
		);

		int textureIndexOffset = 0; // отступ в массиве текстур для текущего объекта
		for (auto& id : modelObjectsIds)
		{
			auto& obj = frameInfo.gameObjects[id];

			SimplePushConstantData push{};
			push.modelMatrix = obj.transform.mat4();
			push.normalMatrix = obj.transform.normalMatrix();

			// Отрисовка каждого подобъекта .obj модели по отдельности с передачей своего индекса текстуры
			for (auto& info : obj.model->getSubObjectsInfo())
			{
				// Передача в пуш константу индекса текстуры. Если её нет у данного подобъекта, то будет передано -1.
				if (obj.model->getTextures().at(info.textureIndex) != nullptr)
					push.textureIndex = textureIndexOffset + info.textureIndex;
				else
				{
					push.textureIndex = -1;
					push.diffuseColor = info.diffuseColor;	
				}

				vkCmdPushConstants(
				frameInfo.commandBuffer,
				pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(SimplePushConstantData),
				&push);

				// прикрепление буфера вершин (модели) и буфера индексов к буферу команд (создание привязки)
				obj.model->bind(frameInfo.commandBuffer);
				// отрисовка буфера вершин
				obj.model->drawIndexed(frameInfo.commandBuffer, info.indexCount, info.indexStart);
			}
			textureIndexOffset += obj.model->getTextures().size();
		}
	}
}