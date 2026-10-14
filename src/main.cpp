// CS 6983 Coding Assignment 2: load a robot-arm OBJ and shade it with Blinn-Phong.
#include "gl_loader.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

struct App {
    int width = 1280;
    int height = 800;
    float yaw = 0.75f;
    float pitch = 0.35f;
    float distance = 7.0f;
    bool dragging = false;
    double lastX = 0.0;
    double lastY = 0.0;
};

static App gApp;
static GLuint gProgram = 0;
static GLuint gRobotVao = 0;
static GLuint gGridVao = 0;
static GLsizei gRobotVertexCount = 0;
static GLsizei gGridVertexCount = 0;
static GLint gMvpLoc = -1;
static GLint gModelLoc = -1;
static GLint gEyeLoc = -1;
static GLint gLightLoc = -1;
static GLint gAmbientLoc = -1;
static GLint gShininessLoc = -1;
static GLint gSpecularLoc = -1;

static const glm::vec3 BACKGROUND(0.075f, 0.09f, 0.12f);
static const glm::vec3 GRID_COLOR(0.28f, 0.32f, 0.37f);

static std::string read_text(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Could not open " + path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

static GLuint compile_shader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        glDeleteShader(shader);
        throw std::runtime_error(std::string("Shader compile failed: ") + log);
    }
    return shader;
}

static GLuint link_program(const std::string& vertexPath, const std::string& fragmentPath) {
    const GLuint vertex = compile_shader(GL_VERTEX_SHADER, read_text(vertexPath));
    const GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, read_text(fragmentPath));
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048]{};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        throw std::runtime_error(std::string("Program link failed: ") + log);
    }
    return program;
}

// OBJ face indices may be positive (one based) or negative (relative to the end).
static int obj_index(const std::string& text, std::size_t count) {
    if (text.empty()) return -1;
    const int value = std::stoi(text);
    const int index = value > 0 ? value - 1 : static_cast<int>(count) + value;
    if (index < 0 || index >= static_cast<int>(count)) {
        throw std::runtime_error("OBJ contains an out-of-range index");
    }
    return index;
}

static void read_materials(const std::filesystem::path& objPath,
                           std::unordered_map<std::string, glm::vec3>& materials) {
    std::ifstream obj(objPath);
    std::string line;
    while (std::getline(obj, line)) {
        std::istringstream row(line);
        std::string command, name;
        if (!(row >> command >> name) || command != "mtllib") continue;
        std::ifstream mtl(objPath.parent_path() / name);
        if (!mtl) continue;
        std::string materialName;
        while (std::getline(mtl, line)) {
            std::istringstream materialRow(line);
            std::string token;
            if (!(materialRow >> token)) continue;
            if (token == "newmtl") materialRow >> materialName;
            else if (token == "Kd" && !materialName.empty()) {
                glm::vec3 color;
                if (materialRow >> color.r >> color.g >> color.b) materials[materialName] = color;
            }
        }
    }
}

