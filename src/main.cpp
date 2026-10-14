// Assignment 2 - "FUTURE ME"
// Our noodle cart on the footpath at sunset, Michelin Guide 2040.
#undef GLFW_DLL
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
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

// All paths are relative to the working directory (the folder holding
// Shaders/, Models/, Textures/ - CMake copies them next to the .exe)
const std::filesystem::path shaderDirectory = "Shaders";
const std::filesystem::path screenshotDirectory = "Screenshots";

// ================= Shader / Texture / Mesh helpers =================

class Shader
{
public:
    ~Shader() { if (program) glDeleteProgram(program); }

    void CreateFromFiles(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath)
    {
        std::string vertexCode = ReadFile(vertexPath);
        std::string fragmentCode = ReadFile(fragmentPath);
        if (vertexCode.empty() || fragmentCode.empty())
            return;
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
            std::cerr << "Error linking shader program: " << log << std::endl;
        }
    }

    void UseShader() const { glUseProgram(program); }
    GLint GetUniformLocation(const char* name) const { return glGetUniformLocation(program, name); }

private:
    GLuint program = 0;

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
    ~Texture() { if (textureID) glDeleteTextures(1, &textureID); }

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
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);   // rows of RGB images are not always 4-byte aligned
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(data);
        return true;
    }

    void UseTexture() const
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
    }

private:
    GLuint textureID = 0;
    std::string fileLocation;
};

// Interleaved vertex: position, normal, uv, colour (Kd from .mtl, white otherwise)
struct Vertex
{
    glm::vec3 position{};
    glm::vec3 normal{};
    glm::vec2 uv{};
    glm::vec3 colour{ 1.0f };
};

// Directory part of a path, including the trailing slash ("" if none)
std::string DirectoryOf(const std::string& path)
{
    size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}

std::string TrimLine(std::string s)
{
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ')) s.pop_back();
    return s;
}

