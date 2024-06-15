#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "shader.h"
#include "camera.h"
#include "perlin_noise.h"
#include <iostream>
#include <vector>
#include <stb_image.h>

// settings
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;
const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
const float RENDER_DISTANCE = 16.0f; // Render distance in blocks
const int TERRAIN_SIZE = 100;
const int MAX_HEIGHT = 24;

// camera
Camera camera(glm::vec3(0.0f, 7.0f, 3.0f));

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// process all input
void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

// callback function for mouse movement
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    static bool firstMouse = true;
    static float lastX = SCR_WIDTH / 2.0f;
    static float lastY = SCR_HEIGHT / 2.0f;

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// callback function for mouse scroll
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.ProcessMouseScroll(yoffset);
}

unsigned int loadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else {
        std::cout << "Failed to load texture" << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}


void renderSun(Shader& shader, unsigned int VAO, glm::vec3 lightPos, glm::mat4 view, glm::mat4 projection) {
    shader.use();
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, lightPos);

    model = glm::scale(model, glm::vec3(4.0f, 4.0f, 4.0f));

    // Ensure the sun always faces the origin (0,0,0)
    glm::vec3 direction = glm::normalize(-lightPos);
    float angle = acos(glm::dot(direction, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 axis = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), direction);
    if (glm::length(axis) < 0.0001f) {
        axis = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    model = glm::rotate(model, angle, axis);

    shader.setMat4("model", model);
    shader.setMat4("projection", projection);
    shader.setMat4("view", view);

    shader.setInt("isSun", 1); // Set isSun to true for rendering the sun

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    shader.setInt("isSun", 0); // Reset isSun to false after rendering the sun
}

// Perlin noise parameters
const int OCTAVES = 4;
const float FREQUENCY = 0.02f;
const float PERSISTENCE = 0.5f;

float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

glm::vec3 lerp(const glm::vec3& a, const glm::vec3& b, float t) {
    return a + t * (b - a);
}

float smoothStep(float t) {
    return t * t * (3 - 2 * t);
}

float perlinNoise(float x, float y, PerlinNoise& perlin) {
    float total = 0.0f;
    float frequency = FREQUENCY;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < OCTAVES; i++) {
        total += perlin.noise(x * frequency, y * frequency) * amplitude;
        maxValue += amplitude;
        frequency *= 2.0f;
        amplitude *= PERSISTENCE;
    }

    return total / maxValue;
}

void smoothTerrain(std::vector<std::vector<int>>& terrainHeights) {
    std::vector<std::vector<int>> smoothedHeights(TERRAIN_SIZE, std::vector<int>(TERRAIN_SIZE));

    for (int i = 0; i < TERRAIN_SIZE; i++) {
        for (int j = 0; j < TERRAIN_SIZE; j++) {
            int sum = 0;
            int count = 0;

            for (int x = -1; x <= 1; x++) {
                for (int y = -1; y <= 1; y++) {
                    int neighborX = i + x;
                    int neighborY = j + y;

                    if (neighborX >= 0 && neighborX < TERRAIN_SIZE && neighborY >= 0 && neighborY < TERRAIN_SIZE) {
                        sum += terrainHeights[neighborX][neighborY];
                        count++;
                    }
                }
            }

            smoothedHeights[i][j] = sum / count;
        }
    }

    terrainHeights = smoothedHeights;
}

glm::vec3 calculateSkyColor(float sunY, float radius) {
    glm::vec3 nightColor(0.0f, 0.0f, 0.0f); // Dark
    glm::vec3 noonColor(0.5f, 0.6f, 0.7f); // Light blue

    float normalizedSunY = (sunY + radius) / (2.0f * radius);

    //std::cout << "sunY: " << sunY << ", normalizedSunY: " << normalizedSunY << std::endl;

    if (normalizedSunY < 0.45f) {
        //std::cout << "Night" << std::endl;
        return nightColor;
    }
    /*
    else if (normalizedSunY < 0.5f) {
        float t = glm::smoothstep(0.4f, 0.55f, normalizedSunY);
        //std::cout << "Night to Sunrise" << std::endl;
        return glm::mix(nightColor, sunriseColor, t);
    }
    else if (normalizedSunY < 0.55f) {
        //std::cout << "Sunrise/Sunset" << std::endl;
        return sunriseColor; // Same color for both sunrise and sunset
    }
    */
    else if (normalizedSunY < 0.65f) {
        float t = glm::smoothstep(0.45f, 0.65f, normalizedSunY);
        //std::cout << "Sunrise/Sunset to Noon" << std::endl;
        return glm::mix(nightColor, noonColor, t);
    }
    else {
        //std::cout << "Noon" << std::endl;
        return noonColor;
    }
}


