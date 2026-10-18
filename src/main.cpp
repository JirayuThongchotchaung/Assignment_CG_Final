#undef GLFW_DLL
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

const GLint WIDTH = 800;
const GLint HEIGHT = 600;
const float PI = 3.14159265f;
const int MAX_POINT_LIGHTS = 8;

const std::filesystem::path shaderDirectory = "Shaders";
const std::filesystem::path screenshotDirectory = "Screenshots";

class Shader
{
public:
    Shader() = default;
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    ~Shader() { if (program) glDeleteProgram(program); }

    void CreateFromFiles(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath,
                         const char* passDefine = nullptr)
    {
        std::string vertexCode = ReadFile(vertexPath);
        std::string fragmentCode = ReadFile(fragmentPath);
        if (vertexCode.empty() || fragmentCode.empty())
            return;
        if (passDefine)
        {
            AddDefine(vertexCode, passDefine);
            AddDefine(fragmentCode, passDefine);
        }
        program = glCreateProgram();
        AddShader(vertexCode.c_str(), GL_VERTEX_SHADER);
        AddShader(fragmentCode.c_str(), GL_FRAGMENT_SHADER);
        glLinkProgram(program);
        GLint ok = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok)
        {
            GLchar log[1024] = {};
            glGetProgramInfoLog(program, sizeof(log), nullptr, log);
            std::cerr << "Error linking " << fragmentPath.u8string() << ": " << log << std::endl;
        }
    }

    void UseShader() const { glUseProgram(program); }

    // หา location ของ uniform จากชื่อ (ใช้ตอน setup พอ ช้า)
    GLint GetUniformLocation(const char* name) const { return glGetUniformLocation(program, name); }

private:
    GLuint program = 0;

    // อ่านไฟล์ shader ทั้งไฟล์มาเป็น string
    static std::string ReadFile(const std::filesystem::path& path)
    {
        std::ifstream in(path);
        if (!in)
        {
            std::cerr << "Failed to read shader file: " << path.u8string() << std::endl;
            return {};
        }
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    // #version ต้องเป็นบรรทัดแรกเสมอ เลยแทรก #define ต่อจากบรรทัดนั้น
    static void AddDefine(std::string& code, const char* name)
    {
        size_t lineEnd = code.find('\n');
        size_t at = lineEnd == std::string::npos ? code.size() : lineEnd + 1;
        code.insert(at, std::string("#define ") + name + "\n");
    }

    // คอมไพล์ shader ทีละอัน แล้วเอาไปแปะกับโปรแกรม
    void AddShader(const char* code, GLenum type)
    {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &code, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            GLchar log[1024] = {};
            glGetShaderInfoLog(s, sizeof(log), nullptr, log);
            std::cerr << "Error compiling shader type " << type << ": " << log << std::endl;
        }
        glAttachShader(program, s);
        glDeleteShader(s);
    }
};

class Texture
{
public:
    explicit Texture(const std::string& path) : fileLocation(path) {}
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    ~Texture() { if (textureID) glDeleteTextures(1, &textureID); }

    // โหลดรูปด้วย stb_image แล้วโยนขึ้น GPU เป็น texture + mipmap
    bool LoadTexture()
    {
        int width, height, channels;
        unsigned char* data = stbi_load(fileLocation.c_str(), &width, &height, &channels, 0);
        if (!data)
        {
            std::cerr << "Failed to load texture " << fileLocation << ": " << stbi_failure_reason() << std::endl;
            return false;
        }
        GLenum format = channels == 4 ? GL_RGBA : channels == 1 ? GL_RED : GL_RGB;
        GLenum internalFormat = channels == 4 ? GL_RGBA8 : channels == 1 ? GL_R8 : GL_RGB8;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(data);
        return true;
    }

    // bind texture เข้า unit ที่บอก
    void UseTexture(GLenum unit = GL_TEXTURE0) const
    {
        glActiveTexture(unit);
        glBindTexture(GL_TEXTURE_2D, textureID);
    }

private:
    GLuint textureID = 0;
    std::string fileLocation;
};

struct AABB
{
    glm::vec3 min = glm::vec3(1e30f);
    glm::vec3 max = glm::vec3(-1e30f);

    void Expand(glm::vec3 p) { min = glm::min(min, p); max = glm::max(max, p); }

    // เอามุมกล่องทั้ง 8 มุมไปคูณ m แล้วหากล่องใหม่
    AABB Transformed(const glm::mat4& m) const
    {
        AABB out;
        for (int i = 0; i < 8; i++)
        {
            glm::vec3 corner((i & 1) ? max.x : min.x, (i & 2) ? max.y : min.y, (i & 4) ? max.z : min.z);
            out.Expand(glm::vec3(m * glm::vec4(corner, 1.0f)));
        }
        return out;
    }
};

// เช็ค ray ชนกล่อง AABB แบบ slab ชนคืนระยะ ไม่ชนคืน -1
float RayAABB(glm::vec3 origin, glm::vec3 dir, const AABB& box)
{
    float tMin = 0.0f, tMax = 1e30f;
    for (int a = 0; a < 3; a++)
    {
        if (std::abs(dir[a]) < 1e-8f)
        {
            if (origin[a] < box.min[a] || origin[a] > box.max[a])
                return -1.0f;
            continue;
        }
        float t0 = (box.min[a] - origin[a]) / dir[a];
        float t1 = (box.max[a] - origin[a]) / dir[a];
        if (t0 > t1) std::swap(t0, t1);
        tMin = std::max(tMin, t0);
        tMax = std::min(tMax, t1);
        if (tMin > tMax)
            return -1.0f;
    }
    return tMin;
}

struct Vertex
{
    glm::vec3 position{};
    glm::vec3 normal{};
    glm::vec2 uv{};
    glm::vec3 colour{ 1.0f };
};

// เอาแค่ส่วนโฟลเดอร์ของ path (มี / ท้ายด้วย)
std::string DirectoryOf(const std::string& path)
{
    size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}

// ตัด space กับ \r ท้ายบรรทัดทิ้ง ไฟล์จาก windows ชอบมี
std::string TrimLine(std::string s)
{
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ')) s.pop_back();
    return s;
}

