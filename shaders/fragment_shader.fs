#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform sampler2D topTexture;
uniform sampler2D sideTexture;
uniform sampler2D bottomTexture;
uniform sampler2D shadowMap;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform mat4 lightSpaceMatrix;

uniform bool isSun;
uniform float renderDistance;
uniform vec3 sunPosition;
uniform vec3 fogColor;

float ShadowCalculation(vec4 fragPosLightSpace)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;    
    
    float currentDepth = projCoords.z;
    float shadow = 0.0;
    float bias = 0.005;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    if(projCoords.z > 1.0)
        shadow = 0.0;

    return shadow;
}

void main()
{
    if (isSun) {
        FragColor = vec4(1.0, 1.0, 0.0, 1.0);
        return;
    }

    vec3 color;
    if (Normal.y > 0.5) { // Top face
        color = texture(topTexture, TexCoords).rgb;
    } else if (Normal.y < -0.5) { // Bottom face
        color = texture(bottomTexture, TexCoords).rgb;
    } else { // Side faces
        color = texture(sideTexture, TexCoords).rgb;
    }

    vec3 normal = normalize(Normal);
    vec3 lightColor = vec3(1.0);
    // Ambient
    vec3 ambient = 0.3 * color;
    // Diffuse
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(lightDir, normal), 0.0);

    vec3 diffuse = diff * lightColor;
    // Specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = 0.0;
    vec3 halfwayDir = normalize(lightDir + viewDir);
    spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    vec3 specular = spec * lightColor;

    // Shadow
    vec4 fragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
    float shadow = ShadowCalculation(fragPosLightSpace);
    vec3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular)) * color;

    // Fog effect
    float distance = length(viewPos - FragPos);
    float fogFactor = clamp((distance - 4.0) / (renderDistance - 4.0), 0.0, 1.0);
    lighting = mix(lighting, fogColor, fogFactor);

    FragColor = vec4(lighting, 1.0);
}