class Mesh
{
public:
    ~Mesh()
    {
        if (IBO) glDeleteBuffers(1, &IBO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (VAO) glDeleteVertexArrays(1, &VAO);
    }

    // From an interleaved float array: 8 floats per vertex P(3) N(3) UV(2)
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

    // Load an .obj (v, vt, vn, f; n-gons are fan-triangulated).
    // mtllib/usemtl: Kd becomes the vertex colour, the first map_Kd is kept in diffuseMap.
    bool CreateMeshFromOBJ(const std::string& path, bool flipV = false)
    {
        std::ifstream in(path);
        if (!in)
            return false;

        std::vector<glm::vec3> V, VN;
        std::vector<glm::vec2> VT;
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        std::unordered_map<std::string, unsigned int> dedup;   // "v/vt/vn/material" -> index
        std::unordered_map<std::string, glm::vec3> materials;  // material name -> Kd
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

                    // parse v, v/vt, v//vn or v/vt/vn (negative = relative to the end)
                    int idx[3] = { 0, 0, 0 };
                    std::istringstream ts(tok);
                    std::string part;
                    for (int k = 0; k < 3 && std::getline(ts, part, '/'); k++)
                        if (!part.empty()) idx[k] = std::stoi(part);
                    auto fix = [](int i, size_t count) { return i > 0 ? i - 1 : i < 0 ? (int)count + i : -1; };
                    int iv = fix(idx[0], V.size()), ivt = fix(idx[1], VT.size()), ivn = fix(idx[2], VN.size());
                    if (iv < 0) continue;

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
        Upload(vertices, indices);
        return true;
    }

    void RenderMesh() const
    {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    const std::string& GetDiffuseMap() const { return diffuseMap; }

private:
    GLuint VAO = 0, VBO = 0, IBO = 0;
    GLsizei indexCount = 0;
    std::string diffuseMap;

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

    // VAO + interleaved VBO + EBO
    // attribute locations: 0 position, 1 uv, 2 normal, 3 colour (match shader.vert)
    void Upload(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
    {
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

// ===================================================================

Shader* mainShader = nullptr;
Shader* depthShader = nullptr;
Shader* currentShader = nullptr;   // shader used by DrawObject (main or depth pass)

// uniform locations of the shader in use (Lab 7 / Lab 13)
GLuint uniformModel = 0, uniformView = 0, uniformProjection = 0;

// meshes built from vertices in code
Mesh* boxMesh = nullptr;
Mesh* quadMesh = nullptr;
Mesh* cylinderMesh = nullptr;
Mesh* stoolMesh = nullptr;
// external models
Mesh* cartMesh = nullptr;
Mesh* workerMesh = nullptr;
Mesh* lanternMesh = nullptr;
Mesh* tableMesh = nullptr;
Mesh* umbrellaMesh = nullptr;
Mesh* ramenMesh = nullptr;

Texture* whiteTex = nullptr;
Texture* sidewalkTex = nullptr;
Texture* asphaltTex = nullptr;
Texture* curbTex = nullptr;
Texture* shophouseTex = nullptr;
Texture* stainlessTex = nullptr;
Texture* shopSignTex = nullptr;
Texture* michelinTex = nullptr;
Texture* workerTex = nullptr;
Texture* ramenTex = nullptr;

// ---------------- lights ----------------
struct PointLight
{
    glm::vec3 position;
    glm::vec3 colour;
    float constant, linear, quadratic;
};

// sunset sun: low, from the front-left
glm::vec3 sunDirection = glm::normalize(glm::vec3(0.85f, -0.5f, -0.75f));
glm::vec3 sunColour = glm::vec3(1.0f, 0.62f, 0.32f) * 0.75f;
// ambient (Lab 11): ambient = ambientColour * ambientStrength -> (0.24, 0.19, 0.30) evening sky
glm::vec3 ambientColour = glm::vec3(0.8f, 0.633f, 1.0f);
float ambientStrength = 0.3f;

// cart: model scaled 0.7 at cartPos, awning x[-2.17,-0.60] y[1.96,2.37] z[-2.46,-1.01], counter top y 1.19
const glm::vec3 cartPos         = glm::vec3(-0.9f, 1.05f, -1.4f);
const float cartScale           = 0.7f;
const float counterY            = 1.19f;
const glm::vec3 lanternLeftPos  = glm::vec3(-2.10f, 1.74f, -0.98f);
const glm::vec3 lanternRightPos = glm::vec3(-0.66f, 1.74f, -0.98f);
const glm::vec3 neonPos         = glm::vec3(-1.38f, 1.88f, -1.06f);
const glm::vec3 potPos          = glm::vec3(-1.55f, counterY, -1.50f);

std::vector<PointLight> pointLights =
{
    { lanternLeftPos,  glm::vec3(1.0f, 0.42f, 0.18f) * 2.0f, 1.0f, 0.35f, 0.45f },
    { lanternRightPos, glm::vec3(1.0f, 0.42f, 0.18f) * 2.0f, 1.0f, 0.35f, 0.45f },
    { neonPos,         glm::vec3(0.85f, 0.95f, 1.0f) * 1.4f, 1.0f, 0.22f, 0.20f },
    { potPos + glm::vec3(0.0f, 0.12f, 0.35f), glm::vec3(1.0f, 0.38f, 0.12f) * 0.8f, 1.0f, 0.70f, 1.80f },
};

// ---------------- camera ----------------
glm::vec3 cameraPos = glm::vec3(0.5f, 1.5f, 5.4f);
glm::vec3 cameraFront;
glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
float yaw = -97.0f;
float pitch = -3.0f;

void UpdateCameraFront()
{
    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(direction);
}

void ProcessInput(GLFWwindow* window, float deltaTime)
{
    float moveSpeed = 3.0f * deltaTime;
    float turnSpeed = 60.0f * deltaTime;
    glm::vec3 right = glm::normalize(glm::cross(cameraFront, up));
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraPos += cameraFront * moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraPos -= cameraFront * moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cameraPos -= right * moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cameraPos += right * moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) cameraPos += up * moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) cameraPos -= up * moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  yaw -= turnSpeed;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) yaw += turnSpeed;
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    pitch += turnSpeed;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  pitch -= turnSpeed;
    pitch = std::clamp(pitch, -89.0f, 89.0f);
    UpdateCameraFront();
}

// ---------------- meshes from vertices ----------------
// Vertex layout for Mesh::CreateMesh: P(3) N(3) UV(2)
// (V is flipped because stb_image loads images top row first)
void PushVertex(std::vector<GLfloat>& v, glm::vec3 p, glm::vec3 n, glm::vec2 uv)
{
    v.insert(v.end(), { p.x, p.y, p.z, n.x, n.y, n.z, uv.x, 1.0f - uv.y });
}

// Unit box centred at the origin, each face has its own normal and 0..1 UVs
Mesh* CreateBox()
{
    std::vector<GLfloat> v;
    std::vector<unsigned int> idx;
    // normal, u axis, v axis (u x v = normal so triangles are counter-clockwise)
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
    Mesh* mesh = new Mesh();
    mesh->CreateMesh(v.data(), idx.data(), (unsigned int)v.size(), (unsigned int)idx.size());
    return mesh;
}

// Unit quad in the XY plane facing +Z
Mesh* CreateQuad()
{
    std::vector<GLfloat> v;
    glm::vec3 n(0, 0, 1);
    PushVertex(v, { -0.5f, -0.5f, 0 }, n, { 0, 0 });
    PushVertex(v, {  0.5f, -0.5f, 0 }, n, { 1, 0 });
    PushVertex(v, {  0.5f,  0.5f, 0 }, n, { 1, 1 });
    PushVertex(v, { -0.5f,  0.5f, 0 }, n, { 0, 1 });
    unsigned int idx[] = { 0, 1, 2, 0, 2, 3 };
    Mesh* mesh = new Mesh();
    mesh->CreateMesh(v.data(), idx, (unsigned int)v.size(), 6);
    return mesh;
}

// Cylinder (or cone frustum) of height 1 centred at the origin, with caps
Mesh* CreateCylinder(int segments, float bottomRadius, float topRadius)
{
    std::vector<GLfloat> v;
    std::vector<unsigned int> idx;
    float slope = (bottomRadius - topRadius);   // for the side normal of a frustum
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
    // caps
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
    Mesh* mesh = new Mesh();
    mesh->CreateMesh(v.data(), idx.data(), (unsigned int)v.size(), (unsigned int)idx.size());
    return mesh;
}

Mesh* LoadOBJ(const char* path, bool flipV = false)
{
    Mesh* mesh = new Mesh();
    if (!mesh->CreateMeshFromOBJ(path, flipV))
        std::cout << "Failed to load " << path << std::endl;
    return mesh;
}

Texture* LoadTex(const std::string& path)
{
    Texture* tex = new Texture(path);
    tex->LoadTexture();
    return tex;
}

void CreateObjects()
{
    boxMesh = CreateBox();
    quadMesh = CreateQuad();
    cylinderMesh = CreateCylinder(32, 0.5f, 0.5f);
    stoolMesh = CreateCylinder(20, 0.5f, 0.42f);

    cartMesh = LoadOBJ("Models/cart/model.obj");
    lanternMesh = LoadOBJ("Models/lantern/model.obj");
    tableMesh = LoadOBJ("Models/table/model.obj");
    umbrellaMesh = LoadOBJ("Models/umbrella/model.obj");
    workerMesh = LoadOBJ("Models/worker/model.obj", true);
    ramenMesh = LoadOBJ("Models/ramen/model.obj", true);

    whiteTex = LoadTex("Textures/white.png");
    sidewalkTex = LoadTex("Textures/sidewalk.png");
    asphaltTex = LoadTex("Textures/asphalt.png");
    curbTex = LoadTex("Textures/curb.png");
    shophouseTex = LoadTex("Textures/shophouse.png");
    stainlessTex = LoadTex("Textures/stainless.png");
    shopSignTex = LoadTex("Textures/sign_shop.png");
    michelinTex = LoadTex("Textures/sign_michelin.png");
    workerTex = LoadTex(workerMesh->GetDiffuseMap());
    ramenTex = LoadTex(ramenMesh->GetDiffuseMap());
}

void CreateShaders()
{
    mainShader = new Shader();
    mainShader->CreateFromFiles(shaderDirectory / "shader.vert", shaderDirectory / "shader.frag");
    depthShader = new Shader();
    depthShader->CreateFromFiles(shaderDirectory / "depthShader.vert", shaderDirectory / "depthShader.frag");
}

// ---------------- drawing ----------------
struct Material
{
    float specularStrength = 0.3f;
    float shininess = 16.0f;
    glm::vec3 emissive = glm::vec3(0.0f);
    glm::vec2 uvScale = glm::vec2(1.0f);
    glm::vec3 tint = glm::vec3(1.0f);
};

const Material MATTE;                                   // painted / plastic / cloth
const Material STAINLESS = { 1.0f, 64.0f, glm::vec3(0.0f), glm::vec2(1.0f), glm::vec3(0.65f) };  // shiny steel
const Material GLOSSY = { 0.6f, 32.0f };                // ceramic bowl, glossy paint

glm::mat4 Transform(glm::vec3 pos, glm::vec3 scale, float rotY = 0.0f, float rotX = 0.0f)
{
    glm::mat4 model(1.0f);
    model = glm::translate(model, pos);
    model = glm::rotate(model, glm::radians(rotY), glm::vec3(0, 1, 0));
    model = glm::rotate(model, glm::radians(rotX), glm::vec3(1, 0, 0));
    model = glm::scale(model, scale);
    return model;
}

void DrawObject(Mesh* mesh, Texture* tex, const glm::mat4& model, const Material& mat = MATTE)
{
    glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
    // material uniforms do not exist in the depth shader (location -1 is ignored)
    glUniform1f(currentShader->GetUniformLocation("specularStrength"), mat.specularStrength);
    glUniform1f(currentShader->GetUniformLocation("shininess"), mat.shininess);
    glUniform3fv(currentShader->GetUniformLocation("emissive"), 1, glm::value_ptr(mat.emissive));
    glUniform2fv(currentShader->GetUniformLocation("uvScale"), 1, glm::value_ptr(mat.uvScale));
    glUniform3fv(currentShader->GetUniformLocation("tint"), 1, glm::value_ptr(mat.tint));
    tex->UseTexture();
    mesh->RenderMesh();
}

// Box defined by its min/max corners (easier to lay out the street)
void DrawBox(glm::vec3 minCorner, glm::vec3 maxCorner, Texture* tex, const Material& mat = MATTE)
{
    DrawObject(boxMesh, tex, Transform((minCorner + maxCorner) * 0.5f, maxCorner - minCorner), mat);
}

void DrawStool(glm::vec3 pos, glm::vec3 colour)
{
    Material plastic = GLOSSY;
    plastic.tint = colour;
    DrawObject(stoolMesh, whiteTex, Transform(pos + glm::vec3(0, 0.22f, 0), glm::vec3(0.34f, 0.44f, 0.34f)), plastic);
}

void DrawTableWithBowls(glm::vec3 pos, float rotY, int bowls)
{
    // folding table model: y -0.615..0.535 (scaled 0.65 -> ~0.75 m tall)
    DrawObject(tableMesh, whiteTex, Transform(pos + glm::vec3(0, 0.40f, 0), glm::vec3(0.65f), rotY), STAINLESS);
    float topY = pos.y + 0.40f + 0.535f * 0.65f;
    for (int i = 0; i < bowls; i++)
    {
        float along = (i - (bowls - 1) * 0.5f) * 0.55f;
        glm::vec3 offset = glm::vec3(glm::rotate(glm::mat4(1.0f), glm::radians(rotY), glm::vec3(0, 1, 0)) *
                                     glm::vec4(0.0f, 0.0f, along, 1.0f));
        DrawObject(ramenMesh, ramenTex, Transform(pos + offset + glm::vec3(0, topY + 0.01f, 0), glm::vec3(0.2f), 30.0f * i), GLOSSY);
    }
}

// A-frame board standing on the footpath: two boards hinged at the top
void DrawSandwichBoard(glm::vec3 pos, float rotY, const Material& front)
{
    const float h = 0.80f, w = 0.64f, t = 0.03f;   // board size
    const float tilt = 14.0f;                      // each board leans this many degrees
    float a = glm::radians(tilt);
    // board centre measured from the hinge line (top edge sits on the hinge)
    float cy = 0.5f * h * cos(a);
    float cz = 0.5f * h * sin(a) + 0.5f * t;
    glm::mat4 base = glm::rotate(glm::translate(glm::mat4(1.0f), pos), glm::radians(rotY), glm::vec3(0, 1, 0));
    glm::mat4 frontBoard = glm::translate(base, glm::vec3(0, cy, cz)) *
                           glm::rotate(glm::mat4(1.0f), -a, glm::vec3(1, 0, 0)) *
                           glm::scale(glm::mat4(1.0f), glm::vec3(w, h, t));
    glm::mat4 backBoard = glm::translate(base, glm::vec3(0, cy, -cz)) *
                          glm::rotate(glm::mat4(1.0f), a, glm::vec3(1, 0, 0)) *
                          glm::scale(glm::mat4(1.0f), glm::vec3(w, h, t));
    DrawObject(boxMesh, michelinTex, frontBoard, front);
    Material wood = MATTE; wood.tint = glm::vec3(0.45f, 0.30f, 0.20f);
    DrawObject(boxMesh, whiteTex, backBoard, wood);
}

// Everything that casts/receives shadows. Called twice per frame (Lab 13):
// once with the light's view/projection (depth map), once with the camera's.
void RenderScene(glm::mat4 view, glm::mat4 projection)
{
    glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));