class Mesh
{
public:
    Mesh() = default;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    ~Mesh()
    {
        if (IBO) glDeleteBuffers(1, &IBO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (VAO) glDeleteVertexArrays(1, &VAO);
    }

    // สร้าง mesh จาก array ที่เรียง P(3) N(3) UV(2) ต่อกันไปเรื่อยๆ
    void CreateMesh(const GLfloat* vertices, const unsigned int* indices,
                    unsigned int numOfFloats, unsigned int numOfIndices)
    {
        std::vector<Vertex> vtx(numOfFloats / 8);
        for (size_t i = 0; i < vtx.size(); i++)
        {
            const GLfloat* b = vertices + i * 8;
            vtx[i].position = { b[0], b[1], b[2] };
            vtx[i].normal = { b[3], b[4], b[5] };
            vtx[i].uv = { b[6], b[7] };
        }
        Upload(vtx, std::vector<unsigned int>(indices, indices + numOfIndices));
    }

    // โหลด .obj เข้า mesh เอาสีจาก .mtl มาด้วย
    bool CreateMeshFromOBJ(const std::string& path, bool flipV = false)
    {
        std::ifstream in(path);
        if (!in)
            return false;

        std::vector<glm::vec3> V, VN;
        std::vector<glm::vec2> VT;
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        std::unordered_map<std::string, unsigned int> dedup;
        std::unordered_map<std::string, glm::vec3> materials;
        std::vector<unsigned int> positionIndex;
        glm::vec3 currentColour(1.0f);
        std::string currentMaterial;

        std::string line;
        while (std::getline(in, line))
        {
            std::istringstream ss(line);
            std::string tag;
            ss >> tag;
            if (tag == "v")       { glm::vec3 p; ss >> p.x >> p.y >> p.z; V.push_back(p); }
            else if (tag == "vn") { glm::vec3 n; ss >> n.x >> n.y >> n.z; VN.push_back(n); }
            else if (tag == "vt")
            {
                glm::vec2 t; ss >> t.x >> t.y;
                if (flipV) t.y = 1.0f - t.y;
                VT.push_back(t);
            }
            else if (tag == "mtllib")
            {
                std::string file;
                std::getline(ss >> std::ws, file);
                LoadMTL(DirectoryOf(path) + TrimLine(file), materials);
            }
            else if (tag == "usemtl")
            {
                ss >> currentMaterial;
                auto it = materials.find(currentMaterial);
                currentColour = it != materials.end() ? it->second : glm::vec3(1.0f);
            }
            else if (tag == "f")
            {
                std::vector<unsigned int> face;
                std::string tok;
                while (ss >> tok)
                {
                    std::string key = tok + "/" + currentMaterial;
                    auto it = dedup.find(key);
                    if (it != dedup.end()) { face.push_back(it->second); continue; }

                    int idx[3] = { 0, 0, 0 };
                    std::istringstream ts(tok);
                    std::string part;
                    for (int k = 0; k < 3 && std::getline(ts, part, '/'); k++)
                        if (!part.empty()) idx[k] = std::stoi(part);
                    auto fix = [](int i, size_t count) { return i > 0 ? i - 1 : i < 0 ? (int)count + i : -1; };
                    int iv = fix(idx[0], V.size()), ivt = fix(idx[1], VT.size()), ivn = fix(idx[2], VN.size());
                    if (iv < 0) continue;

                    positionIndex.push_back((unsigned int)iv);
                    Vertex out;
                    out.position = V[iv];
                    if (ivt >= 0) out.uv = VT[ivt];
                    if (ivn >= 0) out.normal = VN[ivn];
                    out.colour = currentColour;
                    unsigned int id = (unsigned int)vertices.size();
                    vertices.push_back(out);
                    dedup.emplace(key, id);
                    face.push_back(id);
                }
                for (size_t i = 1; i + 1 < face.size(); i++)
                    indices.insert(indices.end(), { face[0], face[i], face[i + 1] });
            }
        }
        if (indices.empty())
            return false;
        CheckManifold(path, V.size(), vertices, indices, positionIndex);
        Upload(vertices, indices);
        return true;
    }

    // วาด mesh (indexed triangles)
    void RenderMesh() const
    {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    const std::string& GetDiffuseMap() const { return diffuseMap; }
    const AABB& GetBounds() const { return bounds; }

private:
    GLuint VAO = 0, VBO = 0, IBO = 0;
    GLsizei indexCount = 0;
    std::string diffuseMap;
    AABB bounds;

    // เช็ค manifold นับขอบ boundary กับ non-manifold แล้วปริ้นออกมาดู
    static void CheckManifold(const std::string& path, size_t positionCount, const std::vector<Vertex>&,
                              const std::vector<unsigned int>& indices, const std::vector<unsigned int>& positionIndex)
    {
        std::unordered_map<unsigned long long, int> edgeFaces;
        for (size_t t = 0; t + 2 < indices.size(); t += 3)
            for (int k = 0; k < 3; k++)
            {
                unsigned long long a = positionIndex[indices[t + k]];
                unsigned long long b = positionIndex[indices[t + (k + 1) % 3]];
                if (a > b) std::swap(a, b);
                edgeFaces[(a << 32) | b]++;
            }
        size_t boundary = 0, nonManifold = 0;
        for (const auto& e : edgeFaces)
        {
            if (e.second == 1) boundary++;
            else if (e.second > 2) nonManifold++;
        }
        std::cout << path << ": V=" << positionCount << " E=" << edgeFaces.size() << " F=" << indices.size() / 3
                  << ", boundary edges " << boundary << ", non-manifold edges " << nonManifold << std::endl;
    }

    // อ่าน Kd กับ map_Kd อันแรกจาก .mtl
    void LoadMTL(const std::string& path, std::unordered_map<std::string, glm::vec3>& materials)
    {
        std::ifstream in(path);
        std::string line, current;
        while (std::getline(in, line))
        {
            std::istringstream ss(line);
            std::string tag;
            ss >> tag;
            if (tag == "newmtl") { ss >> current; materials[current] = glm::vec3(1.0f); }
            else if (tag == "Kd") { glm::vec3 kd; ss >> kd.r >> kd.g >> kd.b; materials[current] = kd; }
            else if (tag == "map_Kd" && diffuseMap.empty())
            {
                std::string file;
                std::getline(ss >> std::ws, file);
                diffuseMap = DirectoryOf(path) + TrimLine(file);
            }
        }
    }

    // สร้าง VAO VBO EBO แล้วเซ็ต attribute ให้ตรงกับ shader.vert
    void Upload(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
    {
        for (const Vertex& v : vertices)
            bounds.Expand(v.position);
        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);
        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
        glGenBuffers(1, &IBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        const GLsizei stride = sizeof(Vertex);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, position));
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, uv));
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, normal));
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, colour));
        for (GLuint i = 0; i < 4; i++)
            glEnableVertexAttribArray(i);

        indexCount = (GLsizei)indices.size();
        glBindVertexArray(0);
    }
};

