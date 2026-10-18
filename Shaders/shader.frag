#version 330 core

#if defined(DEPTH_PASS)

void main()
{
}

#elif defined(POINT_SHADOW_PASS)

in vec3 WorldPos;

uniform vec3 lightPos;
uniform float farPlane;

void main()
{
    gl_FragDepth = length(WorldPos - lightPos) / farPlane;
}

#elif defined(SKY_PASS)

uniform vec3 skyColour;

out vec4 FragColour;

void main()
{
    gl_FragDepth = 1.0;
    FragColour = vec4(skyColour, 1.0);
}

#else

in vec4 vCol;
in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;
in vec4 FragPosLightSpace;

out vec4 FragColour;

struct DirLight
{
    vec3 direction;
    vec3 colour;
};

struct PointLight
{
    vec3 position;
    vec3 colour;
    float constant;
    float linear;
    float quadratic;
};

struct SpotLight
{
    vec3 position;
    vec3 direction;
    vec3 colour;
    float constant;
    float linear;
    float quadratic;
    float cutOff;
};

#define MAX_POINT_LIGHTS 8

uniform DirLight sun;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform int numPointLights;
uniform SpotLight spot;
uniform vec3 ambientColour;
uniform float ambientStrength;
uniform vec3 viewPos;

uniform sampler2DShadow shadowMap;
uniform sampler2DShadow spotShadowMap;
uniform mat4 spotLightSpace;
uniform samplerCube pointShadowMap;
uniform int shadowedPointLight;
uniform float pointShadowFar;

uniform sampler2D texture2D;
uniform float specularStrength;
uniform float shininess;
uniform float roughness;
uniform bool twoSided;
uniform vec3 emissive;
uniform vec3 tint;

uniform int shadingModel;
uniform bool flatShading;

vec3 AmbientLight()
{
    return ambientColour * ambientStrength;
}

float Lambert(vec3 norm, vec3 lightDir)
{
    return max(dot(norm, lightDir), 0.0);
}

float OrenNayar(vec3 norm, vec3 lightDir, vec3 viewDir, float sigma)
{
    float NdotL = dot(norm, lightDir);
    if (NdotL <= 0.0)
        return 0.0;
    float NdotV = clamp(dot(norm, viewDir), 0.0, 1.0);
    float s2 = sigma * sigma;
    float A = 1.0 - 0.5 * s2 / (s2 + 0.33);
    float B = 0.45 * s2 / (s2 + 0.09);
    float thetaI = acos(NdotL);
    float thetaR = acos(NdotV);
    float alpha = max(thetaI, thetaR);
    float beta = min(thetaI, thetaR);
    vec3 lProj = lightDir - norm * NdotL;
    vec3 vProj = viewDir - norm * NdotV;
    float lenProduct = length(lProj) * length(vProj);
    float cosPhi = lenProduct > 1e-4 ? dot(lProj, vProj) / lenProduct : 0.0;
    return NdotL * (A + B * max(0.0, cosPhi) * sin(alpha) * tan(beta));
}

float DiffuseFactor(vec3 norm, vec3 lightDir, vec3 viewDir)
{
    float d = roughness > 0.0 ? OrenNayar(norm, lightDir, viewDir, roughness) : Lambert(norm, lightDir);
    if (shadingModel == 3)
        d = floor(d * 3.0 + 0.5) / 3.0;
    return d;
}

float SpecularFactor(vec3 norm, vec3 lightDir, vec3 viewDir)
{
    if (dot(norm, lightDir) <= 0.0 || shadingModel == 2)
        return 0.0;
    float s;
    if (shadingModel == 1)
    {
        vec3 reflectDir = reflect(-lightDir, norm);
        s = pow(max(dot(viewDir, reflectDir), 0.0), shininess * 0.25);
    }
    else
    {
        vec3 halfwayDir = normalize(lightDir + viewDir);
        s = pow(max(dot(norm, halfwayDir), 0.0), shininess);
    }
    if (shadingModel == 3)
        s = step(0.5, s);
    return s * specularStrength;
}

