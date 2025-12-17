#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <cmath>
#include <random>

using namespace glm;

// --- Shader class ---
class Shader {
public:
    unsigned int ID;
    Shader(const char* vertexSource, const char* fragmentSource) {
        unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vertexSource, NULL);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");

        unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fragmentSource, NULL);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");

        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");

        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }

    void use() { glUseProgram(ID); }
    void setMat4(const std::string& name, const mat4& mat) { glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, value_ptr(mat)); }
    void setVec3(const std::string& name, const vec3& vec) { glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, value_ptr(vec)); }
    void setFloat(const std::string& name, float value) { glUniform1f(glGetUniformLocation(ID, name.c_str()), value); }
    void setInt(const std::string& name, int value) { glUniform1i(glGetUniformLocation(ID, name.c_str()), value); }

private:
    void checkCompileErrors(unsigned int shader, std::string type) {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n";
            }
        }
        else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n";
            }
        }
    }
};

// --- Texture loader ---
unsigned int loadTexture(const char* path, bool repeat = true) {
    sf::Image image;
    if (!image.loadFromFile(path)) {
        std::cout << "Failed to load texture: " << path << std::endl;
        // Возвращаем 0, но программа продолжит работу, возможно с черной текстурой
        return 0;
    }

    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.getSize().x, image.getSize().y, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.getPixelsPtr());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (repeat) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    return textureID;
}

// --- Mesh Logic ---
struct Mesh {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    unsigned int VAO, VBO, EBO;
    unsigned int texture, normalMap = 0;

    void setup() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(3); glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(8 * sizeof(float)));
        glEnableVertexAttribArray(4); glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(11 * sizeof(float)));
    }

    void draw(Shader& shader) const {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        shader.setInt("texture1", 0);
        if (normalMap) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, normalMap);
            shader.setInt("normalMap", 1);
            shader.setInt("useNormalMap", 1);
        }
        else {
            shader.setInt("useNormalMap", 0);
        }
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
    }
};

Mesh generateCube(float size, unsigned int tex) {
    Mesh mesh;
    float half = size / 2.0f;
    mesh.vertices = {
        // Front
        -half, -half,  half,  0, 0, 1,  0, 0,  1, 0, 0,  0, 1, 0,
         half, -half,  half,  0, 0, 1,  1, 0,  1, 0, 0,  0, 1, 0,
         half,  half,  half,  0, 0, 1,  1, 1,  1, 0, 0,  0, 1, 0,
        -half,  half,  half,  0, 0, 1,  0, 1,  1, 0, 0,  0, 1, 0,
        // Back
        -half, -half, -half,  0, 0,-1,  1, 0, -1, 0, 0,  0, 1, 0,
         half, -half, -half,  0, 0,-1,  0, 0, -1, 0, 0,  0, 1, 0,
         half,  half, -half,  0, 0,-1,  0, 1, -1, 0, 0,  0, 1, 0,
        -half,  half, -half,  0, 0,-1,  1, 1, -1, 0, 0,  0, 1, 0,
        // Top
        -half,  half, -half,  0, 1, 0,  0, 1,  1, 0, 0,  0, 0,-1,
         half,  half, -half,  0, 1, 0,  1, 1,  1, 0, 0,  0, 0,-1,
         half,  half,  half,  0, 1, 0,  1, 0,  1, 0, 0,  0, 0,-1,
        -half,  half,  half,  0, 1, 0,  0, 0,  1, 0, 0,  0, 0,-1,
        // Bottom
        -half, -half, -half,  0,-1, 0,  0, 0,  1, 0, 0,  0, 0, 1,
         half, -half, -half,  0,-1, 0,  1, 0,  1, 0, 0,  0, 0, 1,
         half, -half,  half,  0,-1, 0,  1, 1,  1, 0, 0,  0, 0, 1,
        -half, -half,  half,  0,-1, 0,  0, 1,  1, 0, 0,  0, 0, 1,
        // Right
         half, -half, -half,  1, 0, 0,  1, 0,  0, 0,-1,  0, 1, 0,
         half,  half, -half,  1, 0, 0,  1, 1,  0, 0,-1,  0, 1, 0,
         half,  half,  half,  1, 0, 0,  0, 1,  0, 0,-1,  0, 1, 0,
         half, -half,  half,  1, 0, 0,  0, 0,  0, 0,-1,  0, 1, 0,
         // Left
         -half, -half, -half, -1, 0, 0,  0, 0,  0, 0, 1,  0, 1, 0,
         -half,  half, -half, -1, 0, 0,  0, 1,  0, 0, 1,  0, 1, 0,
         -half,  half,  half, -1, 0, 0,  1, 1,  0, 0, 1,  0, 1, 0,
         -half, -half,  half, -1, 0, 0,  1, 0,  0, 0, 1,  0, 1, 0
    };
    mesh.indices = {
        0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4, 8, 9, 10, 10, 11, 8,
        12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20
    };
    mesh.texture = tex;
    mesh.setup();
    return mesh;
}