class ShadowMap
{
public:
    // สร้าง depth texture กับ framebuffer ไว้ทำ shadow map
    explicit ShadowMap(GLsizei size) : size(size)
    {
        glGenTextures(1, &depthMap);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        const float borderColour[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColour);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

        glGenFramebuffers(1, &FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, FBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;
    ~ShadowMap()
    {
        glDeleteFramebuffers(1, &FBO);
        glDeleteTextures(1, &depthMap);
    }

    // bind fbo เงา แล้ว clear depth ก่อนวาด
    void BeginPass() const
    {
        glViewport(0, 0, size, size);
        glBindFramebuffer(GL_FRAMEBUFFER, FBO);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    void BindTexture(GLenum unit) const
    {
        glActiveTexture(unit);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        glActiveTexture(GL_TEXTURE0);
    }

private:
    GLsizei size;
    GLuint FBO = 0, depthMap = 0;
};

class CubeShadowMap
{
public:
    // สร้าง cube map เก็บ depth ไว้ทำเงารอบทิศของตะเกียง
    CubeShadowMap(GLsizei size, float farPlane) : size(size), farPlane(farPlane)
    {
        glGenTextures(1, &cubeMap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap);
        for (int face = 0; face < 6; face++)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT24, size, size, 0,
                         GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1, &FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, FBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, cubeMap, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    CubeShadowMap(const CubeShadowMap&) = delete;
    CubeShadowMap& operator=(const CubeShadowMap&) = delete;
    ~CubeShadowMap()
    {
        glDeleteFramebuffers(1, &FBO);
        glDeleteTextures(1, &cubeMap);
    }

    // perspective 90 องศา ใช้ทุกหน้า cube
    glm::mat4 Projection() const { return glm::perspective(glm::radians(90.0f), 1.0f, 0.05f, farPlane); }

    // view matrix มองจากไฟออกไปทางหน้า cube นั้นๆ
    static glm::mat4 FaceView(glm::vec3 lightPos, int face)
    {
        static const glm::vec3 dirs[6] = { {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1} };
        static const glm::vec3 ups[6]  = { {0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0} };
        return glm::lookAt(lightPos, lightPos + dirs[face], ups[face]);
    }

    // เอาหน้า cube ไปผูกกับ fbo แล้ว clear depth
    void BeginFace(int face) const
    {
        glViewport(0, 0, size, size);
        glBindFramebuffer(GL_FRAMEBUFFER, FBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, cubeMap, 0);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    void BindTexture(GLenum unit) const
    {
        glActiveTexture(unit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap);
        glActiveTexture(GL_TEXTURE0);
    }

    float FarPlane() const { return farPlane; }

private:
    GLsizei size;
    float farPlane;
    GLuint FBO = 0, cubeMap = 0;
};

struct ObjectUniforms
{
    GLint model, view, projection, normalMatrix;
    GLint specularStrength, shininess, roughness, twoSided, emissive, uvScale, tint;

    // หา location uniform ของ object
    static ObjectUniforms Locate(const Shader& s)
    {
        return { s.GetUniformLocation("model"), s.GetUniformLocation("view"),
                 s.GetUniformLocation("projection"), s.GetUniformLocation("normalMatrix"),
                 s.GetUniformLocation("specularStrength"), s.GetUniformLocation("shininess"),
                 s.GetUniformLocation("roughness"), s.GetUniformLocation("twoSided"),
                 s.GetUniformLocation("emissive"), s.GetUniformLocation("uvScale"),
                 s.GetUniformLocation("tint") };
    }
};

struct PointLightUniforms
{
    GLint position, colour, constant, linear, quadratic;
};

struct LightingUniforms
{
    GLint sunDirection, sunColour, ambientColour, ambientStrength, numPointLights;
    GLint viewPos, lightSpace, texture2D, shadowMap;
    GLint spotPosition, spotDirection, spotColour, spotConstant, spotLinear, spotQuadratic, spotCutOff;
    GLint spotShadowMap, spotLightSpace, pointShadowMap, shadowedPointLight, pointShadowFar;
    GLint shadingModel, flatShading, reflection, clipPlane;
    std::array<PointLightUniforms, MAX_POINT_LIGHTS> pointLights;

    // หา location uniform ของแสงทั้งหมด
    static LightingUniforms Locate(const Shader& s)
    {
        LightingUniforms u;
        u.sunDirection = s.GetUniformLocation("sun.direction");
        u.sunColour = s.GetUniformLocation("sun.colour");
        u.ambientColour = s.GetUniformLocation("ambientColour");
        u.ambientStrength = s.GetUniformLocation("ambientStrength");
        u.numPointLights = s.GetUniformLocation("numPointLights");
        u.viewPos = s.GetUniformLocation("viewPos");
        u.lightSpace = s.GetUniformLocation("lightSpace");
        u.texture2D = s.GetUniformLocation("texture2D");
        u.shadowMap = s.GetUniformLocation("shadowMap");
        u.spotPosition = s.GetUniformLocation("spot.position");
        u.spotDirection = s.GetUniformLocation("spot.direction");
        u.spotColour = s.GetUniformLocation("spot.colour");
        u.spotConstant = s.GetUniformLocation("spot.constant");
        u.spotLinear = s.GetUniformLocation("spot.linear");
        u.spotQuadratic = s.GetUniformLocation("spot.quadratic");
        u.spotCutOff = s.GetUniformLocation("spot.cutOff");
        u.spotShadowMap = s.GetUniformLocation("spotShadowMap");
        u.spotLightSpace = s.GetUniformLocation("spotLightSpace");
        u.pointShadowMap = s.GetUniformLocation("pointShadowMap");
        u.shadowedPointLight = s.GetUniformLocation("shadowedPointLight");
        u.pointShadowFar = s.GetUniformLocation("pointShadowFar");
        u.shadingModel = s.GetUniformLocation("shadingModel");
        u.flatShading = s.GetUniformLocation("flatShading");
        u.reflection = s.GetUniformLocation("reflection");
        u.clipPlane = s.GetUniformLocation("clipPlane");
        for (int i = 0; i < MAX_POINT_LIGHTS; i++)
        {
            std::string base = "pointLights[" + std::to_string(i) + "].";
            u.pointLights[i] = { s.GetUniformLocation((base + "position").c_str()),
                                 s.GetUniformLocation((base + "colour").c_str()),
                                 s.GetUniformLocation((base + "constant").c_str()),
                                 s.GetUniformLocation((base + "linear").c_str()),
                                 s.GetUniformLocation((base + "quadratic").c_str()) };
        }
        return u;
    }
};

struct Shaders
{
    std::unique_ptr<Shader> main, depth, pointShadow, sky;
};

struct Meshes
{
    std::unique_ptr<Mesh> box, cylinder, stool, sphere, sail, puddle;
    std::unique_ptr<Mesh> cart, worker, lantern, table, umbrella, ramen;
};

struct Textures
{
    std::unique_ptr<Texture> white, sidewalk, asphalt, curb, shophouse, stainless;
    std::unique_ptr<Texture> shopSign, michelinSign, menuSign, worker, ramen;
};

Shaders shaders;
Meshes meshes;
Textures textures;

ObjectUniforms mainUniforms, depthUniforms, pointShadowUniforms;
LightingUniforms lightingUniforms;
const ObjectUniforms* currentUniforms = nullptr;
GLint pointShadowLightPos, pointShadowFarPlane;
GLint skyColourLocation;
GLuint emptyVAO = 0;

enum class Pass { SunShadow, LocalShadow, Camera };
Pass currentPass = Pass::Camera;

const glm::vec3 cartPos         = glm::vec3(-0.9f, 1.05f, -1.4f);
const float cartScale           = 0.7f;
const float counterY            = 1.19f;
const glm::vec3 lanternLeftPos  = glm::vec3(-2.10f, 1.74f, -0.98f);
const glm::vec3 lanternRightPos = glm::vec3(-0.66f, 1.74f, -0.98f);
const glm::vec3 neonPos         = glm::vec3(-1.38f, 1.88f, -1.06f);
const glm::vec3 potPos          = glm::vec3(-1.55f, counterY, -1.50f);
const glm::vec3 shopSignPos     = glm::vec3(-1.38f, 2.66f, -1.25f);
const glm::vec3 frontTablePos   = glm::vec3(2.0f, 0.0f, 0.3f);
const float facadeHeight        = 5.0f;

const glm::vec3 lampPos         = glm::vec3(2.2f, 1.95f, 0.35f);
const glm::vec3 puddleCentre    = glm::vec3(-0.35f, 0.0f, 0.55f);
const float puddleY             = 0.004f;

// หาจุดบนเส้นโค้ง bezier (cubic) ที่ t
glm::vec3 BezierCubic(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 p4, float t)
{
    float s = 1.0f - t;
    return s * s * s * p1 + 3.0f * t * s * s * p2 + 3.0f * t * t * s * p3 + t * t * t * p4;
}

// หา tangent dP/dt ของ bezier ที่ t เอาไปหา normal ของ patch
glm::vec3 BezierCubicTangent(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 p4, float t)
{
    float s = 1.0f - t;
    return 3.0f * s * s * (p2 - p1) + 6.0f * s * t * (p3 - p2) + 3.0f * t * t * (p4 - p3);
}

// แปลง HSV เป็น RGB
glm::vec3 HsvToRgb(float h, float s, float v)
{
    glm::vec3 k = glm::fract(glm::vec3(h) + glm::vec3(1.0f, 2.0f / 3.0f, 1.0f / 3.0f)) * 6.0f - 3.0f;
    glm::vec3 rgb = glm::clamp(glm::abs(k) - 1.0f, 0.0f, 1.0f);
    return v * glm::mix(glm::vec3(1.0f), rgb, s);
}

struct BulbString
{
    glm::vec3 from, to;
    float sag;
    int bulbs;
};

const std::vector<BulbString> bulbStrings =
{
    { shopSignPos + glm::vec3(0.85f, 0.0f, 0.0f), glm::vec3(3.3f, 2.25f, -2.3f), 0.35f, 9 },
    { shopSignPos - glm::vec3(0.85f, 0.0f, 0.0f), glm::vec3(-5.5f, 3.4f, -4.45f), 0.30f, 7 },
    { glm::vec3(-9.0f, 3.9f, -4.4f), glm::vec3(9.0f, 3.9f, -4.4f), 0.45f, 26 },
};

// หาจุดบนสายไฟที่ห้อยๆ ที่ t
glm::vec3 PointOnString(const BulbString& s, float t)
{
    glm::vec3 drop(0.0f, s.sag * 4.0f / 3.0f, 0.0f);
    return BezierCubic(s.from, glm::mix(s.from, s.to, 1.0f / 3.0f) - drop,
                       glm::mix(s.from, s.to, 2.0f / 3.0f) - drop, s.to, t);
}

struct PointLight
{
    glm::vec3 position;
    glm::vec3 colour;
    float constant, linear, quadratic;
};

const glm::vec3 sunDirection = glm::normalize(glm::vec3(0.85f, -0.5f, -0.75f));

const glm::vec3 sunColour = glm::vec3(1.0f, 0.62f, 0.38f) * 0.8f;
const glm::vec3 ambientColour = glm::vec3(0.45f, 0.40f, 0.55f);
const float ambientStrength = 0.25f;
const glm::vec3 skyColour = glm::vec3(0.55f, 0.45f, 0.50f);

struct SpotLight
{
    glm::vec3 position, direction, colour;
    float constant, linear, quadratic;
    float cutOffDegrees;
};
const SpotLight spotLight = { lampPos, glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.75f, 0.9f, 1.0f) * 5.0f,
                              1.0f, 0.25f, 0.35f, 38.0f };

const int shadowedPointLight = 1;
const float pointShadowFar = 8.0f;

int shadingModel = 0;
bool flatShading = false;

const std::vector<PointLight> pointLights =
{
    { lanternLeftPos,  glm::vec3(1.0f, 0.30f, 0.08f) * 1.2f, 1.0f, 0.70f, 1.20f },
    { lanternRightPos, glm::vec3(1.0f, 0.30f, 0.08f) * 1.2f, 1.0f, 0.70f, 1.20f },
    { neonPos,         glm::vec3(0.70f, 0.90f, 1.0f) * 0.8f, 1.0f, 0.50f, 0.90f },
    { potPos + glm::vec3(0.0f, 0.12f, 0.35f), glm::vec3(1.0f, 0.25f, 0.05f) * 0.5f, 1.0f, 0.70f, 1.80f },
    { PointOnString(bulbStrings[0], 0.5f), glm::vec3(1.0f, 0.65f, 0.30f) * 0.6f, 1.0f, 0.70f, 1.20f },
    { PointOnString(bulbStrings[2], 0.5f), glm::vec3(1.0f, 0.65f, 0.30f) * 0.4f, 1.0f, 0.50f, 0.55f },
};

const int spotLightIndex = MAX_POINT_LIGHTS;
bool lightOn[MAX_POINT_LIGHTS + 1] = { true, true, true, true, true, true, true, true, true };

struct SceneObject
{
    std::string name;
    AABB box;
    int light;
};
std::vector<SceneObject> sceneObjects;

glm::vec3 cameraPos = glm::vec3(0.5f, 1.35f, 5.6f);
glm::vec3 cameraFront;
const glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
float yaw = -97.0f;
float pitch = 2.0f;

// คำนวน cameraFront ใหม่จาก yaw pitch
void UpdateCameraFront()
{
    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(direction);
}

// ยิง ray แล้วเอา object ที่ชนก่อน (ใกล้สุด)
const SceneObject* FirstHit(glm::vec3 origin, glm::vec3 dir, float& tHit)
{
    const SceneObject* best = nullptr;
    tHit = 1e30f;
    for (const SceneObject& o : sceneObjects)
    {
        float t = RayAABB(origin, dir, o.box);
        if (t >= 0.0f && t < tHit) { tHit = t; best = &o; }
    }
    return best;
}

// ray ชนระนาบที่ระยะเท่าไหร่ ถ้าขนานคืน -1
float RayPlane(glm::vec3 origin, glm::vec3 dir, glm::vec3 normal, float c)
{
    float denom = glm::dot(normal, dir);
    if (std::abs(denom) < 1e-6f)
        return -1.0f;
    return (c - glm::dot(normal, origin)) / denom;
}

// ขยับกล้อง ถ้าจะชนของก็ไม่ให้ไป แล้วกันไม่ให้จมพื้น
void TryMove(glm::vec3 step)
{
    float length = glm::length(step);
    if (length < 1e-6f)
        return;
    const float radius = 0.3f;
    float t;
    if (FirstHit(cameraPos, step / length, t) && t < length + radius)
        return;
    cameraPos += step;
    float ground = RayPlane(cameraPos, glm::vec3(0, -1, 0), glm::vec3(0, 1, 0), 0.0f);
    if (ground >= 0.0f && ground < 0.4f)
        cameraPos.y += 0.4f - ground;
}

// รับ input คีบอร์ด เดิน/หันกล้อง
void ProcessInput(GLFWwindow* window, float deltaTime)
{
    float moveSpeed = 3.0f * deltaTime;
    float turnSpeed = 60.0f * deltaTime;
    glm::vec3 right = glm::normalize(glm::cross(cameraFront, up));
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) TryMove(cameraFront * moveSpeed);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) TryMove(-cameraFront * moveSpeed);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) TryMove(-right * moveSpeed);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) TryMove(right * moveSpeed);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) TryMove(up * moveSpeed);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) TryMove(-up * moveSpeed);
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  yaw -= turnSpeed;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) yaw += turnSpeed;
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    pitch += turnSpeed;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  pitch -= turnSpeed;
    pitch = std::clamp(pitch, -89.0f, 89.0f);
    UpdateCameraFront();
}