// Loads positions, normals, faces, and diffuse colors from a Wavefront OBJ/MTL pair.
// The expanded triangle stream keeps the loader small and supports polygons and
// negative OBJ indices without requiring an external model-import library.
static GLuint load_obj(const std::filesystem::path& path, GLsizei& vertexCount) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Could not open OBJ model: " + path.string());
    std::unordered_map<std::string, glm::vec3> materials;
    read_materials(path, materials);
    std::vector<glm::vec3> positions, normals;
    std::vector<float> vertices;
    std::string activeMaterial;
    glm::vec3 defaultColor(0.75f, 0.43f, 0.22f);
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream row(line);
        std::string command;
        if (!(row >> command) || command[0] == '#') continue;
        if (command == "v") {
            glm::vec3 p;
            if (row >> p.x >> p.y >> p.z) positions.push_back(p);
        } else if (command == "vn") {
            glm::vec3 n;
            if (row >> n.x >> n.y >> n.z) normals.push_back(glm::normalize(n));
        } else if (command == "usemtl") {
            row >> activeMaterial;
            auto it = materials.find(activeMaterial);
            defaultColor = it == materials.end() ? glm::vec3(0.75f, 0.43f, 0.22f) : it->second;
        } else if (command == "f") {
            struct Corner { int position; int normal; };
            std::vector<Corner> face;
            std::string item;
            while (row >> item) {
                std::stringstream fields(item);
                std::string positionText, textureText, normalText;
                std::getline(fields, positionText, '/');
                std::getline(fields, textureText, '/');
                std::getline(fields, normalText, '/');
                face.push_back({obj_index(positionText, positions.size()), obj_index(normalText, normals.size())});
            }
            for (std::size_t i = 1; i + 1 < face.size(); ++i) {
                const Corner corners[3] = {face[0], face[i], face[i + 1]};
                const glm::vec3 a = positions.at(corners[0].position);
                const glm::vec3 b = positions.at(corners[1].position);
                const glm::vec3 c = positions.at(corners[2].position);
                const glm::vec3 faceNormal = glm::normalize(glm::cross(b - a, c - a));
                for (const Corner& corner : corners) {
                    const glm::vec3 p = positions.at(corner.position);
                    const glm::vec3 n = corner.normal >= 0 ? normals.at(corner.normal) : faceNormal;
                    vertices.insert(vertices.end(), {p.x, p.y, p.z, n.x, n.y, n.z,
                                                     defaultColor.r, defaultColor.g, defaultColor.b});
                }
            }
        }
    }
    if (vertices.empty()) throw std::runtime_error("OBJ has no drawable faces: " + path.string());
    vertexCount = static_cast<GLsizei>(vertices.size() / 9);
    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);
    const GLsizei stride = 9 * static_cast<GLsizei>(sizeof(float));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(3 * sizeof(float)));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(6 * sizeof(float)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    return vao;
}

static GLuint make_grid(GLsizei& vertexCount) {
    std::vector<float> positions;
    const auto appendVertex = [&positions](float x, float y, float z) {
        positions.insert(positions.end(), {x, y, z, 0.0f, 1.0f, 0.0f,
                                           GRID_COLOR.r, GRID_COLOR.g, GRID_COLOR.b});
    };
    for (int i = -5; i <= 5; ++i) {
        const float x = static_cast<float>(i);
        appendVertex(x, 0.0f, -5.0f); appendVertex(x, 0.0f, 5.0f);
        appendVertex(-5.0f, 0.0f, x); appendVertex(5.0f, 0.0f, x);
    }
    vertexCount = static_cast<GLsizei>(positions.size() / 9);
    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(positions.size() * sizeof(float)), positions.data(), GL_STATIC_DRAW);
    const GLsizei stride = 9 * static_cast<GLsizei>(sizeof(float));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(3 * sizeof(float)));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(6 * sizeof(float)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    return vao;
}

static glm::vec3 camera_position() {
    const float cp = std::cos(gApp.pitch);
    return glm::vec3(gApp.distance * cp * std::sin(gApp.yaw),
                     1.05f + gApp.distance * std::sin(gApp.pitch),
                     gApp.distance * cp * std::cos(gApp.yaw));
}

static glm::mat4 view_matrix() {
    return glm::lookAt(camera_position(), glm::vec3(0.0f, 1.05f, 0.0f), glm::vec3(0, 1, 0));
}

static glm::mat4 projection_matrix() {
    const float aspect = gApp.height > 0 ? static_cast<float>(gApp.width) / gApp.height : 1.0f;
    return glm::perspective(glm::radians(45.0f), aspect, 0.1f, 40.0f);
}