    // ---- street ----
    Material tiles = MATTE;  tiles.uvScale = glm::vec2(16.0f, 5.0f);
    DrawBox({ -12.0f, -0.20f, -4.5f }, { 12.0f, 0.0f, 1.4f }, sidewalkTex, tiles);          // footpath
    Material kerb = MATTE;   kerb.uvScale = glm::vec2(24.0f, 1.0f);
    DrawBox({ -12.0f, -0.20f, 1.4f }, { 12.0f, 0.02f, 1.65f }, curbTex, kerb);              // red/white kerb
    Material road = MATTE;   road.uvScale = glm::vec2(12.0f, 4.0f);
    DrawBox({ -12.0f, -0.40f, 1.65f }, { 12.0f, -0.18f, 12.0f }, asphaltTex, road);         // road
    Material facade = MATTE; facade.uvScale = glm::vec2(6.0f, 1.0f);
    DrawBox({ -12.0f, 0.0f, -5.0f }, { 12.0f, 6.0f, -4.5f }, shophouseTex, facade);         // shophouses

    // ---- noodle cart (model: x -2.06..2.36, y -1.50..2.55) ----
    DrawObject(cartMesh, whiteTex, Transform(cartPos, glm::vec3(cartScale)), GLOSSY);

    // stove + pot of boiling soup on the cart counter
    Material stove = MATTE; stove.tint = glm::vec3(0.15f);
    DrawObject(cylinderMesh, whiteTex, Transform(potPos + glm::vec3(0, 0.05f, 0), { 0.42f, 0.10f, 0.42f }), stove);
    DrawObject(cylinderMesh, stainlessTex, Transform(potPos + glm::vec3(0, 0.25f, 0), { 0.36f, 0.30f, 0.36f }), STAINLESS);
    DrawObject(cylinderMesh, stainlessTex, Transform(potPos + glm::vec3(0, 0.41f, 0), { 0.38f, 0.02f, 0.38f }), STAINLESS);