glm::mat4 viewMatrix(1.0f), projectionMatrix(1.0f);

// คลิกเม้าส์ที่โคมไฟ = เปิด/ปิดไฟ (mouse picking)
void OnMouseButton(GLFWwindow* window, int button, int action, int)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
        return;
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    glm::vec2 ndc(2.0f * (float)mx / WIDTH - 1.0f, 1.0f - 2.0f * (float)my / HEIGHT);
    glm::mat4 inverseViewProjection = glm::inverse(projectionMatrix * viewMatrix);
    glm::vec4 farPoint = inverseViewProjection * glm::vec4(ndc, 1.0f, 1.0f);
    glm::vec3 dir = glm::normalize(glm::vec3(farPoint) / farPoint.w - cameraPos);

    float t;
    if (const SceneObject* hit = FirstHit(cameraPos, dir, t))
    {
        std::cout << "Clicked " << hit->name << " at distance " << t;
        if (hit->light >= 0)
        {
            lightOn[hit->light] = !lightOn[hit->light];
            std::cout << " -> light " << (lightOn[hit->light] ? "on" : "off");
        }
        std::cout << std::endl;
    }
    else
    {
        float tGround = RayPlane(cameraPos, dir, glm::vec3(0, 1, 0), 0.0f);
        if (tGround > 0.0f)
        {
            glm::vec3 p = cameraPos + tGround * dir;
            std::cout << "Clicked the footpath at (" << p.x << ", " << p.z << ")" << std::endl;
        }
    }
}

// ยัด vertex 1 ตัว (P N UV) ต่อท้าย array
void PushVertex(std::vector<GLfloat>& v, glm::vec3 p, glm::vec3 n, glm::vec2 uv)
{
    v.insert(v.end(), { p.x, p.y, p.z, n.x, n.y, n.z, uv.x, 1.0f - uv.y });
}

// เอา vertex กับ index ไปสร้าง Mesh
std::unique_ptr<Mesh> MakeMesh(const std::vector<GLfloat>& v, const std::vector<unsigned int>& idx)
{
    auto mesh = std::make_unique<Mesh>();
    mesh->CreateMesh(v.data(), idx.data(), (unsigned int)v.size(), (unsigned int)idx.size());
    return mesh;
}

// กล่อง 1x1x1 กลางอยู่ที่ origin
std::unique_ptr<Mesh> CreateBox()
{
    std::vector<GLfloat> v;
    std::vector<unsigned int> idx;
    const glm::vec3 faces[6][3] =
    {
        { { 0, 0, 1}, { 1, 0, 0}, {0, 1, 0} },
        { { 0, 0,-1}, {-1, 0, 0}, {0, 1, 0} },
        { { 1, 0, 0}, { 0, 0,-1}, {0, 1, 0} },
        { {-1, 0, 0}, { 0, 0, 1}, {0, 1, 0} },
        { { 0, 1, 0}, { 1, 0, 0}, {0, 0,-1} },
        { { 0,-1, 0}, { 1, 0, 0}, {0, 0, 1} },
    };
    for (const auto& f : faces)
    {
        glm::vec3 n = f[0], u = f[1], w = f[2];
        unsigned int start = (unsigned int)(v.size() / 8);
        glm::vec3 c = n * 0.5f;
        PushVertex(v, c - u * 0.5f - w * 0.5f, n, { 0, 0 });
        PushVertex(v, c + u * 0.5f - w * 0.5f, n, { 1, 0 });
        PushVertex(v, c + u * 0.5f + w * 0.5f, n, { 1, 1 });
        PushVertex(v, c - u * 0.5f + w * 0.5f, n, { 0, 1 });
        idx.insert(idx.end(), { start, start + 1, start + 2, start, start + 2, start + 3 });
    }
    return MakeMesh(v, idx);
}

// ทรงกระบอก (หรือกรวยตัดยอด) สูง 1 มีฝาบนล่าง
std::unique_ptr<Mesh> CreateCylinder(int segments, float bottomRadius, float topRadius)
{
    std::vector<GLfloat> v;
    std::vector<unsigned int> idx;
    float slope = (bottomRadius - topRadius);
    for (int i = 0; i <= segments; i++)
    {
        float t = (float)i / segments;
        float a = t * 2.0f * PI;
        glm::vec3 dir(cos(a), 0.0f, sin(a));
        glm::vec3 n = glm::normalize(glm::vec3(dir.x, slope, dir.z));
        PushVertex(v, dir * bottomRadius + glm::vec3(0, -0.5f, 0), n, { t, 0 });
        PushVertex(v, dir * topRadius + glm::vec3(0, 0.5f, 0), n, { t, 1 });
    }
    for (int i = 0; i < segments; i++)
    {
        unsigned int b = i * 2;
        idx.insert(idx.end(), { b, b + 1, b + 3, b, b + 3, b + 2 });
    }
    for (int cap = 0; cap < 2; cap++)
    {
        float y = cap == 0 ? -0.5f : 0.5f;
        float r = cap == 0 ? bottomRadius : topRadius;
        glm::vec3 n(0, cap == 0 ? -1.0f : 1.0f, 0);
        unsigned int centre = (unsigned int)(v.size() / 8);
        PushVertex(v, { 0, y, 0 }, n, { 0.5f, 0.5f });
        for (int i = 0; i <= segments; i++)
        {
            float a = (float)i / segments * 2.0f * PI;
            PushVertex(v, { cos(a) * r, y, sin(a) * r }, n, { 0.5f + 0.5f * cos(a), 0.5f + 0.5f * sin(a) });
        }
        for (int i = 0; i < segments; i++)
            idx.insert(idx.end(), { centre, centre + 1 + i, centre + 2 + i });
    }
    return MakeMesh(v, idx);
}

// ทรงกลม uv sphere รัศมี 0.5
std::unique_ptr<Mesh> CreateSphere(int stacks, int slices)
{
    std::vector<GLfloat> v;
    std::vector<unsigned int> idx;
    for (int i = 0; i <= stacks; i++)
    {
        float phi = PI * i / stacks;
        for (int j = 0; j <= slices; j++)
        {
            float theta = 2.0f * PI * j / slices;
            glm::vec3 n(sin(phi) * cos(theta), cos(phi), sin(phi) * sin(theta));
            PushVertex(v, n * 0.5f, n, { (float)j / slices, 1.0f - (float)i / stacks });
        }
    }
    for (int i = 0; i < stacks; i++)
        for (int j = 0; j < slices; j++)
        {
            unsigned int a = i * (slices + 1) + j;
            unsigned int b = a + slices + 1;
            idx.insert(idx.end(), { a, a + 1, b, a + 1, b + 1, b });
        }
    return MakeMesh(v, idx);
}

// bezier patch จาก control point 4x4 (ใช้ทำผ้าใบ)
std::unique_ptr<Mesh> CreateBezierPatch(const glm::vec3 control[4][4], int steps)
{
    std::vector<GLfloat> v;
    std::vector<unsigned int> idx;
    for (int a = 0; a <= steps; a++)
    {
        float u = (float)a / steps;
        for (int b = 0; b <= steps; b++)
        {
            float w = (float)b / steps;
            glm::vec3 row[4], rowTangentV[4];
            for (int i = 0; i < 4; i++)
            {
                row[i] = BezierCubic(control[i][0], control[i][1], control[i][2], control[i][3], w);
                rowTangentV[i] = BezierCubicTangent(control[i][0], control[i][1], control[i][2], control[i][3], w);
            }
            glm::vec3 p = BezierCubic(row[0], row[1], row[2], row[3], u);
            glm::vec3 dPdu = BezierCubicTangent(row[0], row[1], row[2], row[3], u);
            glm::vec3 dPdv = BezierCubic(rowTangentV[0], rowTangentV[1], rowTangentV[2], rowTangentV[3], u);
            glm::vec3 n = glm::normalize(glm::cross(dPdu, dPdv));
            if (n.y < 0.0f) n = -n;
            PushVertex(v, p, n, { u * 3.0f, w * 3.0f });
        }
    }
    for (int a = 0; a < steps; a++)
        for (int b = 0; b < steps; b++)
        {
            unsigned int i0 = a * (steps + 1) + b, i1 = i0 + steps + 1;
            idx.insert(idx.end(), { i0, i1, i1 + 1, i0, i1 + 1, i0 + 1 });
        }
    return MakeMesh(v, idx);
}