float Attenuation(float c, float l, float q, float d)
{
    return 1.0 / (c + l * d + q * d * d);
}

float ShadowCalculation(sampler2DShadow map, vec4 lightSpacePos, vec3 norm, vec3 lightDir, float biasScale)
{
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0)
        return 0.0;

    float bias = biasScale * max(0.0025 * (1.0 - dot(norm, lightDir)), 0.0005);
    vec2 texelSize = 1.0 / vec2(textureSize(map, 0));
    float lit = 0.0;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            lit += texture(map, vec3(projCoords.xy + vec2(x, y) * texelSize, projCoords.z - bias));
    return 1.0 - lit / 9.0;
}

const vec3 cubeOffsets[20] = vec3[](
    vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
    vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
    vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
    vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
    vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1));

float PointShadowCalculation(vec3 lightPos)
{
    vec3 fragToLight = FragPos - lightPos;
    float current = length(fragToLight);
    float bias = 0.04;
    float diskRadius = 0.015 + current * 0.01;
    float shadow = 0.0;
    for (int i = 0; i < 20; ++i)
    {
        float closest = texture(pointShadowMap, fragToLight + cubeOffsets[i] * diskRadius).r * pointShadowFar;
        shadow += current - bias > closest ? 1.0 : 0.0;
    }
    return shadow / 20.0;
}

void main()
{
    vec3 norm = flatShading ? normalize(cross(dFdx(FragPos), dFdy(FragPos))) : normalize(Normal);
    if (twoSided && !gl_FrontFacing)
        norm = -norm;
    vec3 viewDir = normalize(viewPos - FragPos);

    vec3 sunDir = normalize(-sun.direction);
    float shadow = ShadowCalculation(shadowMap, FragPosLightSpace, norm, sunDir, 1.0);
    vec3 diffuse = AmbientLight() + (1.0 - shadow) * sun.colour * DiffuseFactor(norm, sunDir, viewDir);
    vec3 specular = (1.0 - shadow) * sun.colour * SpecularFactor(norm, sunDir, viewDir);

    for (int i = 0; i < numPointLights; ++i)
    {
        vec3 toLight = pointLights[i].position - FragPos;
        float d = length(toLight);
        vec3 lightDir = toLight / d;
        vec3 radiance = pointLights[i].colour *
                        Attenuation(pointLights[i].constant, pointLights[i].linear, pointLights[i].quadratic, d);
        if (i == shadowedPointLight)
            radiance *= 1.0 - PointShadowCalculation(pointLights[i].position);
        diffuse += radiance * DiffuseFactor(norm, lightDir, viewDir);
        specular += radiance * SpecularFactor(norm, lightDir, viewDir);
    }

    {
        vec3 toLight = spot.position - FragPos;
        float d = length(toLight);
        vec3 lightDir = toLight / d;
        float angleToFragment = dot(-lightDir, normalize(spot.direction));
        if (angleToFragment > spot.cutOff)
        {
            float spotLightFade = 1.0 - (1.0 - angleToFragment) / (1.0 - spot.cutOff);
            float spotShadow = ShadowCalculation(spotShadowMap, spotLightSpace * vec4(FragPos, 1.0),
                                                 norm, lightDir, 0.3);
            vec3 radiance = spot.colour * Attenuation(spot.constant, spot.linear, spot.quadratic, d) *
                            spotLightFade * (1.0 - spotShadow);
            diffuse += radiance * DiffuseFactor(norm, lightDir, viewDir);
            specular += radiance * SpecularFactor(norm, lightDir, viewDir);
        }
    }

    vec3 albedo = texture(texture2D, TexCoord).rgb * vCol.rgb * tint;
    FragColour = vec4(albedo * (diffuse + emissive) + specular, 1.0);
}

#endif