Mesh generateCone(float radius, float height, int segments, unsigned int tex) {
    Mesh mesh;
    float angleStep = 2.0f * 3.14159f / segments;
    // Base center
    mesh.vertices.insert(mesh.vertices.end(), { 0.0f, 0.0f, 0.0f,  0.0f, -1.0f, 0.0f,  0.5f, 0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f });
    // Apex
    mesh.vertices.insert(mesh.vertices.end(), { 0.0f, height, 0.0f,  0.0f, 1.0f, 0.0f,  0.5f, 0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f });

    for (int i = 0; i < segments; ++i) {
        float angle = i * angleStep;
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        float u = (x / radius + 1.0f) * 0.5f;
        float v = (z / radius + 1.0f) * 0.5f;
        // Base vertex
        mesh.vertices.insert(mesh.vertices.end(), { x, 0.0f, z, 0.0f, -1.0f, 0.0f, u, v, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f });
        // Side Vertex
        vec3 sideNormal = normalize(vec3(x, radius / height, z));
        mesh.vertices.insert(mesh.vertices.end(), { x, 0.0f, z, sideNormal.x, sideNormal.y, sideNormal.z, (float)i / segments, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f });
    }
    // Indices Base
    for (int i = 0; i < segments; ++i) {
        mesh.indices.push_back(0); mesh.indices.push_back(2 + i * 2); mesh.indices.push_back(2 + ((i + 1) % segments) * 2);
    }
    // Indices Sides
    for (int i = 0; i < segments; ++i) {
        mesh.indices.push_back(1); mesh.indices.push_back(2 + ((i + 1) % segments) * 2 + 1); mesh.indices.push_back(2 + i * 2 + 1);
    }
    mesh.texture = tex;
    mesh.setup();
    return mesh;
}

Mesh generateCylinder(float radius, float height, int segments, unsigned int tex) {
    Mesh mesh;
    float angleStep = 2.0f * 3.14159f / segments;
    // Center points
    mesh.vertices.insert(mesh.vertices.end(), { 0.0f, 0.0f, 0.0f,  0.0f, -1.0f, 0.0f,  0.5f, 0.5f,  1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f });
    mesh.vertices.insert(mesh.vertices.end(), { 0.0f, height, 0.0f, 0.0f, 1.0f, 0.0f,  0.5f, 0.5f,  1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f });

    for (int i = 0; i < segments; ++i) {
        float angle = i * angleStep;
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        float u = (float)i / segments;
        // Bottom rim
        mesh.vertices.insert(mesh.vertices.end(), { x, 0.0f, z, 0.0f, -1.0f, 0.0f, u, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f });
        // Top rim
        mesh.vertices.insert(mesh.vertices.end(), { x, height, z, 0.0f, 1.0f, 0.0f, u, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f });
        // Side Bottom
        vec3 n = normalize(vec3(x, 0, z));
        mesh.vertices.insert(mesh.vertices.end(), { x, 0.0f, z, n.x, n.y, n.z, u, 1.0f, -z, 0.0f, x, 0.0f, 1.0f, 0.0f });
        // Side Top
        mesh.vertices.insert(mesh.vertices.end(), { x, height, z, n.x, n.y, n.z, u, 0.0f, -z, 0.0f, x, 0.0f, 1.0f, 0.0f });
    }

    int baseOffset = 2;
    for (int i = 0; i < segments; ++i) {
        int next = (i + 1) % segments;
        // Bottom cap
        mesh.indices.push_back(0); mesh.indices.push_back(baseOffset + i * 4); mesh.indices.push_back(baseOffset + next * 4);
        // Top cap
        mesh.indices.push_back(1); mesh.indices.push_back(baseOffset + next * 4 + 1); mesh.indices.push_back(baseOffset + i * 4 + 1);
        // Sides
        int bl = baseOffset + i * 4 + 2; int tl = baseOffset + i * 4 + 3; int br = baseOffset + next * 4 + 2; int tr = baseOffset + next * 4 + 3;
        mesh.indices.push_back(bl); mesh.indices.push_back(tr); mesh.indices.push_back(tl);
        mesh.indices.push_back(bl); mesh.indices.push_back(br); mesh.indices.push_back(tr);
    }
    mesh.texture = tex;
    mesh.setup();
    return mesh;
}