// แอ่งน้ำ สุ่มขอบก่อนแล้ว smooth ด้วย laplacian ไม่งั้นแหลมเกิน
std::unique_ptr<Mesh> CreatePuddle(int segments, float radiusX, float radiusZ, int smoothingIterations)
{
    std::vector<float> radius(segments);
    unsigned int seed = 12345;
    for (float& r : radius)
    {
        seed = seed * 1103515245u + 12345u;
        r = 0.6f + 0.6f * (float)((seed >> 16) & 0x7fff) / 32767.0f;
    }
    for (int it = 0; it < smoothingIterations; it++)
    {
        std::vector<float> next(segments);
        for (int i = 0; i < segments; i++)
        {
            float average = 0.5f * (radius[(i + segments - 1) % segments] + radius[(i + 1) % segments]);
            next[i] = glm::mix(radius[i], average, 0.5f);
        }
        radius = next;
    }

    std::vector<GLfloat> v;
    std::vector<unsigned int> idx;
    glm::vec3 n(0, 1, 0);
    PushVertex(v, { 0, 0, 0 }, n, { 0.5f, 0.5f });
    for (int i = 0; i < segments; i++)
    {
        float a = 2.0f * PI * i / segments;
        glm::vec3 p(cos(a) * radiusX * radius[i], 0.0f, sin(a) * radiusZ * radius[i]);
        PushVertex(v, p, n, { 0.5f + p.x, 0.5f + p.z });
        idx.insert(idx.end(), { 0u, (unsigned int)(1 + (i + 1) % segments), (unsigned int)(1 + i) });
    }
    return MakeMesh(v, idx);
}

// โหลดโมเดล .obj
std::unique_ptr<Mesh> LoadOBJ(const char* path, bool flipV = false)
{
    auto mesh = std::make_unique<Mesh>();
    if (!mesh->CreateMeshFromOBJ(path, flipV))
        std::cerr << "Failed to load " << path << std::endl;
    return mesh;
}

// โหลด texture
std::unique_ptr<Texture> LoadTex(const std::string& path)
{
    auto tex = std::make_unique<Texture>(path);
    tex->LoadTexture();
    return tex;
}

// สร้าง mesh + โหลดโมเดล + texture ทั้งหมดตรงนี้
void CreateObjects()
{
    meshes.box = CreateBox();
    meshes.cylinder = CreateCylinder(32, 0.5f, 0.5f);
    meshes.stool = CreateCylinder(20, 0.5f, 0.42f);
    meshes.sphere = CreateSphere(12, 16);
    meshes.puddle = CreatePuddle(48, 1.15f, 0.65f, 6);

    const glm::vec3 c00(1.30f, 2.45f, -0.45f), c30(3.40f, 2.20f, -0.45f);
    const glm::vec3 c03(1.30f, 2.20f, 1.20f), c33(3.40f, 2.50f, 1.20f);
    glm::vec3 sail[4][4];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
        {
            float u = i / 3.0f, w = j / 3.0f;
            glm::vec3 p = glm::mix(glm::mix(c00, c30, u), glm::mix(c03, c33, u), w);
            bool innerU = i == 1 || i == 2, innerV = j == 1 || j == 2;
            p.y -= (innerU && innerV) ? 0.40f : (innerU || innerV) ? 0.12f : 0.0f;
            sail[i][j] = p;
        }
    meshes.sail = CreateBezierPatch(sail, 24);

    meshes.cart = LoadOBJ("Models/cart/model.obj");
    meshes.lantern = LoadOBJ("Models/lantern/model.obj");
    meshes.table = LoadOBJ("Models/table/model.obj");
    meshes.umbrella = LoadOBJ("Models/umbrella/model.obj");
    meshes.worker = LoadOBJ("Models/worker/model.obj", true);
    meshes.ramen = LoadOBJ("Models/ramen/model.obj", true);

    textures.white = LoadTex("Textures/white.png");
    textures.sidewalk = LoadTex("Textures/sidewalk.png");
    textures.asphalt = LoadTex("Textures/asphalt.png");
    textures.curb = LoadTex("Textures/curb.png");
    textures.shophouse = LoadTex("Textures/shophouse.png");
    textures.stainless = LoadTex("Textures/stainless.png");
    textures.shopSign = LoadTex("Textures/sign_shop.png");
    textures.michelinSign = LoadTex("Textures/sign_michelin.png");
    textures.menuSign = LoadTex("Textures/sign_menu.png");
    textures.worker = LoadTex(meshes.worker->GetDiffuseMap());
    textures.ramen = LoadTex(meshes.ramen->GetDiffuseMap());
}

// โหลด shader ทุกตัว แล้วเก็บ location uniform ไว้
void CreateShaders()
{
    // ทุก pass ใช้ shader.vert กับ shader.frag ไฟล์เดียวกัน ต่างกันแค่ #define ที่เลือก pass
    auto load = [](const char* passDefine)
    {
        auto shader = std::make_unique<Shader>();
        shader->CreateFromFiles(shaderDirectory / "shader.vert", shaderDirectory / "shader.frag", passDefine);
        return shader;
    };
    shaders.main = load(nullptr);
    shaders.depth = load("DEPTH_PASS");
    shaders.pointShadow = load("POINT_SHADOW_PASS");
    shaders.sky = load("SKY_PASS");

    mainUniforms = ObjectUniforms::Locate(*shaders.main);
    depthUniforms = ObjectUniforms::Locate(*shaders.depth);
    pointShadowUniforms = ObjectUniforms::Locate(*shaders.pointShadow);
    pointShadowLightPos = shaders.pointShadow->GetUniformLocation("lightPos");
    pointShadowFarPlane = shaders.pointShadow->GetUniformLocation("farPlane");
    lightingUniforms = LightingUniforms::Locate(*shaders.main);
    skyColourLocation = shaders.sky->GetUniformLocation("skyColour");
}

struct Material
{
    float specularStrength = 0.3f;
    float shininess = 32.0f;
    glm::vec3 emissive = glm::vec3(0.0f);
    glm::vec2 uvScale = glm::vec2(1.0f);
    glm::vec3 tint = glm::vec3(1.0f);
    float roughness = 0.0f;
    bool twoSided = false;
};

const Material MATTE;
const Material STAINLESS = { 1.0f, 128.0f, glm::vec3(0.0f), glm::vec2(1.0f), glm::vec3(0.65f) };
const Material GLOSSY = { 0.6f, 64.0f };

// สร้าง model matrix (translate -> rotate -> scale)
glm::mat4 Transform(glm::vec3 pos, glm::vec3 scale, float rotY = 0.0f, float rotX = 0.0f)
{
    glm::mat4 model(1.0f);
    model = glm::translate(model, pos);
    model = glm::rotate(model, glm::radians(rotY), glm::vec3(0, 1, 0));
    model = glm::rotate(model, glm::radians(rotX), glm::vec3(1, 0, 0));
    model = glm::scale(model, scale);
    return model;
}

// เซ็ต uniform ของ object แล้ววาด
void DrawObject(const Mesh& mesh, const Texture& tex, const glm::mat4& model, const Material& mat = MATTE)
{
    const ObjectUniforms& u = *currentUniforms;
    glUniformMatrix4fv(u.model, 1, GL_FALSE, glm::value_ptr(model));
    if (u.normalMatrix >= 0)
    {
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
        glUniformMatrix3fv(u.normalMatrix, 1, GL_FALSE, glm::value_ptr(normalMatrix));
    }
    glUniform1f(u.specularStrength, mat.specularStrength);
    glUniform1f(u.shininess, mat.shininess);
    glUniform1f(u.roughness, mat.roughness);
    glUniform1i(u.twoSided, mat.twoSided ? 1 : 0);
    glUniform3fv(u.emissive, 1, glm::value_ptr(mat.emissive));
    glUniform2fv(u.uvScale, 1, glm::value_ptr(mat.uvScale));
    glUniform3fv(u.tint, 1, glm::value_ptr(mat.tint));
    tex.UseTexture();
    mesh.RenderMesh();
}

// วาดกล่องจากมุม min max (จัดฉากง่ายกว่า)
void DrawBox(glm::vec3 minCorner, glm::vec3 maxCorner, const Texture& tex, const Material& mat = MATTE)
{
    DrawObject(*meshes.box, tex, Transform((minCorner + maxCorner) * 0.5f, maxCorner - minCorner), mat);
}