    // neon tube under the awning (emissive)
    Material neon = MATTE; neon.emissive = glm::vec3(1.0f);
    DrawObject(cylinderMesh, whiteTex, Transform(neonPos, { 1.3f, 0.07f, 0.07f }) *
               glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0, 0, 1)), neon);   // lying along X

    // shop name sign above the awning + Michelin 2040 board on the footpath
    Material sign = MATTE; sign.emissive = glm::vec3(0.35f);
    DrawObject(boxMesh, shopSignTex, Transform({ -1.38f, 2.66f, -1.25f }, { 1.7f, 0.43f, 0.04f }), sign);
    DrawSandwichBoard({ -2.45f, 0.0f, 0.7f }, 25.0f, sign);

    // red lanterns (emissive) hanging at the awning corners
    Material lantern = MATTE; lantern.emissive = glm::vec3(0.8f);
    DrawObject(lanternMesh, whiteTex, Transform(lanternLeftPos, glm::vec3(0.55f)), lantern);
    DrawObject(lanternMesh, whiteTex, Transform(lanternRightPos, glm::vec3(0.55f)), lantern);

    // ---- us: two owners behind the cart (model height 2.3, faces +Z) ----
    // one cooks at the pot beside the cart, the other serves behind the counter
    DrawObject(workerMesh, workerTex, Transform({ -2.55f, 0.0f, -1.45f }, glm::vec3(0.72f), 35.0f));
    Material partner = MATTE; partner.tint = glm::vec3(0.85f, 0.95f, 1.1f);
    DrawObject(workerMesh, workerTex, Transform({ 0.75f, 0.0f, -1.05f }, glm::vec3(0.72f), 20.0f), partner);

    // ---- customer tables, stools and noodles ----
    DrawTableWithBowls({ 2.0f, 0.0f, 0.3f }, 90.0f, 3);
    DrawTableWithBowls({ 3.3f, 0.0f, -2.3f }, 90.0f, 2);
    glm::vec3 red(0.85f, 0.12f, 0.10f), blue(0.15f, 0.30f, 0.80f);
    DrawStool({ 1.4f, 0.0f, 1.05f }, red);
    DrawStool({ 2.4f, 0.0f, 1.05f }, blue);
    DrawStool({ 1.5f, 0.0f, -0.45f }, blue);
    DrawStool({ 2.6f, 0.0f, -0.45f }, red);
    DrawStool({ 2.9f, 0.0f, -1.5f }, red);
    DrawStool({ 3.8f, 0.0f, -1.5f }, blue);

    // umbrella over the back table (model height ~0.67 -> ~2.7 m)
    DrawObject(umbrellaMesh, whiteTex, Transform({ 3.3f, 1.08f, -2.3f }, glm::vec3(4.0f)));
}