static void draw_scene() {
    const glm::mat4 vp = projection_matrix() * view_matrix();
    glUseProgram(gProgram);
    glUniform1f(gAmbientLoc, 0.22f);
    glUniform1f(gShininessLoc, 48.0f);
    glUniform1f(gSpecularLoc, 0.45f);

    const glm::mat4 identity(1.0f);
    glUniformMatrix4fv(gModelLoc, 1, GL_FALSE, glm::value_ptr(identity));
    glUniformMatrix4fv(gMvpLoc, 1, GL_FALSE, glm::value_ptr(vp));
    glUniform3f(gEyeLoc, camera_position().x, camera_position().y, camera_position().z);
    glUniform3f(gLightLoc, -3.0f, 6.0f, 4.0f);
    glBindVertexArray(gGridVao);
    glDrawArrays(GL_LINES, 0, gGridVertexCount);

    glBindVertexArray(gRobotVao);
    glDrawArrays(GL_TRIANGLES, 0, gRobotVertexCount);
}

static void framebuffer_size_callback(GLFWwindow*, int width, int height) {
    gApp.width = width;
    gApp.height = height;
}

static void key_callback(GLFWwindow* window, int key, int, int action, int) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GLFW_TRUE);
    else if (key == GLFW_KEY_LEFT) gApp.yaw -= 0.08f;
    else if (key == GLFW_KEY_RIGHT) gApp.yaw += 0.08f;
    else if (key == GLFW_KEY_UP) gApp.pitch += 0.06f;
    else if (key == GLFW_KEY_DOWN) gApp.pitch -= 0.06f;
    else if (key == GLFW_KEY_MINUS) gApp.distance += 0.3f;
    else if (key == GLFW_KEY_EQUAL) gApp.distance -= 0.3f;
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        gApp.dragging = action == GLFW_PRESS;
        glfwGetCursorPos(window, &gApp.lastX, &gApp.lastY);
    }
}

static void cursor_callback(GLFWwindow*, double x, double y) {
    if (gApp.dragging) {
        gApp.yaw += static_cast<float>(x - gApp.lastX) * 0.008f;
        gApp.pitch += static_cast<float>(y - gApp.lastY) * 0.008f;
    }
    gApp.lastX = x;
    gApp.lastY = y;
}

static void scroll_callback(GLFWwindow*, double, double yOffset) { gApp.distance -= static_cast<float>(yOffset) * 0.4f; }

static void clamp_camera() {
    gApp.pitch = std::clamp(gApp.pitch, -1.15f, 1.15f);
    gApp.distance = std::clamp(gApp.distance, 3.0f, 20.0f);
}

int main() {
    glfwSetErrorCallback([](int code, const char* description) { std::cerr << "GLFW " << code << ": " << description << '\n'; });
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(gApp.width, gApp.height, "CS 6983 Robot Arm", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwGetFramebufferSize(window, &gApp.width, &gApp.height);
    if (!load_gl()) { glfwTerminate(); return 1; }

    try {
        gProgram = link_program(std::string(SHADER_DIR) + "/blinn_phong_vert.glsl",
                                std::string(SHADER_DIR) + "/blinn_phong_frag.glsl");
        gMvpLoc = glGetUniformLocation(gProgram, "uMVP");
        gModelLoc = glGetUniformLocation(gProgram, "uModel");
        gEyeLoc = glGetUniformLocation(gProgram, "uEyePosition");
        gLightLoc = glGetUniformLocation(gProgram, "uLightPosition");
        gAmbientLoc = glGetUniformLocation(gProgram, "uAmbientStrength");
        gShininessLoc = glGetUniformLocation(gProgram, "uShininess");
        gSpecularLoc = glGetUniformLocation(gProgram, "uSpecularStrength");
        gRobotVao = load_obj(std::filesystem::path(ASSET_DIR) / "robot_arm.obj", gRobotVertexCount);
        gGridVao = make_grid(gGridVertexCount);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    glUseProgram(gProgram);
    glEnable(GL_DEPTH_TEST);
    glClearColor(BACKGROUND.r, BACKGROUND.g, BACKGROUND.b, 1.0f);
    std::cout << "Drag or use arrows to orbit, wheel or -/= to zoom, Esc quits\n";
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        clamp_camera();
        glViewport(0, 0, gApp.width, gApp.height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        draw_scene();
        glfwSwapBuffers(window);
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