// วาดแท่งบางๆ จาก a ไป b ใช้ทำสายไฟกับเสา
void DrawSegment(glm::vec3 a, glm::vec3 b, float radius, const Material& mat)
{
    glm::vec3 axisY = b - a;
    float length = glm::length(axisY);
    axisY /= length;
    glm::vec3 helper = std::abs(axisY.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 axisX = glm::normalize(glm::cross(helper, axisY));
    glm::vec3 axisZ = glm::cross(axisX, axisY);
    float diameter = radius * 2.0f;
    glm::mat4 model(glm::vec4(axisX * diameter, 0.0f), glm::vec4(axisY * length, 0.0f),
                    glm::vec4(axisZ * diameter, 0.0f), glm::vec4((a + b) * 0.5f, 1.0f));
    DrawObject(*meshes.cylinder, *textures.white, model, mat);
}

// วาดเก้าอี้พลาสติก
void DrawStool(glm::vec3 pos, glm::vec3 colour)
{
    Material plastic = GLOSSY;
    plastic.tint = colour;
    DrawObject(*meshes.stool, *textures.white, Transform(pos + glm::vec3(0, 0.22f, 0), glm::vec3(0.34f, 0.44f, 0.34f)), plastic);
}

// วาดโต๊ะ + ชามบะหมี่
void DrawTableWithBowls(glm::vec3 pos, float rotY, int bowls)
{
    DrawObject(*meshes.table, *textures.white, Transform(pos + glm::vec3(0, 0.40f, 0), glm::vec3(0.65f), rotY), STAINLESS);
    float topY = pos.y + 0.40f + 0.535f * 0.65f;
    for (int i = 0; i < bowls; i++)
    {
        float along = (i - (bowls - 1) * 0.5f) * 0.55f;
        glm::vec3 offset = glm::vec3(glm::rotate(glm::mat4(1.0f), glm::radians(rotY), glm::vec3(0, 1, 0)) *
                                     glm::vec4(0.0f, 0.0f, along, 1.0f));
        DrawObject(*meshes.ramen, *textures.ramen, Transform(pos + offset + glm::vec3(0, topY + 0.01f, 0), glm::vec3(0.2f), 30.0f * i), GLOSSY);
    }
}

// ป้าย michelin ตั้งพื้นแบบตัว A
void DrawSandwichBoard(glm::vec3 pos, float rotY, const Material& front)
{
    const float h = 0.80f, w = 0.64f, t = 0.03f;
    const float tilt = 14.0f;
    float a = glm::radians(tilt);
    float cy = 0.5f * h * cos(a);
    float cz = 0.5f * h * sin(a) + 0.5f * t;
    glm::mat4 base = glm::rotate(glm::translate(glm::mat4(1.0f), pos), glm::radians(rotY), glm::vec3(0, 1, 0));
    glm::mat4 frontBoard = glm::translate(base, glm::vec3(0, cy, cz)) *
                           glm::rotate(glm::mat4(1.0f), -a, glm::vec3(1, 0, 0)) *
                           glm::scale(glm::mat4(1.0f), glm::vec3(w, h, t));
    glm::mat4 backBoard = glm::translate(base, glm::vec3(0, cy, -cz)) *
                          glm::rotate(glm::mat4(1.0f), a, glm::vec3(1, 0, 0)) *
                          glm::scale(glm::mat4(1.0f), glm::vec3(w, h, t));
    DrawObject(*meshes.box, *textures.michelinSign, frontBoard, front);
    Material wood = MATTE; wood.tint = glm::vec3(0.45f, 0.30f, 0.20f);
    DrawObject(*meshes.box, *textures.white, backBoard, wood);
}

// สายไฟ bezier + หลอดไฟหลายสี (สีจาก hsv)
void DrawBulbString(const BulbString& s, int stringIndex)
{
    Material wire = MATTE; wire.tint = glm::vec3(0.08f);
    const int wireSegments = 16;
    for (int i = 0; i < wireSegments; i++)
        DrawSegment(PointOnString(s, (float)i / wireSegments), PointOnString(s, (float)(i + 1) / wireSegments), 0.006f, wire);
    if (currentPass == Pass::LocalShadow)
        return;

    for (int i = 0; i < s.bulbs; i++)
    {
        Material bulb = MATTE;
        float hue = (float)i / s.bulbs * 0.6f + stringIndex * 0.33f;
        bulb.tint = HsvToRgb(hue, 0.45f, 1.0f);
        bulb.emissive = glm::vec3(1.0f);
        glm::vec3 p = PointOnString(s, (i + 0.5f) / s.bulbs) - glm::vec3(0.0f, 0.04f, 0.0f);
        DrawObject(*meshes.sphere, *textures.white, Transform(p, glm::vec3(0.07f)), bulb);
    }
}

// วาดทั้งฉาก เรียกหลายรอบ (shadow pass / กล้อง)
void RenderScene(const glm::mat4& view, const glm::mat4& projection)
{
    glUniformMatrix4fv(currentUniforms->view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(currentUniforms->projection, 1, GL_FALSE, glm::value_ptr(projection));

    Material tiles = MATTE;  tiles.uvScale = glm::vec2(16.0f, 5.0f); tiles.roughness = 0.6f; tiles.specularStrength = 0.1f;
    DrawBox({ -12.0f, -0.20f, -4.5f }, { 12.0f, 0.0f, 1.4f }, *textures.sidewalk, tiles);
    Material kerb = MATTE;   kerb.uvScale = glm::vec2(24.0f, 1.0f); kerb.roughness = 0.5f;
    DrawBox({ -12.0f, -0.20f, 1.4f }, { 12.0f, 0.02f, 1.65f }, *textures.curb, kerb);
    Material road = MATTE;   road.uvScale = glm::vec2(12.0f, 4.0f); road.roughness = 0.8f; road.specularStrength = 0.05f;
    DrawBox({ -12.0f, -0.40f, 1.65f }, { 12.0f, -0.18f, 12.0f }, *textures.asphalt, road);
    Material facade = MATTE; facade.uvScale = glm::vec2(6.0f, facadeHeight / 6.0f); facade.roughness = 0.5f;
    DrawBox({ -12.0f, 0.0f, -5.0f }, { 12.0f, facadeHeight, -4.5f }, *textures.shophouse, facade);
    Material cornice = MATTE; cornice.tint = glm::vec3(0.55f, 0.45f, 0.40f);
    DrawBox({ -12.0f, facadeHeight, -5.0f }, { 12.0f, facadeHeight + 0.18f, -4.35f }, *textures.white, cornice);

    DrawObject(*meshes.cart, *textures.white, Transform(cartPos, glm::vec3(cartScale)), GLOSSY);

    Material stove = MATTE; stove.tint = glm::vec3(0.15f);
    DrawObject(*meshes.cylinder, *textures.white, Transform(potPos + glm::vec3(0, 0.05f, 0), { 0.42f, 0.10f, 0.42f }), stove);
    DrawObject(*meshes.cylinder, *textures.stainless, Transform(potPos + glm::vec3(0, 0.25f, 0), { 0.36f, 0.30f, 0.36f }), STAINLESS);
    DrawObject(*meshes.cylinder, *textures.stainless, Transform(potPos + glm::vec3(0, 0.41f, 0), { 0.38f, 0.02f, 0.38f }), STAINLESS);

    bool drawFixtures = currentPass != Pass::LocalShadow;
    Material neon = MATTE; neon.emissive = glm::vec3(lightOn[2] ? 1.0f : 0.0f); neon.tint = glm::vec3(0.8f, 0.95f, 1.0f);
    if (drawFixtures)
        DrawObject(*meshes.cylinder, *textures.white, Transform(neonPos, { 1.3f, 0.07f, 0.07f }) *
                   glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0, 0, 1)), neon);

    Material sign = MATTE; sign.emissive = glm::vec3(0.5f);
    DrawObject(*meshes.box, *textures.shopSign, Transform(shopSignPos, { 1.7f, 0.43f, 0.04f }), sign);
    DrawSandwichBoard({ -2.45f, 0.0f, 0.7f }, 25.0f, sign);
    Material menuBoard = MATTE; menuBoard.emissive = glm::vec3(0.8f);
    DrawObject(*meshes.box, *textures.menuSign, Transform({ 1.25f, 2.35f, -4.47f }, { 1.2f, 0.9f, 0.04f }), menuBoard);

    if (drawFixtures)
    {
        Material lantern = MATTE;
        lantern.emissive = glm::vec3(lightOn[0] ? 1.0f : 0.1f);
        DrawObject(*meshes.lantern, *textures.white, Transform(lanternLeftPos, glm::vec3(0.55f)), lantern);
        lantern.emissive = glm::vec3(lightOn[1] ? 1.0f : 0.1f);
        DrawObject(*meshes.lantern, *textures.white, Transform(lanternRightPos, glm::vec3(0.55f)), lantern);
    }

    for (size_t i = 0; i < bulbStrings.size(); i++)
        DrawBulbString(bulbStrings[i], (int)i);

    Material fabric = MATTE; fabric.tint = glm::vec3(0.25f, 0.75f, 0.80f); fabric.twoSided = true;
    fabric.specularStrength = 0.15f;
    DrawObject(*meshes.sail, *textures.white, glm::mat4(1.0f), fabric);
    Material steel = STAINLESS; steel.tint = glm::vec3(0.35f);
    for (glm::vec3 corner : { glm::vec3(1.30f, 2.45f, -0.45f), glm::vec3(3.40f, 2.20f, -0.45f),
                              glm::vec3(1.30f, 2.20f, 1.20f), glm::vec3(3.40f, 2.50f, 1.20f) })
        DrawSegment(glm::vec3(corner.x, 0.0f, corner.z), corner, 0.03f, steel);
    DrawSegment(lampPos + glm::vec3(0, 0.08f, 0), glm::vec3(lampPos.x, 2.12f, lampPos.z), 0.006f, steel);
    if (drawFixtures)
    {
        Material shade = GLOSSY; shade.tint = glm::vec3(0.15f, 0.15f, 0.17f);
        DrawObject(*meshes.stool, *textures.white, Transform(lampPos + glm::vec3(0, 0.05f, 0), glm::vec3(0.30f, 0.14f, 0.30f)), shade);
        Material led = MATTE; led.emissive = glm::vec3(lightOn[spotLightIndex] ? 1.0f : 0.1f); led.tint = glm::vec3(0.8f, 0.92f, 1.0f);
        DrawObject(*meshes.cylinder, *textures.white, Transform(lampPos - glm::vec3(0, 0.02f, 0), glm::vec3(0.18f, 0.01f, 0.18f)), led);
    }

    DrawObject(*meshes.worker, *textures.worker, Transform({ -2.55f, 0.0f, -1.45f }, glm::vec3(0.72f), 35.0f));
    Material partner = MATTE; partner.tint = glm::vec3(0.85f, 0.95f, 1.1f);
    DrawObject(*meshes.worker, *textures.worker, Transform({ 0.75f, 0.0f, -1.05f }, glm::vec3(0.72f), 20.0f), partner);

    DrawTableWithBowls(frontTablePos, 90.0f, 3);
    DrawTableWithBowls({ 3.3f, 0.0f, -2.3f }, 90.0f, 2);
    glm::vec3 red(0.85f, 0.12f, 0.10f), blue(0.15f, 0.30f, 0.80f);
    DrawStool({ 1.4f, 0.0f, 1.05f }, red);
    DrawStool({ 2.4f, 0.0f, 1.05f }, blue);
    DrawStool({ 1.5f, 0.0f, -0.45f }, blue);
    DrawStool({ 2.6f, 0.0f, -0.45f }, red);
    DrawStool({ 2.9f, 0.0f, -1.5f }, red);
    DrawStool({ 3.8f, 0.0f, -1.5f }, blue);

    DrawObject(*meshes.umbrella, *textures.white, Transform({ 3.3f, 1.08f, -2.3f }, glm::vec3(4.0f)));
}

