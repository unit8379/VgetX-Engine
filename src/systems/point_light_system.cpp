#include "point_light_system.hpp"

// libs
#define GLM_FORCE_RADIANS			  // Функции GLM будут работать с радианами, а не градусами
#define GLM_FORCE_DEPTH_ZERO_TO_ONE   // GLM будет ожидать интервал нашего буфера глубины от 0 до 1 (например, для OpenGL используется интервал от -1 до 1)
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <stdexcept>
#include <cassert>
#include <array>
#include <map>

namespace vget
{
	struct PointLightPushConstants
	{
		glm::vec4 position{};
		glm::vec4 color{};
		float radius{};
	};

	PointLightSystem::PointLightSystem(VgetDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
		: vgetDevice{device}
	{
		createPipelineLayout(globalSetLayout);
		createPipeline(renderPass);
	}

	PointLightSystem::~PointLightSystem()
	{
		vkDestroyPipelineLayout(vgetDevice.device(), pipelineLayout, nullptr);
	}

	void PointLightSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
	{
		// описание диапазона пуш-констант
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // доступ к данным констант из обоих шейдеров
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(PointLightPushConstants);

		std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout}; // вектор используемых схем для наборов дескрипторов

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

	void PointLightSystem::createPipeline(VkRenderPass renderPass)
	{
		assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

		PipelineConfigInfo pipelineConfig{};
		VgetPipeline::defaultPipelineConfigInfo(pipelineConfig);
		VgetPipeline::enableAlphaBlending(pipelineConfig);
		pipelineConfig.bindingDescriptions.clear();   // массивы с привязками и атрибутами буфера вершин очищаем, т.к.
		pipelineConfig.attributeDescriptions.clear(); // они не нужны в PointLightSystem с билбордами
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;

		vgetPipeline = std::make_unique<VgetPipeline>(
			vgetDevice,
			"./shaders/point_light.vert.spv",
			"./shaders/point_light.frag.spv",
			pipelineConfig);
	}

	void PointLightSystem::update(FrameInfo& frameInfo, GlobalUbo& ubo)
	{
		// матрица преобразования для вращения объектов точечного света
		auto rotateLight = glm::rotate(
			glm::mat4(1.f),	 // инициализируем единичную матрицу
			frameInfo.frameTime, // угол положения объекта изменяется пропорционально прошедшему между кадрами времени
			{0.f, -1.f, 0.f} // ось вращения (y == -1, значит вращение вокруг Up-вектора)
		);

		int lightIndex = 0;
		for (auto& kv : frameInfo.gameObjects)
		{
			auto& obj = kv.second;
			if (obj.pointLight == nullptr) continue;

			assert(lightIndex < MAX_LIGHTS && "Point Lights exceed maximum specified");

			// обновление позиции PointLight'а в карусели
			obj.transform.translation = glm::vec3(rotateLight * glm::vec4(obj.transform.translation, 1.f));

			// копируем текущие данные об объекте Point Light'а в Ubo структуру
			ubo.pointLights[lightIndex].position = glm::vec4(obj.transform.translation, 1.f);
			ubo.pointLights[lightIndex].color = glm::vec4(obj.color, obj.pointLight->lightIntensity);

			lightIndex += 1;
		}

		ubo.numLights = lightIndex;
	}

	void PointLightSystem::render(FrameInfo& frameInfo)
	{
		// Автоматическа сортировка PointLight'ов в мапе по их дистанции до камеры.
		// Это нужно для поочерёдного порядка их отрисовки, начиная с дальних билбордов,
		// а затем для их дальнейшего правильного смешивания цветов в ColorBlend этапе.
		std::map<float, VgetGameObject::id_t> sorted;
		for (auto& kv : frameInfo.gameObjects)
		{
			auto& obj = kv.second;
			if (obj.pointLight == nullptr) continue;

			// вычисление дистанции до камеры
			auto offset = frameInfo.camera.getPosition() - obj.transform.translation;
			float disSquared = glm::dot(offset, offset);
			sorted[disSquared] = obj.getId();
		}

		// render objects
		vgetPipeline->bind(frameInfo.commandBuffer);  // прикрепление графического пайплайна к буферу команд

		// привязываем набор дескрипторов к пайплайну
		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			0,
			1,
			&frameInfo.globalDescriptorSet,
			0,
			nullptr
		);

		// Отрисовываем билборды поинт лайтов в обратном порядке (от самого дальнего, до самого близкого к камере)
		for (auto it = sorted.rbegin(); it != sorted.rend(); ++it)
		{
			auto& obj = frameInfo.gameObjects.at(it->second);

			PointLightPushConstants push{};
			push.position = glm::vec4(obj.transform.translation, 1.f);
			push.color = glm::vec4(obj.color, obj.pointLight->lightIntensity);
			push.radius = obj.transform.scale.x;

			vkCmdPushConstants(
				frameInfo.commandBuffer,
				pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(PointLightPushConstants),
				&push
			);
			vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
		}
	}
}