#version 330 core

in vec4 vCol;
in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;
in vec4 FragPosLightSpace;

out vec4 FragColour;

// ---- lights (Week 12: directional, point and spot light sources) ----
struct DirLight
{
    vec3 direction;   // direction the light travels (from the sun towards the scene)
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

struct SpotLight
{
    vec3 position;
    vec3 direction;   // where the spot is facing
    vec3 colour;
    float constant;
    float linear;
    float quadratic;
    float cutOff;     // cos(cut-off angle)
};

#define MAX_POINT_LIGHTS 8

uniform DirLight sun;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform int numPointLights;
uniform SpotLight spot;
uniform vec3 ambientColour;
uniform float ambientStrength;
uniform vec3 viewPos;

// ---- shadows (Week 13) ----
uniform sampler2DShadow shadowMap;      // sun: orthographic depth map, hardware compare
uniform sampler2DShadow spotShadowMap;  // spot: perspective depth map
uniform mat4 spotLightSpace;
uniform samplerCube pointShadowMap;     // omnidirectional (cube) map of one point light
uniform int shadowedPointLight;         // index of the point light that owns the cube map
uniform float pointShadowFar;           // cube map stores distance / far

// ---- material ----
uniform sampler2D texture2D;        // colour texture
uniform float specularStrength;     // Ks
uniform float shininess;            // n
uniform float roughness;            // sigma for Oren-Nayar; 0 = Lambert
uniform bool twoSided;              // thin sheets (sail) are lit from both sides
uniform vec3 emissive;              // self-lit strength (lanterns, neon tube, bulbs, signs)
uniform vec3 tint;                  // per-object colour multiplier

// ---- shading model switch (keys 1-4, F) for comparing the models from Week 12 ----
// 0 Blinn-Phong, 1 Phong, 2 Lambert only (matte), 3 cel / toon
uniform int shadingModel;
uniform bool flatShading;           // one normal per polygon instead of interpolated normals

// Lab 11: ambient light, I_amb = Ka * Ia
vec3 AmbientLight()
{
    return ambientColour * ambientStrength;
}

// Lambert's cosine law: irradiance falls with cos(theta) = N . L
float Lambert(vec3 norm, vec3 lightDir)
{
    return max(dot(norm, lightDir), 0.0);
}

// Oren-Nayar (Week 12) for rough diffuse surfaces such as concrete and asphalt.
// The 1/pi and albedo are applied outside, so sigma = 0 gives exactly Lambert.
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
    // cos(phi_i - phi_r): angle between L and V after projecting both onto the surface
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
        d = floor(d * 3.0 + 0.5) / 3.0;   // cel shading: three flat bands
    return d;
}

// Specular term. A light behind the surface (no diffuse) gives no highlight either.
//  Phong:       (R . V)^n
//  Blinn-Phong: (N . H)^n with H = normalize(L + V); never cuts off sharply past 90 degrees
float SpecularFactor(vec3 norm, vec3 lightDir, vec3 viewDir)
{
    if (dot(norm, lightDir) <= 0.0 || shadingModel == 2)
        return 0.0;
    float s;
    if (shadingModel == 1)
    {
        vec3 reflectDir = reflect(-lightDir, norm);
        s = pow(max(dot(viewDir, reflectDir), 0.0), shininess * 0.25);   // Phong needs ~1/4 the exponent
    }
    else
    {
        vec3 halfwayDir = normalize(lightDir + viewDir);
        s = pow(max(dot(norm, halfwayDir), 0.0), shininess);
    }
    if (shadingModel == 3)
        s = step(0.5, s);                // cel shading: hard highlight
    return s * specularStrength;
}

// Attenuation = 1 / (constant + linear*d + quadratic*d^2)
float Attenuation(float c, float l, float q, float d)
{
    return 1.0 / (c + l * d + q * d * d);
}

// Shadow map lookup (Week 13) for a 2D depth map.
// Perspective divide, scale -1..1 to 0..1, compare with the stored closest depth.
// Bias grows with the surface slope to remove shadow acne without Peter Panning.
// sampler2DShadow compares in hardware and filters the 2x2 results; averaging a 3x3
// block of those is Percentage Closer Filtering (PCF) for soft edges.
float ShadowCalculation(sampler2DShadow map, vec4 lightSpacePos, vec3 norm, vec3 lightDir, float biasScale)
{
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;   // perspective divide
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0)          // beyond the far plane of the light: never in shadow
        return 0.0;

    float bias = biasScale * max(0.0025 * (1.0 - dot(norm, lightDir)), 0.0005);
    vec2 texelSize = 1.0 / vec2(textureSize(map, 0));
    float lit = 0.0;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            lit += texture(map, vec3(projCoords.xy + vec2(x, y) * texelSize, projCoords.z - bias));
    return 1.0 - lit / 9.0;
}

// Omnidirectional shadow (Week 13): the cube map is looked up with the direction from the
// light to the fragment, so no light-space matrix is needed. PCF over 20 offset directions.
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
    // Phong interpolation uses the interpolated vertex normal; flat shading rebuilds one normal
    // per polygon from the screen-space derivatives of the position (no interpolation)
    vec3 norm = flatShading ? normalize(cross(dFdx(FragPos), dFdy(FragPos))) : normalize(Normal);
    if (twoSided && !gl_FrontFacing)
        norm = -norm;
    vec3 viewDir = normalize(viewPos - FragPos);

    // sun (directional): diffuse and specular are blocked by the shadow; ambient always stays
    vec3 sunDir = normalize(-sun.direction);
    float shadow = ShadowCalculation(shadowMap, FragPosLightSpace, norm, sunDir, 1.0);
    vec3 diffuse = AmbientLight() + (1.0 - shadow) * sun.colour * DiffuseFactor(norm, sunDir, viewDir);
    vec3 specular = (1.0 - shadow) * sun.colour * SpecularFactor(norm, sunDir, viewDir);

    // point lights (lanterns, neon, stove, bulb strings) with attenuation
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

    // spot light: a point light limited to a cone, with the soft edge from the lecture:
    // spotLightFade = 1 - (1 - angleToFragment) / (1 - cutOff)
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

    // colour = object colour x (ambient + diffuse + emission) + specular
    // the highlight keeps the light's colour (not tinted by the surface)
    vec3 albedo = texture(texture2D, TexCoord).rgb * vCol.rgb * tint;
    FragColour = vec4(albedo * (diffuse + emissive) + specular, 1.0);
}
