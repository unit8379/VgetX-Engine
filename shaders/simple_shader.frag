#version 450

// Входные интерполированные от трёх вершин переменные. Location и тип данных должны совпадать с выходными переменными из шейдера вершин.
layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec3 fragPosWorld;
layout (location = 2) in vec3 fragNormalWorld;
layout (location = 3) in vec2 fragUv;

layout (location = 0) out vec4 outColor;

struct PointLight {
	vec4 position; // w - игнорируется
	vec4 color;    // w - интенсивность цвета
};

layout(set = 0, binding = 0) uniform GlobalUBO {
	mat4 projection;
	mat4 view;
	mat4 invView;
	vec4 ambientLightColor;
	PointLight pointLights[10];
	int numLights;
} ubo;

layout(binding = 1) uniform sampler2D texSampler[20]; // Combined Image Sampler дескриптор

layout(push_constant) uniform Push {
	mat4 modelMatrix;
	mat4 normalMatrix;
	int textureIndex;
	vec3 diffuseColor;
} push;

void main() {
	// Итоговое рассеянное освещение. Инициализируется сразу обвалакивающим освещением
	vec3 diffuseLight = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;
	// Суммарное зеркальное освещение от каждого источника света
	vec3 specularLight = vec3(0.0);
	// Нормаль поверхности одинакова для всех источников света, поэтому она нормализуется вне цикла.
	// Нормализовывать её надо, потому что она пришла в шейдер фрагментов после интерполяции из нескольких нормалей вершин.
	vec3 surfaceNormal = normalize(fragNormalWorld);

	vec3 cameraPosWorld = ubo.invView[3].xyz; // извлекаем позицию наблюдателя в World Space из обратной матрицы просмотра
	vec3 viewDirection = normalize(cameraPosWorld - fragPosWorld); // направление до наблюдателя

	// В цикле считаем вклад каждого Point Light'а на сцене в результирующее рассеянное освещение фрагмента
	for (int i = 0; i < ubo.numLights; ++i) {
		PointLight light = ubo.pointLights[i]; // берём текущий точечный источник света

		vec3 directionToLight = light.position.xyz - fragPosWorld; // ещё ненормализованное направление к ист. света
		float attenuation = 1.0 / dot(directionToLight, directionToLight); // фактор ослабевания интенсивности света = 1 / квадрат расстояния до источника
		directionToLight = normalize(directionToLight);

		// косинус угла падения
		float cosAngIncidence = max(dot(surfaceNormal, directionToLight), 0);
		// интенсивность освещения данного точечного света
		vec3 intensity = light.color.xyz * light.color.w * attenuation;

		// вклад данного источника света в диффузное освещение
		diffuseLight += intensity * cosAngIncidence;

		// расчёт зеркального компонента освещения
		vec3 halfAngle = normalize(directionToLight + viewDirection);
		float blinnTerm = dot(surfaceNormal, halfAngle); // фактор-член влияния зеркального света по Блинн-Фонгу на интенсивность отражённого света
		blinnTerm = clamp(blinnTerm, 0, 1);  // отбросить случаи, когда источник света или наблюдатель находятся с другой стороны поверхности
		blinnTerm = pow(blinnTerm, 512.0);  // больше степень => резче блик отражённого света

		// вклад данного источника света в зеркальное освещение
		specularLight += intensity * blinnTerm;
	}

	// vec4 переменная может конструироваться из vec3 переменной и ещё одного компонента.
	//outColor = vec4(diffuseLight * fragColor + specularLight * fragColor, 1.0);

	// тест наложения текстуры на фрагмент
	//outColor = vec4(fragUv, 0.0, 1.0); окрашивание фрагмента значением координаты текстуры
	//outColor = texture(texSampler, fragUv * 2.0); адресация за пределы размера текстуры

	vec4 sampleTextureColor = vec4(0.8, 0.1, 0.1, 1);
	// текстурирование фрагмента по координатам + освещение
	if (push.textureIndex != -1) {
		sampleTextureColor = texture(texSampler[push.textureIndex], fragUv);
	} else {
		sampleTextureColor = vec4(push.diffuseColor, 1.0);
	}

	//outColor = sampleTextureColor; // просто текстура
	outColor = vec4(diffuseLight * sampleTextureColor.rgb + specularLight * sampleTextureColor.rgb, 1.0);
}