Mesh generateEllipsoid(float rx, float ry, float rz, int slices, int stacks, unsigned int tex, unsigned int normal = 0) {
    Mesh mesh;
    for (int i = 0; i <= stacks; ++i) {
        float phi = 3.14159f * i / stacks;
        for (int j = 0; j <= slices; ++j) {
            float theta = 2.0f * 3.14159f * j / slices;
            float x = rx * cos(theta) * sin(phi);
            float y = ry * cos(phi);
            float z = rz * sin(theta) * sin(phi);
            vec3 normalVec = normalize(vec3(x / rx, y / ry, z / rz));

            mesh.vertices.insert(mesh.vertices.end(), { x, y, z, normalVec.x, normalVec.y, normalVec.z, (float)j / slices, (float)i / stacks });
            vec3 tangent = normalize(vec3(-sin(theta), 0, cos(theta)));
            vec3 bitangent = cross(normalVec, tangent);
            mesh.vertices.insert(mesh.vertices.end(), { tangent.x, tangent.y, tangent.z, bitangent.x, bitangent.y, bitangent.z });
        }
    }
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            int first = (i * (slices + 1)) + j; int second = first + slices + 1;
            mesh.indices.push_back(first); mesh.indices.push_back(second); mesh.indices.push_back(first + 1);
            mesh.indices.push_back(second); mesh.indices.push_back(second + 1); mesh.indices.push_back(first + 1);
        }
    }
    mesh.texture = tex;
    mesh.normalMap = normal;
    mesh.setup();
    return mesh;
}

Mesh generateTerrain(int width, int depth, unsigned int tex, unsigned int heightTex) {
    Mesh mesh;
    for (int z = 0; z <= depth; ++z) {
        for (int x = 0; x <= width; ++x) {
            float u = (float)x / width;
            float v = (float)z / depth;
            mesh.vertices.insert(mesh.vertices.end(), { (float)x - width / 2.0f, 0.0f, (float)z - depth / 2.0f, 0.0f, 1.0f, 0.0f, u * 10.0f, v * 10.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f });
        }
    }
    for (int z = 0; z < depth; ++z) {
        for (int x = 0; x < width; ++x) {
            int topLeft = (z * (width + 1)) + x; int topRight = topLeft + 1; int bottomLeft = ((z + 1) * (width + 1)) + x; int bottomRight = bottomLeft + 1;
            mesh.indices.push_back(topLeft); mesh.indices.push_back(bottomLeft); mesh.indices.push_back(topRight);
            mesh.indices.push_back(topRight); mesh.indices.push_back(bottomLeft); mesh.indices.push_back(bottomRight);
        }
    }
    mesh.texture = tex;
    mesh.setup();
    return mesh;
}

struct Parcel {
    vec3 position;
    vec3 velocity = vec3(0, -9.8f, 0);
    Mesh mesh;
    float radius = 0.5f;
    bool active = true;
};

struct Target {
    vec3 position;
    Mesh body;
    Mesh roof;
    float radius = 2.5f;
    bool active = true;
};

// NEW: Decoration struct for tree ornaments
struct Decoration {
    Mesh mesh;
    vec3 relativePos; // Position relative to the tree base
};