void SetLightUniforms(const glm::mat4& lightSpace)
{
    Shader* s = mainShader;
    glUniform3fv(s->GetUniformLocation("sun.direction"), 1, glm::value_ptr(sunDirection));
    glUniform3fv(s->GetUniformLocation("sun.colour"), 1, glm::value_ptr(sunColour));
    glUniform3fv(s->GetUniformLocation("ambientColour"), 1, glm::value_ptr(ambientColour));
    glUniform1f(s->GetUniformLocation("ambientStrength"), ambientStrength);
    glUniform1i(s->GetUniformLocation("numPointLights"), (GLint)pointLights.size());
    for (size_t i = 0; i < pointLights.size(); i++)
    {
        std::string base = "pointLights[" + std::to_string(i) + "].";
        glUniform3fv(s->GetUniformLocation((base + "position").c_str()), 1, glm::value_ptr(pointLights[i].position));
        glUniform3fv(s->GetUniformLocation((base + "colour").c_str()), 1, glm::value_ptr(pointLights[i].colour));
        glUniform1f(s->GetUniformLocation((base + "constant").c_str()), pointLights[i].constant);
        glUniform1f(s->GetUniformLocation((base + "linear").c_str()), pointLights[i].linear);
        glUniform1f(s->GetUniformLocation((base + "quadratic").c_str()), pointLights[i].quadratic);
    }
    glUniform3fv(s->GetUniformLocation("viewPos"), 1, glm::value_ptr(cameraPos));
    glUniformMatrix4fv(s->GetUniformLocation("lightSpace"), 1, GL_FALSE, glm::value_ptr(lightSpace));
    glUniform1i(s->GetUniformLocation("texture2D"), 0);
    glUniform1i(s->GetUniformLocation("shadowMap"), 1);
}