// uniform แสงที่ไม่เปลี่ยน เซ็ตทีเดียวพอ
void SetStaticLightUniforms(const glm::mat4& sunLightSpace, const glm::mat4& spotLightSpace)
{
    const LightingUniforms& u = lightingUniforms;
    shaders.main->UseShader();
    glUniform3fv(u.sunDirection, 1, glm::value_ptr(sunDirection));
    glUniform3fv(u.sunColour, 1, glm::value_ptr(sunColour));
    glUniform3fv(u.ambientColour, 1, glm::value_ptr(ambientColour));
    glUniform1f(u.ambientStrength, ambientStrength);
    glUniform1i(u.texture2D, 0);
    glUniform1i(u.shadowMap, 1);
    glUniform1i(u.spotShadowMap, 2);
    glUniform1i(u.pointShadowMap, 3);
    glUniformMatrix4fv(u.lightSpace, 1, GL_FALSE, glm::value_ptr(sunLightSpace));
    glUniformMatrix4fv(u.spotLightSpace, 1, GL_FALSE, glm::value_ptr(spotLightSpace));
    glUniform1i(u.shadowedPointLight, shadowedPointLight);
    glUniform1f(u.pointShadowFar, pointShadowFar);
    glUniform1i(u.numPointLights, (GLint)pointLights.size());
    for (size_t i = 0; i < pointLights.size(); i++)
    {
        const PointLightUniforms& p = u.pointLights[i];
        glUniform3fv(p.position, 1, glm::value_ptr(pointLights[i].position));
        glUniform1f(p.constant, pointLights[i].constant);
        glUniform1f(p.linear, pointLights[i].linear);
        glUniform1f(p.quadratic, pointLights[i].quadratic);
    }
    glUniform3fv(u.spotPosition, 1, glm::value_ptr(spotLight.position));
    glUniform3fv(u.spotDirection, 1, glm::value_ptr(spotLight.direction));
    glUniform1f(u.spotConstant, spotLight.constant);
    glUniform1f(u.spotLinear, spotLight.linear);
    glUniform1f(u.spotQuadratic, spotLight.quadratic);
    glUniform1f(u.spotCutOff, std::cos(glm::radians(spotLight.cutOffDegrees)));
    glm::mat4 identity(1.0f);
    glUniformMatrix4fv(u.reflection, 1, GL_FALSE, glm::value_ptr(identity));
    glUniform4f(u.clipPlane, 0.0f, 0.0f, 0.0f, 1.0f);
}

// uniform แสงที่เปลี่ยนทุกเฟรม (ตำแหน่งกล้อง เปิดปิดไฟ)
void SetFrameLightUniforms(glm::vec3 eye)
{
    const LightingUniforms& u = lightingUniforms;
    glUniform3fv(u.viewPos, 1, glm::value_ptr(eye));
    for (size_t i = 0; i < pointLights.size(); i++)
    {
        glm::vec3 colour = lightOn[i] ? pointLights[i].colour : glm::vec3(0.0f);
        glUniform3fv(u.pointLights[i].colour, 1, glm::value_ptr(colour));
    }
    glm::vec3 spotColour = lightOn[spotLightIndex] ? spotLight.colour : glm::vec3(0.0f);
    glUniform3fv(u.spotColour, 1, glm::value_ptr(spotColour));
    glUniform1i(u.shadingModel, shadingModel);
    glUniform1i(u.flatShading, flatShading ? 1 : 0);
}

