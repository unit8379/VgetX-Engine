#include "vget_model.hpp"
#include "vget_utils.hpp"

// libs
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

// std
#include <cassert>
#include <cstring>
#include <iostream>
#include <unordered_map>

#ifndef ENGINE_DIR
#define ENGINE_DIR "../"
#endif

namespace std
{
	template<>
	struct hash<vget::VgetModel::Vertex>
	{
		size_t operator()(vget::VgetModel::Vertex const& vertex) const
		{
			size_t seed = 0;
			vget::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
			return seed;
		}
	};
}

namespace vget
{
	VgetModel::VgetModel(VgetDevice& device, const VgetModel::Builder& builder) : vgetDevice{device}
	{
		createVertexBuffers(builder.vertices);
		createIndexBuffers(builder.indices);
	}

	VgetModel::~VgetModel() {}

	std::unique_ptr<VgetModel> VgetModel::createModelFromFile(VgetDevice& device, const std::string& filepath)
	{
		Builder builder{};
		builder.loadModel(ENGINE_DIR + filepath);
		std::cout << "Vertex count: " << builder.vertices.size() << "\n";
		return std::make_unique<VgetModel>(device, builder);
	}

	void VgetModel::createVertexBuffers(const std::vector<Vertex>& vertices)
	{
		vertexCount = static_cast<uint32_t>(vertices.size());
		assert(vertexCount >= 3 && "Vertex count must be at least 3");
		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;
		uint32_t vertexSize = sizeof(vertices[0]);

		// Создание промежуточного буфера
		VgetBuffer stagingBuffer
		{
			vgetDevice,
			vertexSize,
			vertexCount,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // буфер используется как источник для операции переноса памяти
			// HOST_VISIBLE флаг указывает на то, что хост (CPU) будет иметь доступ к размещённой в девайсе (GPU) памяти.
			// Это важно для получения возможности писать данные в память GPU.
			// HOST_COHERENT флаг включает полное соответствие памяти хоста и девайса. Это даёт возможность легко
			// передавать изменения из памяти CPU в память GPU.
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		};

		stagingBuffer.map();
		stagingBuffer.writeToBuffer((void*)vertices.data());

		// Создание буфера для данных о вершинах в локальной памяти девайса
		vertexBuffer = std::make_unique<VgetBuffer>(
			vgetDevice,
			vertexSize,
			vertexCount,
			// Буфер используется для входных данных вершин, а данные для него будут перенесены из другого источника (из промежуточного буфера)
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			// DEVICE_LOCAL флаг указывает на то, что данный буфер будет размещён в оптимальной и быстрой локальной памяти девайса
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		// Функция, записывающая команду копирования в командный буфер,
		// который сразу же отправляется в очередь на выполнение.
		vgetDevice.copyBuffer(stagingBuffer.getBuffer(), vertexBuffer->getBuffer(), bufferSize);
	}

	void VgetModel::createIndexBuffers(const std::vector<uint32_t>& indices)
	{
		indexCount = static_cast<uint32_t>(indices.size());
		hasIndexBuffer = indexCount > 0;
		if (!hasIndexBuffer) return;

		VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;
		uint32_t indexSize = sizeof(indices[0]);

		// Создание промежуточного буфера
		VgetBuffer stagingBuffer {
			vgetDevice,
			indexSize,
			indexCount,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		};

		// Маппинг памяти из девайса и передача туда данных по аналогии со staging буфером из createVertexBuffers()
		stagingBuffer.map();
		stagingBuffer.writeToBuffer((void*)indices.data());

		// Создание буфера для индексов в локальной памяти девайса
		indexBuffer = std::make_unique<VgetBuffer>(
			vgetDevice,
			indexSize,
			indexCount,
			// Буфер используется для индексов, а данные для него будут перенесены из другого источника (из промежуточного буфера)
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		vgetDevice.copyBuffer(stagingBuffer.getBuffer(), indexBuffer->getBuffer(), bufferSize);
	}

	void VgetModel::draw(VkCommandBuffer commandBuffer)
	{
		if (hasIndexBuffer)
		{
			// Запись команды на отрисовку с применением буфера индексов
			vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
		}
		else
		{
			// Запись команды на отрисовку. (vertexCount вершин, 1 экземпляр, без смещений)
			vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
		}
	}

	void VgetModel::bind(VkCommandBuffer commandBuffer)
	{
		VkBuffer buffers[] = { vertexBuffer->getBuffer() };
		VkDeviceSize offsets[] = { 0 };

		// Запись команды в буфер команд о создании привязки буфера вершин к пайплайну.
		// После выполнения данная команда создаст Binding[0] для одного буфера вершин из buffers с отступом offsets внутри этого буфера.
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

		if (hasIndexBuffer)
		{
			// Команда создания привязки буфера индексов (если он есть) к пайплайну.
			// Тип индекса должен совпадать с типом данных в самом буфере и может выбираться
			// поменьше для экономии памяти при использовании простых моделей объектов.
			vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
		}
	}

	std::vector<VkVertexInputBindingDescription> VgetModel::Vertex::getBindingDescriptions()
	{
		// Создание описания привязки для буфера вершин.
		std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
		bindingDescriptions[0].binding = 0;										// индекс занимаемой буфером привязки
		bindingDescriptions[0].stride = sizeof(Vertex);							// вершины расположены с шагом в sizeof(Vertex) байт
		bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescriptions;
	}

	std::vector<VkVertexInputAttributeDescription> VgetModel::Vertex::getAttributeDescriptions()
	{
		// Создание описания атрибута внутри буфера вершин
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
		
		// position attribute
		VkVertexInputAttributeDescription attribDescription{};
		attribDescription.binding = 0;								// атрибут расположен в привязке с индексом 0
		attribDescription.location = 0;								// Номер location'а в шейдере вершин для заданного атрибута
		attribDescription.format = VK_FORMAT_R32G32B32_SFLOAT;		// Тип данных данного атрибута
		attribDescription.offset = offsetof(Vertex, position);		// Отступ от начала буфера до данного атрибута
		attributeDescriptions.push_back(attribDescription);

		// Иницизализация оставшихся описаний атрибутов представлена в сжатой форме
		attributeDescriptions.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)});
		attributeDescriptions.push_back({2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)});
		attributeDescriptions.push_back({3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)});

		return attributeDescriptions;
	}

	void VgetModel::Builder::loadModel(const std::string& filepath)
	{
		tinyobj::attrib_t attrib;						// содержит данные позиций, цветов, нормалей и координат текстур
		std::vector<tinyobj::shape_t> shapes;			// shapes хранит значения индексов для каждого из face элементов
		std::vector<tinyobj::material_t> materials;		// materials хранит данные материалах
		std::string warn, err;

		// После успешного выполнения функции LoadObj() переданные локальные переменный заполнятся
		// данными из предоставленного .obj файла
		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str()))
		{
			throw std::runtime_error(warn + err);
		}

		// очистка текщей структуры перед загрузкой новой модели
		vertices.clear();
		indices.clear();

		// Мапа хранит уникальные вершины с их индексами. С её помощью составляется буфер индексов.
		std::unordered_map<Vertex, uint32_t> uniqueVertices{};

		// Проходимся по каждому face элементу модели
		for (const auto &shape : shapes)
		{
			// Проходимся по индексам каждой из вершин примитива
			for (const auto &index : shape.mesh.indices)
			{
				Vertex vertex{};

				if (index.vertex_index >= 0) // отрицательный индекс означает, что позиция не была предоставлена
				{
					// с помощью текущего индекса позиции извлекаем из атрибутов позицию вершины
					vertex.position = {
						attrib.vertices[3 * index.vertex_index + 0], // x
						attrib.vertices[3 * index.vertex_index + 1], // y
						attrib.vertices[3 * index.vertex_index + 2], // z
					};

					// по таким же индексам из атрибутов извелкается цвет вершины, если он был представлен в файле
					vertex.color = {
						attrib.colors[3 * index.vertex_index + 0], // r
						attrib.colors[3 * index.vertex_index + 1], // g
						attrib.colors[3 * index.vertex_index + 2], // b
					};
				}

				// извлекаем из атрибутов позицию нормали
				if (index.normal_index >= 0)
				{
					vertex.normal = {
						attrib.normals[3 * index.normal_index + 0], // x
						attrib.normals[3 * index.normal_index + 1], // y
						attrib.normals[3 * index.normal_index + 2], // z
					};
				}

				// извлекаем из атрибутов координаты текстуры
				if (index.texcoord_index >= 0)
				{
					vertex.uv = {
						attrib.texcoords[2 * index.texcoord_index + 0], // u
						attrib.texcoords[2 * index.texcoord_index + 1], // v
					};
				}

				// Если считанная вершина не найдена в мапе, то она добавляется в неё и получает
				// свой индекс, а затем добавляется в вектор builder'а
				if (uniqueVertices.count(vertex) == 0)
				{
					uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
					vertices.push_back(vertex);
				}
				indices.push_back(uniqueVertices[vertex]); // Буфер индексов добавляет индекс считанной вершины
			}
		}
	}

}