// Save the current frame (800 x 600) as PNG
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
        std::cout << "Failed to save " << file.u8string() << std::endl;
}

void Cleanup()
{
    for (Mesh* m : { boxMesh, quadMesh, cylinderMesh, stoolMesh, cartMesh, workerMesh,
                     lanternMesh, tableMesh, umbrellaMesh, ramenMesh })
        delete m;
    for (Texture* t : { whiteTex, sidewalkTex, asphaltTex, curbTex, shophouseTex, stainlessTex,
                        shopSignTex, michelinTex, workerTex, ramenTex })
        delete t;
    delete mainShader;
    delete depthShader;
}

int main(int argc, char** argv)
{
    // --screenshot : render a few frames, save Screenshots/scene.png and exit
    bool autoScreenshot = argc > 1 && std::strcmp(argv[1], "--screenshot") == 0;

    // window: OpenGL 3.3 core profile, fixed 800 x 600 (not resizable so the image never stretches)
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
    glGetError();   // glewInit may leave a harmless GL_INVALID_ENUM in core profile
    glEnable(GL_DEPTH_TEST);
    int bufferWidth, bufferHeight;
    glfwGetFramebufferSize(window, &bufferWidth, &bufferHeight);

    CreateShaders();
    CreateObjects();
    UpdateCameraFront();

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (GLfloat)WIDTH / (GLfloat)HEIGHT, 0.1f, 100.0f);

    // shadow map for the sun
    const unsigned int SHADOW_WIDTH = 4096, SHADOW_HEIGHT = 4096;
    GLuint depthMapFBO, depthMap;
    glGenFramebuffers(1, &depthMapFBO);
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0,
        GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColour[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColour);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // light view: from the sun's position looking at the middle of the scene;
    // orthographic projection because the sun is a directional light (Lab 13)
    const glm::vec3 shadowCentre(0.5f, 0.0f, -1.5f);
    glm::vec3 lightPos = shadowCentre - sunDirection * 20.0f;
    glm::mat4 lightProjection = glm::ortho(-9.0f, 9.0f, -9.0f, 9.0f, 0.1f, 40.0f);
    glm::mat4 lightView = glm::lookAt(lightPos, shadowCentre, up);
    glm::mat4 lightSpace = lightProjection * lightView;

    std::cout << "WASD/QE move, arrows look, P = screenshot, C = print camera" << std::endl;
    float lastTime = (float)glfwGetTime();
    int frame = 0;
    bool pWasDown = false, cWasDown = false;
    while (!glfwWindowShouldClose(window))
    {
        float now = (float)glfwGetTime();
        float deltaTime = now - lastTime;
        lastTime = now;
        glfwPollEvents();
        ProcessInput(window, deltaTime);
        // camera (Lab 8): right and up vectors from the viewing direction
        glm::vec3 cameraDirection = cameraFront;
        glm::vec3 cameraRight = glm::normalize(glm::cross(cameraDirection, up));
        glm::vec3 cameraUp = glm::normalize(glm::cross(cameraRight, cameraDirection));
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraDirection, cameraUp);

        // first pass: depth map from the sun
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        currentShader = depthShader;
        depthShader->UseShader();
        uniformModel = depthShader->GetUniformLocation("model");
        uniformView = depthShader->GetUniformLocation("view");
        uniformProjection = depthShader->GetUniformLocation("projection");
        // front-face culling against peter panning (Lab 13). GL_CULL_FACE is left
        // disabled because some models (umbrella, cart awning) are single-sided,
        // so these calls only take effect if culling is turned on.
        glCullFace(GL_FRONT);
        RenderScene(lightView, lightProjection);
        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // second pass: the scene
        glViewport(0, 0, bufferWidth, bufferHeight);
        glClearColor(0.96f, 0.58f, 0.36f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        currentShader = mainShader;
        mainShader->UseShader();
        uniformModel = mainShader->GetUniformLocation("model");
        uniformView = mainShader->GetUniformLocation("view");
        uniformProjection = mainShader->GetUniformLocation("projection");
        SetLightUniforms(lightSpace);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        glActiveTexture(GL_TEXTURE0);
        RenderScene(view, projection);
        glUseProgram(0);

        // P: screenshot, C: print camera (for composing the final shot)
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
    glDeleteTextures(1, &depthMap);
    glDeleteFramebuffers(1, &depthMapFBO);
    Cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
