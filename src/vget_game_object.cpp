﻿#include "vget_game_object.hpp"

namespace vget
{
	glm::mat4 TransformComponent::mat4()
	{
		// Ниже представлено оптимизированное создание матрицы афинных преобразований.
		// Она конструируется по столбцам. Первые три столбца это линейные преобразования,
		// а четвёртый столбец - вектор для сдвига объекта (translation).
		// Выражения для поворота по углам Эйлера (YXZ последовательность Тейта-Брайана)
		// взяты из википедии. Эти выражения получены после перемножения матриц всех трёх элементарных вращений.
		const float c3 = glm::cos(rotation.z);
	    const float s3 = glm::sin(rotation.z);
	    const float c2 = glm::cos(rotation.x);
	    const float s2 = glm::sin(rotation.x);
	    const float c1 = glm::cos(rotation.y);
	    const float s1 = glm::sin(rotation.y);
	    return glm::mat4{
	        {
	            scale.x * (c1 * c3 + s1 * s2 * s3),
	            scale.x * (c2 * s3),
	            scale.x * (c1 * s2 * s3 - c3 * s1),
	            0.0f,
	        },
	        {
	            scale.y * (c3 * s1 * s2 - c1 * s3),
	            scale.y * (c2 * c3),
	            scale.y * (c1 * c3 * s2 + s1 * s3),
	            0.0f,
	        },
	        {
	            scale.z * (c2 * s1),
	            scale.z * (-s2),
	            scale.z * (c1 * c2),
	            0.0f,
	        },
	        {translation.x, translation.y, translation.z, 1.0f}};
	}

	glm::mat3 TransformComponent::normalMatrix()
	{
		const float c3 = glm::cos(rotation.z);
	    const float s3 = glm::sin(rotation.z);
	    const float c2 = glm::cos(rotation.x);
	    const float s2 = glm::sin(rotation.x);
	    const float c1 = glm::cos(rotation.y);
	    const float s1 = glm::sin(rotation.y);

		// Матрица масштабирования должна быть обратная
		const glm::vec3 invScale = 1.0f / scale;

		// В отличие от матрицы преобр. для вершин, здесь нет операции сдвига, так как он
		// не влияет на нормали. Следовательно, матрица сократилась до 3x3.
	    return glm::mat3{
	        {
	            invScale.x * (c1 * c3 + s1 * s2 * s3),
	            invScale.x * (c2 * s3),
	            invScale.x * (c1 * s2 * s3 - c3 * s1),
	        },
	        {
	            invScale.y * (c3 * s1 * s2 - c1 * s3),
	            invScale.y * (c2 * c3),
	            invScale.y * (c1 * c3 * s2 + s1 * s3),
	        },
	        {
	            invScale.z * (c2 * s1),
	            invScale.z * (-s2),
	            invScale.z * (c1 * c2),
	        }
		};
	}

	VgetGameObject VgetGameObject::makePointLight(float intensity, float radius, glm::vec3 color)
	{
		VgetGameObject gameObj = VgetGameObject::createGameObject("PointLight");
		gameObj.color = color;
		gameObj.transform.scale.x = radius;  // радиус видимого билборда сохраняется в X-компоненту scale'а
		gameObj.pointLight = std::make_unique<PointLightComponent>();
		gameObj.pointLight->lightIntensity = intensity;
		return gameObj;
	}
}