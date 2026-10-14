#version 330 core

in vec4 vCol;
in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;
in vec4 FragPosLightSpace;

out vec4 colour;

// ---- lights ----
struct DirLight
{
    vec3 direction;   // direction the light travels (from sun towards scene)
    vec3 colour;
};

struct PointLight
{
    vec3 position;
    vec3 colour;
    float constant;   // attenuation = 1 / (constant + linear*d + quadratic*d^2)
    float linear;
    float quadratic;
};

#define MAX_POINT_LIGHTS 8

uniform DirLight sun;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform int numPointLights;
uniform vec3 ambientColour;
uniform float ambientStrength;
uniform vec3 viewPos;

// ---- material ----
uniform sampler2D texture2D;
uniform sampler2D shadowMap;
uniform float specularStrength;
uniform float shininess;
uniform vec3 emissive;        // self-lit colour (lanterns, neon tube, signs)
uniform vec3 tint;            // per-object colour multiplier (stools, second owner)

// Lab 11: ambient light
vec3 ambientLight()
{
    vec3 ambient = ambientColour * ambientStrength;
    return ambient;
}

// Lab 12: Lambertian diffuse light
vec3 diffuseLight(vec3 lightColour, vec3 lightDir)
{
    float diffuseStrength = 1.0f;

    vec3 norm = normalize(Normal);

    float diff = max(dot(norm, lightDir), 0.0f);
    vec3 diffuse = lightColour * diff * diffuseStrength;
    return diffuse;
}

// Lab 12: Phong specular light
vec3 specularLight(vec3 lightColour, vec3 lightDir)
{
    vec3 norm = normalize(Normal);
    vec3 reflectDir = reflect(-lightDir, norm);
    vec3 viewDir = normalize(viewPos - FragPos);

    float spec = pow(max(dot(viewDir, reflectDir), 0.0f), shininess);

    vec3 specular = lightColour * spec * specularStrength;
    return specular;
}

// Lab 13: shadow map with bias and 3x3 averaging filter (PCF)
float ShadowCalculation()
{
    // perform perspective divide
    vec3 projCoords = FragPosLightSpace.xyz / FragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    float currentDepth = projCoords.z;

    float bias = 0.001;
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    // outside the light's frustum: no shadow (over-sampling)
    if (projCoords.z > 1.0)
        shadow = 0.0;

    return shadow;
}

void main()
{
    // sun (directional) - diffuse and specular are blocked by the shadow
    vec3 sunDir = normalize(-sun.direction);
    float shadow = ShadowCalculation();
    vec3 lighting = ambientLight() +
                    (1.0 - shadow) * (diffuseLight(sun.colour, sunDir) + specularLight(sun.colour, sunDir));

    // point lights (lanterns, neon, stove) with attenuation
    for (int i = 0; i < numPointLights; ++i)
    {
        vec3 lightDir = normalize(pointLights[i].position - FragPos);
        float d = length(pointLights[i].position - FragPos);
        float attenuation = 1.0 / (pointLights[i].constant + pointLights[i].linear * d +
                                   pointLights[i].quadratic * d * d);
        lighting += attenuation * (diffuseLight(pointLights[i].colour, lightDir) +
                                   specularLight(pointLights[i].colour, lightDir));
    }

    //Phong Shading
    colour = texture(texture2D, TexCoord) * vCol * vec4(tint, 1.0) * vec4(lighting + emissive, 1.0f);
}