int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Minecraft Terrain", nullptr, nullptr);
    if (window == nullptr) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // Note to marker: Change directory according to your file's local location
    Shader shader("C:/Users/USER/OneDrive - UNIVERSITAS INDONESIA/University of Queensland/Semester 1 2024/COSC3000/Assessment/Major Project/Project/MinecraftTerrain/shaders/vertex_shader.vs",
        "C:/Users/USER/OneDrive - UNIVERSITAS INDONESIA/University of Queensland/Semester 1 2024/COSC3000/Assessment/Major Project/Project/MinecraftTerrain/shaders/fragment_shader.fs");
    Shader simpleDepthShader("C:/Users/USER/OneDrive - UNIVERSITAS INDONESIA/University of Queensland/Semester 1 2024/COSC3000/Assessment/Major Project/Project/MinecraftTerrain/shaders/simple_depth_shader.vs",
        "C:/Users/USER/OneDrive - UNIVERSITAS INDONESIA/University of Queensland/Semester 1 2024/COSC3000/Assessment/Major Project/Project/MinecraftTerrain/shaders/simple_depth_shader.fs");


    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);

    unsigned int depthMap;
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    float vertices[] = {
        // positions          // normals           // texture coords

        // Front face
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,


        // Back face
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,

        // Left face
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,

        // Right face
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,

         // Bottom face
         -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
          0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
          0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
          0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
         -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
         -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,

         // Top face
         -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,
          0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
          0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
          0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
         -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
         -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f
    };

    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Texture coordinate attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // create VAO for sun
    unsigned int sunVBO, sunVAO;
    glGenVertexArrays(1, &sunVAO);
    glGenBuffers(1, &sunVBO);
    glBindVertexArray(sunVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sunVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    // Position attribute for sun
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal attribute for sun
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // Texture coordinate attribute for sun
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // Note to marker: Change directory according to your file's local location
    // load and create textures 
    unsigned int sandTexture = loadTexture("C:/Users/USER/OneDrive - UNIVERSITAS INDONESIA/University of Queensland/Semester 1 2024/COSC3000/Assessment/Major Project/Project/MinecraftTerrain/textures/sand.jpg");
    unsigned int topTexture = loadTexture("C:/Users/USER/OneDrive - UNIVERSITAS INDONESIA/University of Queensland/Semester 1 2024/COSC3000/Assessment/Major Project/Project/MinecraftTerrain/textures/grassTop.jpg");
    unsigned int sideTexture = loadTexture("C:/Users/USER/OneDrive - UNIVERSITAS INDONESIA/University of Queensland/Semester 1 2024/COSC3000/Assessment/Major Project/Project/MinecraftTerrain/textures/grassSide.jpg");
    unsigned int bottomTexture = loadTexture("C:/Users/USER/OneDrive - UNIVERSITAS INDONESIA/University of Queensland/Semester 1 2024/COSC3000/Assessment/Major Project/Project/MinecraftTerrain/textures/dirt.jpg");

    float cubeSpacing = 0.5f;
    float cubeScale = 0.5f;
    PerlinNoise perlin;
    std::vector<std::vector<int>> terrainHeights(TERRAIN_SIZE, std::vector<int>(TERRAIN_SIZE));

    for (int i = 0; i < TERRAIN_SIZE; i++) {
        for (int j = 0; j < TERRAIN_SIZE; j++) {
            float noiseValue = perlinNoise(i, j, perlin);
            terrainHeights[i][j] = static_cast<int>(noiseValue * MAX_HEIGHT);
        }
    }

    // Apply terrain smoothing
    smoothTerrain(terrainHeights);

    while (!glfwWindowShouldClose(window)) {
        // Per-frame time logic
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Input
        processInput(window);

        // Calculate light position for rotating around the scene from top to bottom
        float radius = 64.0f;
        float angle = glfwGetTime() * glm::radians(1.0f); // Rotate 1 degrees per second
        float lightX = radius * cos(angle);
        float lightY = radius * sin(angle); // No offset needed
        glm::vec3 lightPos = glm::vec3(lightX, lightY, 0.0f);

        // Calculate sky color based on the sun's height
        glm::vec3 skyColor = calculateSkyColor(lightY, radius);

        // Render
        glClearColor(skyColor.r, skyColor.g, skyColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render depth of scene to texture (from light's perspective)
        glm::mat4 lightProjection, lightView;
        glm::mat4 lightSpaceMatrix;
        float near_plane = 1.0f, far_plane = 128.0f;
        lightProjection = glm::ortho(-40.0f, 40.0f, -40.0f, 40.0f, near_plane, far_plane);
        lightView = glm::lookAt(lightPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        lightSpaceMatrix = lightProjection * lightView;

        simpleDepthShader.use();
        simpleDepthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        for (int i = 0; i < TERRAIN_SIZE; i++) {
            for (int j = 0; j < TERRAIN_SIZE; j++) {
                for (int k = 0; k < terrainHeights[i][j]; k++) {
                    glm::vec3 cubePos = glm::vec3(
                        (i - TERRAIN_SIZE / 2) * cubeSpacing,
                        k * cubeSpacing,
                        (j - TERRAIN_SIZE / 2) * cubeSpacing
                    );

                    // Calculate the distance between the cube and the camera
                    float distance = glm::length(camera.Position - cubePos);

                    // Check if the cube is within the render distance
                    if (distance <= RENDER_DISTANCE) {
                        glm::mat4 model = glm::mat4(1.0f);
                        model = glm::translate(model, cubePos);
                        model = glm::scale(model, glm::vec3(cubeScale, cubeScale, cubeScale));
                        simpleDepthShader.setMat4("model", model);
                        glBindVertexArray(VAO);
                        glDrawArrays(GL_TRIANGLES, 0, 36);
                    }
                }
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Render scene as normal using the generated depth/shadow map  
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);
        shader.setVec3("viewPos", camera.Position);
        shader.setVec3("lightPos", lightPos); // Use rotating light position
        shader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        shader.setFloat("renderDistance", RENDER_DISTANCE);
        shader.setVec3("sunPosition", lightPos);
        shader.setVec3("fogColor", skyColor);

        // Render the terrain
        for (int i = 0; i < TERRAIN_SIZE; i++) {
            for (int j = 0; j < TERRAIN_SIZE; j++) {
                for (int k = 0; k < terrainHeights[i][j]; k++) {
                    glm::vec3 cubePos = glm::vec3(
                        (i - TERRAIN_SIZE / 2) * cubeSpacing,
                        k * cubeSpacing,
                        (j - TERRAIN_SIZE / 2) * cubeSpacing
                    );

                    // Calculate the distance between the cube and the camera
                    float distance = glm::length(camera.Position - cubePos);
                    if (distance <= RENDER_DISTANCE) {
                        // Render the block

                        glm::mat4 model = glm::mat4(1.0f);
                        model = glm::translate(model, cubePos);
                        model = glm::scale(model, glm::vec3(cubeScale, cubeScale, cubeScale));
                        shader.setMat4("model", model);

                        if (k < 7) {
                            glActiveTexture(GL_TEXTURE0);
                            glBindTexture(GL_TEXTURE_2D, sandTexture);
                            shader.setInt("diffuseTexture", 0);
                            shader.setInt("topTexture", 0);
                            shader.setInt("sideTexture", 0);
                            shader.setInt("bottomTexture", 0);
                        }
                        else {
                            glActiveTexture(GL_TEXTURE2);
                            glBindTexture(GL_TEXTURE_2D, topTexture);
                            glActiveTexture(GL_TEXTURE3);
                            glBindTexture(GL_TEXTURE_2D, sideTexture);
                            glActiveTexture(GL_TEXTURE4);
                            glBindTexture(GL_TEXTURE_2D, bottomTexture);
                            shader.setInt("topTexture", 2);
                            shader.setInt("sideTexture", 3);
                            shader.setInt("bottomTexture", 4);
                        }

                        glBindVertexArray(VAO);
                        glDrawArrays(GL_TRIANGLES, 0, 36);
                    }
                }
            }
        }

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        shader.setInt("shadowMap", 1);

        // Render the sun at its current position
        renderSun(shader, sunVAO, lightPos, view, projection);

        // Swap buffers and poll IO events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Clean up
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &sunVAO);
    glDeleteBuffers(1, &sunVBO);

    // Terminate GLFW
    glfwTerminate();
    return 0;
}