float getTerrainHeight(float worldX, float worldZ, const sf::Image& heightMap, float terrainScale, float terrainHeightScale) {
    float mapSize = 100.0f * terrainScale;
    float halfSize = mapSize / 2.0f;
    if (worldX < -halfSize || worldX > halfSize || worldZ < -halfSize || worldZ > halfSize) return 0.0f;
    float u = (worldX + halfSize) / mapSize;
    float v = (worldZ + halfSize) / mapSize;
    unsigned int x = (unsigned int)(u * heightMap.getSize().x);
    unsigned int y = (unsigned int)(v * heightMap.getSize().y);
    if (x >= heightMap.getSize().x) x = heightMap.getSize().x - 1;
    if (y >= heightMap.getSize().y) y = heightMap.getSize().y - 1;
    sf::Color c = heightMap.getPixel(x, y);
    return (c.r / 255.0f) * terrainHeightScale;
}

int main() {
    sf::ContextSettings settings;
    settings.depthBits = 24; settings.stencilBits = 8; settings.antialiasingLevel = 4;
    settings.majorVersion = 3; settings.minorVersion = 3;

    sf::Window window(sf::VideoMode(800, 600), "Christmas Delivery", sf::Style::Default, settings);
    window.setActive(true);
    window.setFramerateLimit(60);

    if (!gladLoadGL()) { std::cout << "Failed to initialize GLAD" << std::endl; return -1; }
    glEnable(GL_DEPTH_TEST);

    // --- Shaders ---
    const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aNormal;
        layout (location = 2) in vec2 aTexCoords;
        layout (location = 3) in vec3 aTangent;
        layout (location = 4) in vec3 aBitangent;
        out vec3 FragPos; out vec3 Normal; out vec2 TexCoords; out mat3 TBN;
        uniform mat4 model; uniform mat4 view; uniform mat4 projection; uniform sampler2D heightMap; uniform bool isTerrain;
        void main() {
            vec3 pos = aPos;
            if (isTerrain) { float height = texture(heightMap, aTexCoords / 10.0).r * 10.0; pos.y += height; }
            FragPos = vec3(model * vec4(pos, 1.0)); Normal = mat3(transpose(inverse(model))) * aNormal; TexCoords = aTexCoords;
            vec3 T = normalize(vec3(model * vec4(aTangent, 0.0))); vec3 B = normalize(vec3(model * vec4(aBitangent, 0.0))); vec3 N = normalize(vec3(model * vec4(aNormal, 0.0)));
            TBN = mat3(T, B, N); gl_Position = projection * view * vec4(FragPos, 1.0);
        }
    )";
    const char* fragmentShaderSource = R"(
        #version 330 core
        out vec4 FragColor;
        in vec3 FragPos; in vec3 Normal; in vec2 TexCoords; in mat3 TBN;
        uniform sampler2D texture1; uniform sampler2D normalMap; uniform vec3 lightDir; uniform vec3 viewPos; uniform int useNormalMap;
        void main() {
            vec3 norm;
            if (useNormalMap == 1) { vec3 normal = texture(normalMap, TexCoords).rgb; normal = normal * 2.0 - 1.0; norm = normalize(TBN * normal); } else { norm = normalize(Normal); }
            vec3 color = texture(texture1, TexCoords).rgb;
            vec3 ambient = 0.3 * color; float diff = max(dot(norm, -lightDir), 0.0); vec3 diffuse = diff * color;
            vec3 viewDir = normalize(viewPos - FragPos); vec3 halfwayDir = normalize(-lightDir + viewDir);
            float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0); vec3 specular = vec3(0.3) * spec;
            FragColor = vec4(ambient + diffuse + specular, 1.0);
        }
    )";
    Shader shader(vertexShaderSource, fragmentShaderSource);

    // --- Loading Textures ---
    unsigned int grassTex = loadTexture("grass.jpg");
    unsigned int treeBarkTex = loadTexture("tree_bark.jpg");
    unsigned int treeLeavesTex = loadTexture("tree_leaves.jpg");
    unsigned int airshipTex = loadTexture("airship_tex.jpg");
    unsigned int airshipNormal = loadTexture("airship_normal.jpg", false);
    unsigned int houseTex = loadTexture("house_tex.jpg");
    unsigned int parcelTex = loadTexture("parcel_tex.jpg");
    unsigned int heightMapTex = loadTexture("heightmap.jpg", false);

    // NEW: Decoration Textures
    std::vector<unsigned int> ballTexs;
    ballTexs.push_back(loadTexture("ball_tree1.jpg"));
    ballTexs.push_back(loadTexture("ball_tree2.jpg"));
    ballTexs.push_back(loadTexture("ball_tree3.jpg"));
    ballTexs.push_back(loadTexture("ball_tree4.jpg"));
    ballTexs.push_back(loadTexture("ball_tree5.jpg"));
    unsigned int starTex = loadTexture("star.jpg");

    sf::Image heightMapImage;
    if (!heightMapImage.loadFromFile("heightmap.jpg")) std::cout << "Error loading heightmap image!" << std::endl;

    // --- Generate Models ---
    Mesh terrain = generateTerrain(100, 100, grassTex, heightMapTex);
    Mesh trunk = generateCylinder(1.5f, 15.0f, 32, treeBarkTex);
    Mesh branch1 = generateCone(6.0f, 6.0f, 32, treeLeavesTex);
    Mesh branch2 = generateCone(5.0f, 5.0f, 32, treeLeavesTex);
    Mesh branch3 = generateCone(4.0f, 4.0f, 32, treeLeavesTex);
    Mesh balloon = generateEllipsoid(5.0f, 3.0f, 3.0f, 32, 32, airshipTex, airshipNormal);
    Mesh gondola = generateCube(2.0f, airshipTex);
    Mesh parcelMesh = generateCube(1.0f, parcelTex);
    Mesh houseBody = generateCube(4.0f, houseTex);
    Mesh houseRoof = generateCone(3.5f, 3.0f, 4, houseTex);

    // NEW: Generate Decorations
    std::vector<Decoration> treeDecorations;
    // Star on top (sphere with star texture)
    Decoration starDeco;
    starDeco.mesh = generateEllipsoid(0.6f, 3.0f, 0.6f, 24, 24, starTex);
    // Total tree height approx: trunk base at 0, branch3 starts at 5+3+2.5=10.5, height 4 -> tip at 14.5
    starDeco.relativePos = vec3(0.0f, 14.0f, 0.0f);
    treeDecorations.push_back(starDeco);

    // 5 Balls scattered on branches
    // Approx branch levels relative to base: ~3-5 (bottom), ~7-9 (middle), ~10-12 (top)
    std::vector<vec3> ballPositions = {
        vec3(3.5f, 5.0f, 4.6f),   // Bottom branch area white_with_yellow
        vec3(-2.5f, 5.5f, 4.9f),  // Bottom/Middle area pink
        vec3(-1.8f, 8.0f, 4.4f),  // Middle branch area red
        vec3(1.5f, 9.0f, 3.5f),   // Middle/Top area red_with_star
        vec3(-0.8f, 11.5f, 2.8f) // Top branch area white_ball
    };
    for (int i = 0; i < 5; ++i) {
        Decoration ballDeco;
        // Small sphere for ball, cycling through textures
        ballDeco.mesh = generateEllipsoid(0.4f, 0.4f, 0.4f, 24, 24, ballTexs[i % ballTexs.size()]);
        ballDeco.relativePos = ballPositions[i];
        treeDecorations.push_back(ballDeco);
    }


    // --- Setup Scene ---
    vec3 airshipPos(0.0f, 30.0f, 0.0f);
    vec3 treePos(20.0f, 0.0f, 20.0f);
    float terrainScale = 2.0f; float terrainHeightScale = 10.0f;
    treePos.y = getTerrainHeight(treePos.x, treePos.z, heightMapImage, terrainScale, terrainHeightScale);

    std::vector<Target> targets;
    for (int i = 0; i < 5; ++i) {
        Target t;
        float tx = i * 15.0f - 30.0f; float tz = i * 10.0f - 20.0f;
        float ty = getTerrainHeight(tx, tz, heightMapImage, terrainScale, terrainHeightScale);
        t.position = vec3(tx, ty + 2.0f, tz); t.body = houseBody; t.roof = houseRoof; targets.push_back(t);
    }

    std::vector<Parcel> parcels;
    bool aimMode = false;
    vec3 cameraPos; vec3 cameraFront; vec3 cameraUp;
    vec3 lightDir = normalize(vec3(-0.5f, -1.0f, -0.5f));
    int score = 0; sf::Clock clock;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::C) aimMode = !aimMode;
                if (event.key.code == sf::Keyboard::P) {
                    Parcel p; p.position = airshipPos + vec3(0, -4.0f, 0); p.mesh = parcelMesh; parcels.push_back(p);
                }
            }
        }
        float dt = clock.restart().asSeconds();

        // --- Controls ---
        float speed = 15.0f;
        vec3 forward = vec3(0, 0, -1); vec3 right = normalize(cross(forward, vec3(0, 1, 0)));
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) airshipPos += forward * speed * dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) airshipPos -= forward * speed * dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) airshipPos += right * speed * dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) airshipPos -= right * speed * dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Space)) airshipPos.y += speed * dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::LControl)) airshipPos.y -= speed * dt;

        // --- Updates ---
        for (auto& p : parcels) {
            if (!p.active) continue;
            p.position += p.velocity * dt;
            float terrainH = getTerrainHeight(p.position.x, p.position.z, heightMapImage, terrainScale, terrainHeightScale);
            if (p.position.y <= terrainH) { p.active = false; continue; }
            for (auto& t : targets) {
                if (!t.active) continue;
                if (distance(p.position, t.position) < p.radius + t.radius) {
                    t.active = false; p.active = false; score++; std::cout << "HIT! Score: " << score << std::endl; break;
                }
            }
        }

        // --- Camera ---
        if (aimMode) {
            cameraPos = airshipPos + vec3(0, -6.0f, 0); cameraFront = vec3(0.0f, -1.0f, 0.0f); cameraUp = vec3(0.0f, 0.0f, -1.0f);
        }
        else {
            cameraPos = airshipPos + vec3(0, 10.0f, 20.0f); cameraFront = normalize(airshipPos - cameraPos); cameraUp = vec3(0.0f, 1.0f, 0.0f);
        }
        mat4 view = lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        mat4 projection = perspective(radians(60.0f), 800.0f / 600.0f, 0.1f, 1000.0f);
        mat4 model;

        glClearColor(0.5f, 0.7f, 1.0f, 1.0f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shader.use(); shader.setMat4("view", view); shader.setMat4("projection", projection); shader.setVec3("lightDir", lightDir); shader.setVec3("viewPos", cameraPos);

        // --- Drawing ---
        // Terrain
        model = mat4(1.0f); model = scale(model, vec3(terrainScale, 1.0f, terrainScale));
        shader.setMat4("model", model); shader.setInt("isTerrain", 1);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, heightMapTex); shader.setInt("heightMap", 2);
        terrain.draw(shader); shader.setInt("isTerrain", 0);

        // Tree Base
        model = translate(mat4(1.0f), treePos); shader.setMat4("model", model); trunk.draw(shader);
        mat4 branchModel = translate(model, vec3(0, 5.0f, 0)); shader.setMat4("model", branchModel); branch1.draw(shader);
        branchModel = translate(branchModel, vec3(0, 3.0f, 0)); shader.setMat4("model", branchModel); branch2.draw(shader);
        branchModel = translate(branchModel, vec3(0, 2.5f, 0)); shader.setMat4("model", branchModel); branch3.draw(shader);

        // NEW: Draw Decorations
        for (const auto& deco : treeDecorations) {
            // Position relative to tree base
            model = translate(mat4(1.0f), treePos + deco.relativePos);
            shader.setMat4("model", model);
            deco.mesh.draw(shader);
        }

        // Airship
        model = translate(mat4(1.0f), airshipPos); mat4 balloonModel = rotate(model, radians(90.0f), vec3(0, 1, 0));
        shader.setMat4("model", balloonModel); balloon.draw(shader);
        mat4 gondolaModel = translate(model, vec3(0, -3.0f, 0)); shader.setMat4("model", gondolaModel); gondola.draw(shader);

        // Targets
        for (const auto& t : targets) {
            if (!t.active) continue;
            model = translate(mat4(1.0f), t.position); shader.setMat4("model", model); t.body.draw(shader);
            mat4 roofModel = translate(model, vec3(0, 2.0f, 0)); roofModel = rotate(roofModel, radians(45.0f), vec3(0, 1, 0));
            shader.setMat4("model", roofModel); t.roof.draw(shader);
        }
        // Parcels
        for (const auto& p : parcels) {
            if (!p.active) continue;
            model = translate(mat4(1.0f), p.position); shader.setMat4("model", model); p.mesh.draw(shader);
        }

        window.display();
    }
    return 0;
}