// สามเหลี่ยมเต็มจอ ไม่มี vertex data (ไว้ทำท้องฟ้า)
void DrawFullScreenTriangle()
{
    glBindVertexArray(emptyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

// วาดแอ่งน้ำด้วย depth shader เอาแค่ stencil
void DrawPuddleSurface(const glm::mat4& view, const glm::mat4& projection)
{
    shaders.depth->UseShader();
    currentUniforms = &depthUniforms;
    glm::mat4 model = glm::translate(glm::mat4(1.0f), puddleCentre + glm::vec3(0.0f, puddleY, 0.0f));
    glUniformMatrix4fv(depthUniforms.model, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(depthUniforms.view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(depthUniforms.projection, 1, GL_FALSE, glm::value_ptr(projection));
    meshes.puddle->RenderMesh();
}

// เงาสะท้อนในแอ่งน้ำ ใช้ stencil buffer
void DrawPuddleReflection(const glm::mat4& view, const glm::mat4& projection)
{
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
    DrawPuddleSurface(view, projection);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);

    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glDepthFunc(GL_ALWAYS);
    shaders.sky->UseShader();
    glUniform3fv(skyColourLocation, 1, glm::value_ptr(skyColour));
    DrawFullScreenTriangle();
    glDepthFunc(GL_LESS);

    const glm::mat4 mirror = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
    const LightingUniforms& u = lightingUniforms;
    shaders.main->UseShader();
    currentUniforms = &mainUniforms;
    glUniformMatrix4fv(u.reflection, 1, GL_FALSE, glm::value_ptr(mirror));
    glUniform4f(u.clipPlane, 0.0f, 1.0f, 0.0f, -0.005f);
    glUniform3fv(u.viewPos, 1, glm::value_ptr(glm::vec3(mirror * glm::vec4(cameraPos, 1.0f))));
    glEnable(GL_CLIP_DISTANCE0);
    RenderScene(view, projection);
    glDisable(GL_CLIP_DISTANCE0);
    glm::mat4 identity(1.0f);
    glUniformMatrix4fv(u.reflection, 1, GL_FALSE, glm::value_ptr(identity));
    glUniform4f(u.clipPlane, 0.0f, 0.0f, 0.0f, 1.0f);
    glUniform3fv(u.viewPos, 1, glm::value_ptr(cameraPos));
    glDisable(GL_STENCIL_TEST);
}

// แคปจอเซฟเป็น png
void SaveScreenshot(const std::filesystem::path& file)
{
    std::vector<unsigned char> pixels(WIDTH * HEIGHT * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    stbi_flip_vertically_on_write(1);
    std::filesystem::create_directories(file.parent_path());
    if (stbi_write_png(file.u8string().c_str(), WIDTH, HEIGHT, 3, pixels.data(), WIDTH * 3))
        std::cout << "Saved " << file.u8string() << std::endl;
    else
        std::cerr << "Failed to save " << file.u8string() << std::endl;
}

// ลบของ GL ทิ้งให้หมดก่อนปิด context
void Cleanup()
{
    meshes = Meshes();
    textures = Textures();
    shaders = Shaders();
}

// ทำ bounding box ของ object ไว้ใช้ตอนคลิกกับตอนเดินชน
void BuildSceneObjects()
{
    auto add = [](const std::string& name, const Mesh& mesh, const glm::mat4& model, int light = -1)
    {
        sceneObjects.push_back({ name, mesh.GetBounds().Transformed(model), light });
    };
    auto addBox = [](const std::string& name, glm::vec3 lo, glm::vec3 hi, int light = -1)
    {
        AABB box; box.Expand(lo); box.Expand(hi);
        sceneObjects.push_back({ name, box, light });
    };
    add("left lantern", *meshes.lantern, Transform(lanternLeftPos, glm::vec3(0.55f)), 0);
    add("right lantern", *meshes.lantern, Transform(lanternRightPos, glm::vec3(0.55f)), 1);
    addBox("neon tube", neonPos - glm::vec3(0.65f, 0.06f, 0.06f), neonPos + glm::vec3(0.65f, 0.06f, 0.06f), 2);
    addBox("pendant lamp", lampPos - glm::vec3(0.2f, 0.1f, 0.2f), lampPos + glm::vec3(0.2f, 0.15f, 0.2f), spotLightIndex);
    add("noodle cart", *meshes.cart, Transform(cartPos, glm::vec3(cartScale)));
    add("cook", *meshes.worker, Transform({ -2.55f, 0.0f, -1.45f }, glm::vec3(0.72f), 35.0f));
    add("server", *meshes.worker, Transform({ 0.75f, 0.0f, -1.05f }, glm::vec3(0.72f), 20.0f));
    add("front table", *meshes.table, Transform(frontTablePos + glm::vec3(0, 0.40f, 0), glm::vec3(0.65f), 90.0f));
    add("back table", *meshes.table, Transform(glm::vec3(3.3f, 0.40f, -2.3f), glm::vec3(0.65f), 90.0f));
    for (glm::vec3 p : { glm::vec3(1.4f, 0, 1.05f), glm::vec3(2.4f, 0, 1.05f), glm::vec3(1.5f, 0, -0.45f),
                         glm::vec3(2.6f, 0, -0.45f), glm::vec3(2.9f, 0, -1.5f), glm::vec3(3.8f, 0, -1.5f) })
        add("stool", *meshes.stool, Transform(p + glm::vec3(0, 0.22f, 0), glm::vec3(0.34f, 0.44f, 0.34f)));
    for (glm::vec3 p : { glm::vec3(1.30f, 0, -0.45f), glm::vec3(3.40f, 0, -0.45f), glm::vec3(1.30f, 0, 1.2f), glm::vec3(3.40f, 0, 1.2f) })
        addBox("sail pole", p - glm::vec3(0.05f, 0, 0.05f), p + glm::vec3(0.05f, 2.5f, 0.05f));
    addBox("Michelin board", { -2.8f, 0.0f, 0.35f }, { -2.1f, 0.8f, 1.05f });
    addBox("shophouses", { -12.0f, 0.0f, -5.0f }, { 12.0f, facadeHeight, -4.5f });
}

// main: เปิดหน้าต่าง, ทำ shadow map รอบเดียว, แล้ววน loop วาด
int main(int argc, char** argv)
{
    bool autoScreenshot = argc > 1 && std::strcmp(argv[1], "--screenshot") == 0;

    if (!glfwInit())
    {
        std::cerr << "GLFW initialisation failed" << std::endl;
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Assignment2 - FUTURE ME", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "GLFW window creation failed" << std::endl;
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        std::cerr << "GLEW initialisation failed" << std::endl;
        glfwTerminate();
        return 1;
    }
    glGetError();
    glEnable(GL_DEPTH_TEST);
    glfwSetMouseButtonCallback(window, OnMouseButton);
    int bufferWidth, bufferHeight;
    glfwGetFramebufferSize(window, &bufferWidth, &bufferHeight);

    CreateShaders();
    CreateObjects();
    BuildSceneObjects();
    UpdateCameraFront();

    projectionMatrix = glm::perspective(glm::radians(45.0f), (GLfloat)WIDTH / (GLfloat)HEIGHT, 0.1f, 100.0f);

    auto sunShadow = std::make_unique<ShadowMap>(4096);
    auto spotShadow = std::make_unique<ShadowMap>(1024);
    auto lanternShadow = std::make_unique<CubeShadowMap>(1024, pointShadowFar);
    glGenVertexArrays(1, &emptyVAO);

    const glm::vec3 shadowCentre(0.5f, 0.0f, -1.5f);
    glm::mat4 sunView = glm::lookAt(shadowCentre - sunDirection * 20.0f, shadowCentre, up);
    AABB street;
    street.Expand({ -6.0f, -0.2f, -5.0f });
    street.Expand({ 7.0f, facadeHeight + 0.2f, 1.7f });
    AABB inLight = street.Transformed(sunView);
    glm::mat4 sunProjection = glm::ortho(inLight.min.x, inLight.max.x, inLight.min.y, inLight.max.y,
                                         -inLight.max.z - 1.0f, -inLight.min.z + 1.0f);
    glm::mat4 sunLightSpace = sunProjection * sunView;

    glm::mat4 spotView = glm::lookAt(spotLight.position, spotLight.position + spotLight.direction, glm::vec3(0, 0, -1));
    glm::mat4 spotProjection = glm::perspective(glm::radians(2.0f * spotLight.cutOffDegrees + 6.0f), 1.0f, 0.05f, 6.0f);
    glm::mat4 spotLightSpace = spotProjection * spotView;

    shaders.depth->UseShader();
    currentUniforms = &depthUniforms;
    currentPass = Pass::SunShadow;
    sunShadow->BeginPass();
    RenderScene(sunView, sunProjection);
    currentPass = Pass::LocalShadow;
    spotShadow->BeginPass();
    RenderScene(spotView, spotProjection);
    shaders.pointShadow->UseShader();
    currentUniforms = &pointShadowUniforms;
    glm::vec3 lanternLight = pointLights[shadowedPointLight].position;
    glUniform3fv(pointShadowLightPos, 1, glm::value_ptr(lanternLight));
    glUniform1f(pointShadowFarPlane, pointShadowFar);
    for (int face = 0; face < 6; face++)
    {
        lanternShadow->BeginFace(face);
        RenderScene(CubeShadowMap::FaceView(lanternLight, face), lanternShadow->Projection());
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    currentPass = Pass::Camera;
    SetStaticLightUniforms(sunLightSpace, spotLightSpace);

    std::cout << "WASD/QE move, arrows look, click a lamp to switch it, 1-4 shading model, F flat shading,\n"
                 "P = screenshot, C = print camera" << std::endl;
    const char* modelNames[] = { "Blinn-Phong", "Phong", "Lambert only", "cel / toon" };
    float lastTime = (float)glfwGetTime();
    int frame = 0;
    bool pWasDown = false, cWasDown = false, fWasDown = false;
    while (!glfwWindowShouldClose(window))
    {
        float now = (float)glfwGetTime();
        float deltaTime = now - lastTime;
        lastTime = now;
        glfwPollEvents();
        ProcessInput(window, deltaTime);
        for (int k = 0; k < 4; k++)
            if (glfwGetKey(window, GLFW_KEY_1 + k) == GLFW_PRESS && shadingModel != k)
            {
                shadingModel = k;
                std::cout << "Shading model: " << modelNames[k] << std::endl;
            }
        bool fDown = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
        if (fDown && !fWasDown)
        {
            flatShading = !flatShading;
            std::cout << (flatShading ? "Flat shading" : "Phong interpolation (smooth normals)") << std::endl;
        }
        fWasDown = fDown;

        glm::vec3 cameraDirection = cameraFront;
        glm::vec3 cameraRight = glm::normalize(glm::cross(cameraDirection, up));
        glm::vec3 cameraUp = glm::normalize(glm::cross(cameraRight, cameraDirection));
        viewMatrix = glm::lookAt(cameraPos, cameraPos + cameraDirection, cameraUp);
        const glm::mat4& view = viewMatrix;
        const glm::mat4& projection = projectionMatrix;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, bufferWidth, bufferHeight);
        glClearColor(skyColour.r, skyColour.g, skyColour.b, 1.0f);
        glClearStencil(0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        shaders.main->UseShader();
        currentUniforms = &mainUniforms;
        SetFrameLightUniforms(cameraPos);
        sunShadow->BindTexture(GL_TEXTURE1);
        spotShadow->BindTexture(GL_TEXTURE2);
        lanternShadow->BindTexture(GL_TEXTURE3);
        RenderScene(view, projection);
        DrawPuddleReflection(view, projection);

        bool pDown = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
        bool cDown = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
        if ((pDown && !pWasDown) || (autoScreenshot && ++frame == 10))
        {
            SaveScreenshot(screenshotDirectory / "scene.png");
            if (autoScreenshot) glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        if (cDown && !cWasDown)
            std::cout << "cameraPos(" << cameraPos.x << ", " << cameraPos.y << ", " << cameraPos.z
                      << ") yaw " << yaw << " pitch " << pitch << std::endl;
        pWasDown = pDown;
        cWasDown = cDown;

        glfwSwapBuffers(window);
    }
    glDeleteVertexArrays(1, &emptyVAO);
    lanternShadow.reset();
    spotShadow.reset();
    sunShadow.reset();
    Cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
