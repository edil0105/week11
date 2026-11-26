// =====================================================================
//  КОМПЬЮТЕРЛІК ГРАФИКА — бір файлдық жоба
//
//  1-АПТА:    терезе, фон анимациясы, FPS
//  СЕМИНАР 4: uniform, dt, цикл "жаңарту (update)" және "сызу (draw)"
//  5-АПТА:    индекс буфері (EBO), glDrawElements
//  7-АПТА:    координаттар кеңістіктері, MVP
//  8-АПТА:    тереңдік буфері, back-face culling, бес куб
//  9-АПТА:    камера моделі, LookAt, пернетақтамен жылжу
//  10-АПТА:   толық еркін камера — тінтуір (yaw/pitch), дөңгелек (fov)
//  11-АПТА:   ТЕКСТУРАЛАР — суретті файлдан жүктеу (stb_image),
//             текстура объектісі, UV, орау (wrap), сүзгі (filter), mipmap
//             - базалық:     stb_image, ASSET_DIR, vertices[] = поз + UV,
//                            stride 5, sampler2D, loadTexture(), байлау
//             - 1-тапсырма:  1/2/3/4 — REPEAT / MIRRORED / EDGE / BORDER,
//                            U — vertices[] ішіндегі UV × 2.0
//             - 2-тапсырма:  N — MAG: NEAREST / LINEAR (U — UV × 0.1)
//             - 3-тапсырма:  B — MIN: LINEAR / LINEAR_MIPMAP_LINEAR,
//                            z = -40 маңындағы алты куб
//             - 4-тапсырма:  K — MAG-қа mipmap тұрақтысы (әдейі қате),
//                            X — mipmap-сыз текстура + mipmap сүзгісі
//             - 5-тапсырма:  [ / ] — uvScale 0.5 .. 8.0 (шейдерде)
//             - қосымша:     Қ1 O — өз суретім (4 арна), Қ2 T — tint,
//                            Қ3 F — ағып жатқан су, Қ4 G — анизотропия
//                            (H — үлкен жалпақ еден-куб)
//
//  Басқару:
//      Тінтуір     — жан-жаққа қарау       Дөңгелек — fov (zoom)
//      W/A/S/D     — жүру                  Space / Left Ctrl — жоғары / төмен
//      Left Shift  — жүгіру                I — invert Y
//      1 2 3 4     — орау режимі           U — vertices[] UV: 1.0 / 2.0 / 0.1
//      N           — MAG сүзгісі           B — mipmap (MIN сүзгісі)
//      K           — қара куб (MAG қате)   X — қара куб (mipmap-сыз текстура)
//      [ / ]       — uvScale кішірейту / үлкейту
//      O T F G H   — өз суретім / tint / ағын / анизотропия / еден
//      P           — камера мен текстураның күйі
//      R           — камераны бастапқы күйге қайтару
//      L           — pitch шектеуі         M — тінтуір режимі (FPS / сүйреу)
//      F12         — скриншот              ESC — шығу
//
//  Іске қосу:  ./build/bin/cg                — қалыпты режим
//              ./build/bin/cg --screenshots  — README-дегі скриншоттар
// =====================================================================

#include <glad/gl.h>      // МІНДЕТТІ: glad әрқашан GLFW-дан БҰРЫН
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// 11-апта, 1-қадам: stb_image. #define-сыз stbi_load табылмайды
// ("undefined reference to stbi_load") — нақты код осы жолмен ғана қосылады.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#ifndef ASSET_DIR
#define ASSET_DIR "assets"
#endif
#ifndef SCREENSHOT_DIR
#define SCREENSHOT_DIR "screenshots"
#endif

// Анизотропты сүзгі (Қ4): OpenGL 4.6-да ядроға кірген, 3.3-те — кеңейтім.
// Тұрақтылардың мәні екеуінде бірдей.
#ifndef GL_TEXTURE_MAX_ANISOTROPY
#define GL_TEXTURE_MAX_ANISOTROPY     0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY
#define GL_MAX_TEXTURE_MAX_ANISOTROPY 0x84FF
#endif

int gWidth = 800, gHeight = 600;

// =====================================================================
//  10-АПТА: КАМЕРА
// =====================================================================
struct Camera {
    glm::vec3 pos;
    glm::vec3 front;
    glm::vec3 up;
};

const Camera START_CAMERA = {
    glm::vec3(0.0f, 0.0f,  3.0f),
    glm::vec3(0.0f, 0.0f, -1.0f),
    glm::vec3(0.0f, 1.0f,  0.0f)
};

Camera freeCam = START_CAMERA;
glm::vec3& camPos   = freeCam.pos;
glm::vec3& camFront = freeCam.front;
glm::vec3& camUp    = freeCam.up;

const float START_YAW   = -90.0f;
const float START_PITCH =   0.0f;
const float START_FOV   =  45.0f;

float yaw   = START_YAW;
float pitch = START_PITCH;
float lastX = 400.0f;
float lastY = 300.0f;
bool  firstMouse = true;
float fov   = START_FOV;

const float FOV_MIN =  1.0f;
const float FOV_MAX = 90.0f;

bool pitchLimit = true;
bool invertY    = false;
// WSL-де (WSLg) курсорды ұстау дұрыс жұмыс істемейді — сүйреу режимінен бастаймыз
bool dragMode   = std::getenv("WSL_DISTRO_NAME") != nullptr;

float angle = 0.0f;   // кубтардың айналу бұрышы

// =====================================================================
//  11-АПТА: ТЕКСТУРАНЫҢ КҮЙІ
//  Барлық тапсырмалар бір бағдарламада — пернелермен ауыстырылады.
// =====================================================================
const GLenum WRAP_MODES[4] = {
    GL_REPEAT, GL_MIRRORED_REPEAT, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER
};
const char* WRAP_NAMES[4] = {
    "GL_REPEAT", "GL_MIRRORED_REPEAT", "GL_CLAMP_TO_EDGE", "GL_CLAMP_TO_BORDER"
};

// U пернесі: vertices[] ішіндегі UV қандай санға көбейтіледі
//  1.0 — базалық, 2.0 — 1-тапсырма, 0.1 — 2-тапсырма
const float VERTEX_UV[3] = { 1.0f, 2.0f, 0.1f };

struct TexState {
    int   wrap       = 0;      // WRAP_MODES индексі          (1-тапсырма)
    int   vertexUv   = 0;      // VERTEX_UV индексі           (1, 2-тапсырма)
    bool  magNearest = false;  // MAG: NEAREST / LINEAR       (2-тапсырма)
    bool  mipmap     = true;   // MIN: LINEAR_MIPMAP_LINEAR   (3-тапсырма)
    bool  badMag     = false;  // MAG-қа mipmap тұрақтысы     (4-тапсырма)
    bool  noMipTex   = false;  // glGenerateMipmap-сыз текстура (4-тапсырма)
    float uvScale    = 1.0f;   // шейдердегі UV масштабы      (5-тапсырма)
    bool  ownImage   = false;  // Қ1
    bool  tint       = false;  // Қ2
    bool  flow       = false;  // Қ3
    bool  aniso      = false;  // Қ4
    bool  floor      = false;  // Қ4 үшін еден
};

TexState tex;
bool texParamsDirty = true;   // параметрлерді қайта орнату керек пе
bool vertexUvDirty  = true;   // VBO-дағы UV-ді қайта жүктеу керек пе

float flowTime = 0.0f;        // Қ3: ағын тек F қосулы кезде жылжиды

bool  anisoSupported = false;
float maxAniso       = 1.0f;

// =====================================================================
//  Callback-тер (10-апта)
// =====================================================================
void updateCameraFront() {
    glm::vec3 dir;
    dir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    dir.y = sin(glm::radians(pitch));
    dir.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    camFront = glm::normalize(dir);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (dragMode && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) != GLFW_PRESS) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
        return;
    }
    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;
    if (invertY) yoffset = -yoffset;
    lastX = (float)xpos;
    lastY = (float)ypos;

    const float sensitivity = 0.1f;
    yaw   += xoffset * sensitivity;
    pitch += yoffset * sensitivity;

    if (yaw >  360.0f) yaw -= 360.0f;
    if (yaw < -360.0f) yaw += 360.0f;

    if (pitchLimit) {
        if (pitch >  89.0f) pitch =  89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
    }
    updateCameraFront();
}

void scroll_callback(GLFWwindow*, double, double yoffset) {
    fov -= (float)yoffset;
    if (fov < FOV_MIN) fov = FOV_MIN;
    if (fov > FOV_MAX) fov = FOV_MAX;
}

void framebuffer_size_callback(GLFWwindow*, int width, int height) {
    if (width == 0 || height == 0) return;
    gWidth  = width;
    gHeight = height;
    glViewport(0, 0, width, height);
}

void saveScreenshot(const std::string& path) {
    std::vector<unsigned char> pixels((size_t)gWidth * gHeight * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, gWidth, gHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    stbi_flip_vertically_on_write(1);
    if (stbi_write_png(path.c_str(), gWidth, gHeight, 3, pixels.data(), gWidth * 3))
        std::cout << "Скриншот: " << path << "\n";
    else
        std::cerr << "Скриншот сақталмады: " << path << "\n";
}

// Бір рет басу: перне басылған СӘТТЕ ғана true
bool keyPressedOnce(GLFWwindow* window, int key) {
    static bool last[GLFW_KEY_LAST + 1] = {};
    bool now   = glfwGetKey(window, key) == GLFW_PRESS;
    bool fired = now && !last[key];
    last[key]  = now;
    return fired;
}

void applyMouseMode(GLFWwindow* window) {
    glfwSetInputMode(window, GLFW_CURSOR, dragMode ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    firstMouse = true;
}

void resetCamera() {
    freeCam = START_CAMERA;
    yaw   = START_YAW;
    pitch = START_PITCH;
    fov   = START_FOV;
    updateCameraFront();
}

void printState() {
    std::printf("camPos = (%.2f, %.2f, %.2f)  yaw = %.1f  pitch = %.1f  fov = %.1f\n",
                camPos.x, camPos.y, camPos.z, yaw, pitch, fov);
    std::printf("wrap = %s  MAG = %s  MIN = %s  vertices UV × %.1f  uvScale = %.2f\n",
                WRAP_NAMES[tex.wrap],
                tex.badMag ? "GL_LINEAR_MIPMAP_LINEAR (қате!)"
                           : (tex.magNearest ? "GL_NEAREST" : "GL_LINEAR"),
                tex.mipmap ? "GL_LINEAR_MIPMAP_LINEAR" : "GL_LINEAR",
                VERTEX_UV[tex.vertexUv], tex.uvScale);
    std::printf("mipmap-сыз текстура: %s  өз суретім: %s  tint: %s  ағын: %s  "
                "анизотропия: %s  еден: %s\n",
                tex.noMipTex ? "иә" : "жоқ", tex.ownImage ? "иә" : "жоқ",
                tex.tint ? "иә" : "жоқ", tex.flow ? "иә" : "жоқ",
                tex.aniso ? "иә" : "жоқ", tex.floor ? "иә" : "жоқ");
}

// Перне басылғанда күйді ауыстырып, консольге жазып, параметрлерді жаңартамыз
void toggle(bool& flag, const char* name) {
    flag = !flag;
    texParamsDirty = true;
    std::cout << name << ": " << (flag ? "ҚОСУЛЫ" : "ӨШІРУЛІ") << "\n";
}

// =====================================================================
//  Пернетақта — әр кадрда
// =====================================================================
void processInput(GLFWwindow* window, float dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // --- Жылжу (9–10-апта) ---
    float camSpeed = 2.5f * dt;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) camSpeed *= 2.0f;

    glm::vec3 right = glm::normalize(glm::cross(camFront, camUp));
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camPos += camSpeed * camFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camPos -= camSpeed * camFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camPos -= camSpeed * right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camPos += camSpeed * right;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camPos += camSpeed * glm::vec3(0.0f, 1.0f, 0.0f);
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        camPos -= camSpeed * glm::vec3(0.0f, 1.0f, 0.0f);

    // --- 5-тапсырма: uvScale. Үздіксіз әрекет — dt-ге көбейтеміз ---
    if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) tex.uvScale += 2.0f * dt;
    if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET)  == GLFW_PRESS) tex.uvScale -= 2.0f * dt;
    if (tex.uvScale < 0.5f) tex.uvScale = 0.5f;
    if (tex.uvScale > 8.0f) tex.uvScale = 8.0f;

    // --- 1-тапсырма: орау режимдері ---
    const int wrapKeys[4] = { GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4 };
    for (int i = 0; i < 4; i++) {
        if (keyPressedOnce(window, wrapKeys[i])) {
            tex.wrap = i;
            texParamsDirty = true;
            std::cout << "wrap = " << WRAP_NAMES[i] << "\n";
        }
    }
    if (keyPressedOnce(window, GLFW_KEY_U)) {
        tex.vertexUv = (tex.vertexUv + 1) % 3;
        vertexUvDirty = true;
        std::cout << "vertices[] UV × " << VERTEX_UV[tex.vertexUv] << "\n";
    }

    // --- 2, 3, 4-тапсырмалар ---
    if (keyPressedOnce(window, GLFW_KEY_N)) toggle(tex.magNearest, "MAG = GL_NEAREST");
    if (keyPressedOnce(window, GLFW_KEY_B)) toggle(tex.mipmap,     "MIN = GL_LINEAR_MIPMAP_LINEAR");
    if (keyPressedOnce(window, GLFW_KEY_K)) toggle(tex.badMag,     "MAG-қа mipmap тұрақтысы (әдейі қате)");
    if (keyPressedOnce(window, GLFW_KEY_X)) toggle(tex.noMipTex,   "glGenerateMipmap-сыз текстура");

    // --- Қосымша ---
    if (keyPressedOnce(window, GLFW_KEY_O)) toggle(tex.ownImage, "Өз суретім (Қ1)");
    if (keyPressedOnce(window, GLFW_KEY_T)) toggle(tex.tint,     "Tint (Қ2)");
    if (keyPressedOnce(window, GLFW_KEY_F)) toggle(tex.flow,     "Ағын (Қ3)");
    if (keyPressedOnce(window, GLFW_KEY_G)) {
        if (anisoSupported) toggle(tex.aniso, "Анизотропия (Қ4)");
        else std::cout << "Анизотропты сүзгі бұл драйверде жоқ\n";
    }
    if (keyPressedOnce(window, GLFW_KEY_H)) toggle(tex.floor, "Еден (Қ4)");

    // --- 10-аптадан қалған пернелер ---
    if (keyPressedOnce(window, GLFW_KEY_I)) {
        invertY = !invertY;
        std::cout << "invertY = " << (invertY ? "true" : "false") << "\n";
    }
    if (keyPressedOnce(window, GLFW_KEY_P)) printState();
    if (keyPressedOnce(window, GLFW_KEY_R)) {
        resetCamera();
        std::cout << "Камера бастапқы күйде\n";
    }
    if (keyPressedOnce(window, GLFW_KEY_L)) {
        pitchLimit = !pitchLimit;
        if (pitchLimit) {
            if (pitch >  89.0f) pitch =  89.0f;
            if (pitch < -89.0f) pitch = -89.0f;
            updateCameraFront();
        }
        std::cout << "pitch шектеуі: " << (pitchLimit ? "ҚОСУЛЫ" : "ӨШІРУЛІ") << "\n";
    }
    if (keyPressedOnce(window, GLFW_KEY_M)) {
        dragMode = !dragMode;
        applyMouseMode(window);
        std::cout << "Тінтуір режимі: " << (dragMode ? "сүйреу (ЛКМ басулы)" : "FPS") << "\n";
    }
    if (keyPressedOnce(window, GLFW_KEY_F12)) {
        static int shot = 1;
        saveScreenshot(std::string(SCREENSHOT_DIR) + "/shot-" + std::to_string(shot++) + ".png");
    }
}

// =====================================================================
//  ШЕЙДЕРЛЕР
// =====================================================================

// 11-апта, 5-қадам: текстураланған кубтар
const char* texVertexSrc = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    vTexCoord = aTexCoord;
}
)glsl";

// texture2D(...) емес, texture(...) — 3.3 Core-да ескі атау жоқ
const char* texFragmentSrc = R"glsl(
#version 330 core
in  vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D ourTexture;
uniform float uvScale;   // 5-тапсырма
uniform vec3  tint;      // Қ2
uniform float time;      // Қ3

void main() {
    vec2 uv = vTexCoord * uvScale + vec2(0.0, time * 0.1);
    FragColor = texture(ourTexture, uv) * vec4(tint, 1.0);
}
)glsl";

// 10-аптадағы түсті шейдер — еден-торы мен прицел үшін (оларда UV жоқ)
const char* colorVertexSrc = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 vColor;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    vColor = aColor;
}
)glsl";

const char* colorFragmentSrc = R"glsl(
#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor, 1.0);
}
)glsl";

GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "Shader compile error:\n" << log << "\n";
    }
    return shader;
}

GLuint createShaderProgram(const char* vsSrc, const char* fsSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        std::cerr << "Program link error:\n" << log << "\n";
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// =====================================================================
//  11-апта, 6-қадам: текстураны жүктеу
//  generateMipmaps = false — тек 4-тапсырмадағы "қара куб" тәжірибесі үшін
// =====================================================================
unsigned int loadTexture(const char* path, bool generateMipmaps = true) {
    unsigned int id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Суреттің (0,0)-і — сол жақ ЖОҒАРҒЫ бұрыш, OpenGL-дікі — ТӨМЕНГІ.
    // Осы жолсыз сурет төңкеріліп шығады.
    stbi_set_flip_vertically_on_load(true);

    int w, h, n;
    unsigned char* data = stbi_load(path, &w, &h, &n, 0);
    if (data) {
        // Арна саны 4 болса RGBA, әйтпесе RGB (Қ1: өз суретің 4 арналы болуы мүмкін)
        GLenum fmt = (n == 4) ? GL_RGBA : GL_RGB;
        // Ені 4-ке еселі болмаса да жолдар дұрыс оқылсын
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        // glGenerateMipmap — glTexImage2D-ден КЕЙІН: бұрын шақырсаң, дерек әлі жоқ
        if (generateMipmaps)
            glGenerateMipmap(GL_TEXTURE_2D);
        std::cout << path << "  " << w << "x" << h << "  арна: " << n
                  << (generateMipmaps ? "" : "  (mipmap-сыз)") << std::endl;
    } else {
        std::cerr << "Сурет жүктелмеді: " << path << std::endl;
    }
    // Дерек GPU-ға көшті — CPU жадында енді керек емес
    stbi_image_free(data);
    return id;
}

// Пернелермен таңдалған параметрлерді бір текстураға қолдану
void applyTexParams(GLuint id) {
    glBindTexture(GL_TEXTURE_2D, id);

    // 1-тапсырма: орау
    GLenum wrap = WRAP_MODES[tex.wrap];
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    // GL_CLAMP_TO_BORDER үшін шекара түсі — қызыл
    float border[] = { 1.0f, 0.0f, 0.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

    // 3-тапсырма: MIN — mipmap бар / жоқ
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    tex.mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);

    // 2-тапсырма: MAG — NEAREST / LINEAR
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    tex.magNearest ? GL_NEAREST : GL_LINEAR);

    // 4-тапсырма: MAG-қа mipmap тұрақтысы — ӘДЕЙІ ҚАТЕ.
    // Үлкейту кезінде mipmap мағынасыз (ең үлкен деңгейден үлкен ештеңе жоқ),
    // сондықтан MAG үшін тек GL_NEAREST пен GL_LINEAR рұқсат етілген.
    if (tex.badMag) {
        while (glGetError() != GL_NO_ERROR) {}   // ескі қателерді тазалаймыз
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        GLenum err = glGetError();
        GLint  actual = 0;
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &actual);
        std::printf("  glGetError() = 0x%04X%s, MAG қазір = 0x%04X\n", err,
                    err == GL_INVALID_ENUM ? " (GL_INVALID_ENUM)" : "", actual);
    }

    // Қ4: анизотропты сүзгі (1.0 — өшірулі)
    if (anisoSupported)
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY,
                        tex.aniso ? maxAniso : 1.0f);
}

bool hasExtension(const char* name) {
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (GLint i = 0; i < count; i++) {
        const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
        if (ext && std::strcmp(ext, name) == 0) return true;
    }
    return false;
}

// =====================================================================
//  ГЕОМЕТРИЯ
// =====================================================================

// 11-апта, 3-қадам: түстің орнына UV — енді әр вершинада 5 сан (поз + UV).
// Әр жақ сыртынан қарағанда бірдей ретпен жазылған:
//   сол-асты, оң-асты, оң-үсті  |  сол-асты, оң-үсті, сол-үсті
// сондықтан алты жақтың да UV реті бірдей — (0,0)(1,0)(1,1) (0,0)(1,1)(0,1),
// ал орау реті сырттан қарағанда CCW (culling ешбір жақты жоғалтпайды).
// Ескерту: 10-аптадағы түсті кубта әр жақтың бұрыштары әртүрлі ретпен
// жазылған еді. Түс үшін бұл маңызды емес, ал UV үшін бір бұрышқа екі түрлі
// UV түсіп, сурет бұзылатын. Сондықтан бұрыштардың реті бір үлгіге келтірілді.
const float cubeVertices[] = {
    //  поз                   UV
    // алдыңғы жақ (z = +0.5)
    -0.5f, -0.5f,  0.5f,   0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,   1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,   1.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,   0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,   1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,   0.0f, 1.0f,
    // артқы жақ (z = -0.5)
     0.5f, -0.5f, -0.5f,   0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,   1.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,   1.0f, 1.0f,
     0.5f, -0.5f, -0.5f,   0.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,   1.0f, 1.0f,
     0.5f,  0.5f, -0.5f,   0.0f, 1.0f,
    // сол жақ (x = -0.5)
    -0.5f, -0.5f, -0.5f,   0.0f, 0.0f,
    -0.5f, -0.5f,  0.5f,   1.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,   1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,   0.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,   1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,   0.0f, 1.0f,
    // оң жақ (x = +0.5)
     0.5f, -0.5f,  0.5f,   0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,   1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,   1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,   0.0f, 0.0f,
     0.5f,  0.5f, -0.5f,   1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,   0.0f, 1.0f,
    // асты (y = -0.5)
    -0.5f, -0.5f, -0.5f,   0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,   1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,   1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,   0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,   1.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,   0.0f, 1.0f,
    // үсті (y = +0.5)
    -0.5f,  0.5f,  0.5f,   0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,   1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,   1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,   0.0f, 0.0f,
     0.5f,  0.5f, -0.5f,   1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,   0.0f, 1.0f,
};

// U пернесі: vertices[] ішіндегі UV-ді k-ға көбейтіп, VBO-ға қайта жүктейміз
// (1-тапсырмадағы "1.0f орнына 2.0f" — қайта құрастырмай)
void uploadCubeVertices(GLuint vbo, float k) {
    float data[sizeof(cubeVertices) / sizeof(float)];
    for (size_t i = 0; i < sizeof(data) / sizeof(float); i++)
        data[i] = (i % 5 >= 3) ? cubeVertices[i] * k : cubeVertices[i];
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(data), data);
}

const float FLOOR_Y   = -1.5f;
const int   GRID_HALF = 20;
const int   RING_N    = 12;
const float RING_R    = 10.0f;

std::vector<float> makeGrid() {
    std::vector<float> v;
    auto line = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
        v.insert(v.end(), {a.x, a.y, a.z, c.r, c.g, c.b, b.x, b.y, b.z, c.r, c.g, c.b});
    };
    float h = (float)GRID_HALF;
    for (int i = -GRID_HALF; i <= GRID_HALF; i++) {
        float t = (float)i;
        glm::vec3 c = (i % 5 == 0) ? glm::vec3(0.45f) : glm::vec3(0.25f);
        line({t, FLOOR_Y, -h}, {t, FLOOR_Y, h}, i == 0 ? glm::vec3(0.2f, 0.4f, 1.0f) : c);
        line({-h, FLOOR_Y, t}, {h, FLOOR_Y, t}, i == 0 ? glm::vec3(1.0f, 0.3f, 0.3f) : c);
    }
    return v;
}

const float crosshair[] = {
    -0.03f,  0.0f,  0.0f,   1.0f, 1.0f, 1.0f,
     0.03f,  0.0f,  0.0f,   1.0f, 1.0f, 1.0f,
     0.0f,  -0.03f, 0.0f,   1.0f, 1.0f, 1.0f,
     0.0f,   0.03f, 0.0f,   1.0f, 1.0f, 1.0f,
};

// Түсті сызықтар үшін VAO (xyz + rgb, stride 6)
GLuint makeColorVao(const float* data, size_t bytes, GLuint& vbo) {
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, bytes, data, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return vao;
}

// Сахна: 8-аптадағы бес куб + 3-тапсырма: z = -40 маңындағы алты куб
const glm::vec3 cubePositions[] = {
    glm::vec3( 0.0f,  0.0f,   0.0f),
    glm::vec3( 2.0f,  0.5f,  -2.0f),
    glm::vec3(-1.8f, -0.3f,  -1.0f),
    glm::vec3( 1.2f, -0.8f,  -3.5f),
    glm::vec3(-2.2f,  1.0f,  -4.0f),
    // 3-тапсырма: алыстағы кубтар
    glm::vec3(-6.0f,  0.0f, -40.0f),
    glm::vec3(-3.0f,  1.5f, -42.0f),
    glm::vec3( 0.0f, -0.5f, -38.0f),
    glm::vec3( 3.0f,  2.0f, -41.0f),
    glm::vec3( 6.0f,  0.5f, -39.0f),
    glm::vec3( 0.0f,  3.5f, -44.0f),
};
const int CUBE_COUNT = sizeof(cubePositions) / sizeof(cubePositions[0]);

// =====================================================================
//  --screenshots: README-дегі скриншоттар. Әр қадам — камераның күйі және
//  текстура күйі (пернелермен қол жеткізуге болатын күй).
// =====================================================================
struct ShotStep {
    const char* file;
    glm::vec3   pos;
    float       yaw, pitch, fov, angle;
    void      (*setup)(TexState&);
};

const ShotStep SHOTS[] = {
    {"01-basic.png",        {0.0f, 0.9f, 2.6f}, -90, -16, 45, 0.6f, [](TexState&) {}},
    {"02-repeat.png",       {0.0f, 0.0f, 2.0f}, -90,   0, 45, 0.0f, [](TexState& t) { t.vertexUv = 1; t.wrap = 0; }},
    {"03-mirrored.png",     {0.0f, 0.0f, 2.0f}, -90,   0, 45, 0.0f, [](TexState& t) { t.vertexUv = 1; t.wrap = 1; }},
    {"04-clamp-edge.png",   {0.0f, 0.0f, 2.0f}, -90,   0, 45, 0.0f, [](TexState& t) { t.vertexUv = 1; t.wrap = 2; }},
    {"05-clamp-border.png", {0.0f, 0.0f, 2.0f}, -90,   0, 45, 0.0f, [](TexState& t) { t.vertexUv = 1; t.wrap = 3; }},
    {"06-nearest.png",      {0.0f, 0.0f, 2.0f}, -90,   0, 20, 0.0f, [](TexState& t) { t.vertexUv = 2; t.magNearest = true; }},
    {"07-linear.png",       {0.0f, 0.0f, 2.0f}, -90,   0, 20, 0.0f, [](TexState& t) { t.vertexUv = 2; }},
    {"08-no-mipmap.png",    {0.0f, 0.5f,-8.0f}, -90,  -3, 45, 0.6f, [](TexState& t) { t.mipmap = false; t.floor = true; }},
    {"09-mipmap.png",       {0.0f, 0.5f,-8.0f}, -90,  -3, 45, 0.6f, [](TexState& t) { t.floor = true; }},
    // 4-тапсырма. Mesa (WSL) MAG-қа mipmap тұрақтысын GL_INVALID_ENUM-мен
    // қабылдамайды да, MAG = GL_LINEAR болып қалады — куб қара болмайды
    // (10b-bad-mag.png). Сондықтан қара куб "Қате шықса" кестесіндегі екінші
    // себеппен көрсетілген: mipmap сүзгісі бар, бірақ glGenerateMipmap жоқ.
    {"10-black-cube.png",   {0.0f, 0.9f, 2.6f}, -90, -16, 45, 0.6f, [](TexState& t) { t.noMipTex = true; }},
    {"10b-bad-mag.png",     {0.0f, 0.9f, 2.6f}, -90, -16, 45, 0.6f, [](TexState& t) { t.badMag = true; }},
    {"11-uvscale-4.png",    {0.0f, 0.0f, 2.0f}, -90,   0, 45, 0.0f, [](TexState& t) { t.uvScale = 4.0f; }},
    {"12-own-image.png",    {0.0f, 0.9f, 2.6f}, -90, -16, 45, 0.6f, [](TexState& t) { t.ownImage = true; }},
    {"13-tint.png",         {0.0f, 0.9f, 2.6f}, -90, -16, 45, 0.6f, [](TexState& t) { t.tint = true; }},
    {"14-aniso-off.png",    {0.0f,-1.1f, 3.0f}, -90,  -6, 45, 0.6f, [](TexState& t) { t.floor = true; }},
    {"15-aniso-on.png",     {0.0f,-1.1f, 3.0f}, -90,  -6, 45, 0.6f, [](TexState& t) { t.floor = true; t.aniso = true; }},
};
const int SHOT_COUNT = sizeof(SHOTS) / sizeof(SHOTS[0]);

// =====================================================================
//  MAIN
// =====================================================================
int main(int argc, char** argv) {
    bool screenshotMode = (argc > 1 && std::strcmp(argv[1], "--screenshots") == 0);

    if (!glfwInit()) {
        std::cerr << "GLFW іске қосылмады\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(gWidth, gHeight,
                                          "Компьютерлік графика — 11-апта",
                                          nullptr, nullptr);
    if (!window) {
        std::cerr << "Терезе жасалмады. Видеокарта OpenGL 3.3-ті қолдамауы мүмкін.\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSwapInterval(1);

    if (gladLoadGL(glfwGetProcAddress) == 0) {
        std::cerr << "GLAD жүктелмеді\n";
        glfwTerminate();
        return -1;
    }

    applyMouseMode(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

    glfwGetFramebufferSize(window, &gWidth, &gHeight);
    glViewport(0, 0, gWidth, gHeight);

    std::cout << "OpenGL: " << glGetString(GL_VERSION) << "\n";
    std::cout << "1-4 — орау, U — vertices UV, N — MAG, B — mipmap, K/X — қара куб,\n"
                 "[ ] — uvScale, O — өз суретім, T — tint, F — ағын, G — анизотропия,\n"
                 "H — еден, P — күй, R — камера, M — тінтуір режимі, F12 — скриншот\n";
    if (dragMode)
        std::cout << "WSL: сүйреу режимі — камераны бұру үшін ЛКМ басып тұрып тартыңыз (M — ауыстыру)\n";

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    GLuint texShader   = createShaderProgram(texVertexSrc, texFragmentSrc);
    GLuint colorShader = createShaderProgram(colorVertexSrc, colorFragmentSrc);

    GLint tModel = glGetUniformLocation(texShader, "model");
    GLint tView  = glGetUniformLocation(texShader, "view");
    GLint tProj  = glGetUniformLocation(texShader, "projection");
    GLint tUv    = glGetUniformLocation(texShader, "uvScale");
    GLint tTint  = glGetUniformLocation(texShader, "tint");
    GLint tTime  = glGetUniformLocation(texShader, "time");
    GLint cModel = glGetUniformLocation(colorShader, "model");
    GLint cView  = glGetUniformLocation(colorShader, "view");
    GLint cProj  = glGetUniformLocation(colorShader, "projection");

    // --- Куб: поз + UV ---
    GLuint cubeVao, cubeVbo;
    glGenVertexArrays(1, &cubeVao);
    glGenBuffers(1, &cubeVbo);
    glBindVertexArray(cubeVao);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_DYNAMIC_DRAW);

    // 11-апта, 4-қадам: stride ЕКІ жерде де 5, UV — 2 компонент
    // поз: location 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // UV: location 1
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    GLuint gridVbo, crossVbo;
    std::vector<float> grid = makeGrid();
    int    gridVerts = (int)(grid.size() / 6);
    GLuint gridVao   = makeColorVao(grid.data(), grid.size() * sizeof(float), gridVbo);
    GLuint crossVao  = makeColorVao(crosshair, sizeof(crosshair), crossVbo);

    // --- Текстуралар. Жол ASSET_DIR арқылы — build/ ішінен іске қоссаң да табылады ---
    std::string containerPath = std::string(ASSET_DIR) + "/container.png";
    std::string ownPath       = std::string(ASSET_DIR) + "/my-image.png";
    GLuint texContainer = loadTexture(containerPath.c_str());
    GLuint texNoMip     = loadTexture(containerPath.c_str(), false);   // 4-тапсырма
    GLuint texOwn       = loadTexture(ownPath.c_str());                 // Қ1

    anisoSupported = hasExtension("GL_EXT_texture_filter_anisotropic") ||
                     hasExtension("GL_ARB_texture_filter_anisotropic");
    if (anisoSupported) {
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
        std::cout << "Анизотропия: max = " << maxAniso << "\n";
    }

    // 7-қадам: sampler бір рет 0-бірлікке байланады
    glUseProgram(texShader);
    glUniform1i(glGetUniformLocation(texShader, "ourTexture"), 0);

    double lastFrame  = glfwGetTime();
    double lastTitle  = 0.0;
    int    frame      = 0;
    int    shotIndex  = 0;
    int    shotLoaded = -1;

    while (!glfwWindowShouldClose(window)) {
        double currentFrame = glfwGetTime();
        float  dt = (float)(currentFrame - lastFrame);
        lastFrame = currentFrame;

        // ------------------------- ЖАҢАРТУ ---------------------------
        if (screenshotMode) {
            const ShotStep& s = SHOTS[shotIndex];
            if (shotLoaded != shotIndex) {   // жаңа скриншот — күйді бір рет орнатамыз
                tex = TexState();
                s.setup(tex);
                texParamsDirty = vertexUvDirty = true;
                shotLoaded = shotIndex;
                flowTime = 0.0f;
            }
            angle = s.angle;
            camPos = s.pos; yaw = s.yaw; pitch = s.pitch; fov = s.fov;
            updateCameraFront();
        } else {
            angle += dt;
            processInput(window, dt);
            if (tex.flow) flowTime += dt;
        }

        if (vertexUvDirty) {
            uploadCubeVertices(cubeVbo, VERTEX_UV[tex.vertexUv]);
            vertexUvDirty = false;
        }
        if (texParamsDirty) {
            applyTexParams(texContainer);
            applyTexParams(texNoMip);
            applyTexParams(texOwn);
            texParamsDirty = false;
        }

        glm::mat4 view = glm::lookAt(camPos, camPos + camFront, camUp);
        glm::mat4 projection = glm::perspective(glm::radians(fov),
            (float)gWidth / (float)gHeight, 0.1f, 200.0f);

        // Тақырып секундына 4 рет қана жаңарады (WSLg-де әр кадрда жаңарту баяулатады)
        if (currentFrame - lastTitle > 0.25) {
            lastTitle = currentFrame;
            char buf[200];
            std::snprintf(buf, sizeof(buf), "%s | MAG %s | MIN %s | UV×%.1f | uvScale %.2f | fov %.0f",
                          WRAP_NAMES[tex.wrap],
                          tex.badMag ? "MIPMAP(қате)" : (tex.magNearest ? "NEAREST" : "LINEAR"),
                          tex.mipmap ? "MIPMAP" : "LINEAR",
                          VERTEX_UV[tex.vertexUv], tex.uvScale, fov);
            glfwSetWindowTitle(window, buf);
        }

        // --------------------------- СЫЗУ ----------------------------
        glClearColor(0.10f, 0.12f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 11-апта, 7-қадам: текстураны байлау
        glUseProgram(texShader);
        glUniformMatrix4fv(tProj, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(tView, 1, GL_FALSE, glm::value_ptr(view));
        glUniform1f(tUv, tex.uvScale);
        glUniform1f(tTime, flowTime);

        glm::vec3 tint(1.0f);
        if (tex.tint) {   // Қ2: түс уақыт бойынша айналады
            float t = (float)currentFrame;
            if (screenshotMode) t = 1.0f;
            tint = glm::vec3(0.6f + 0.4f * sin(t),
                             0.6f + 0.4f * sin(t + 2.094f),
                             0.6f + 0.4f * sin(t + 4.189f));
        }
        glUniform3fv(tTint, 1, glm::value_ptr(tint));

        GLuint activeTex = tex.noMipTex ? texNoMip : (tex.ownImage ? texOwn : texContainer);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, activeTex);
        glBindVertexArray(cubeVao);

        for (int i = 0; i < CUBE_COUNT; i++) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), cubePositions[i]);
            model = glm::rotate(model, angle * (0.5f + (i % 5) * 0.3f), glm::vec3(0.3f, 1.0f, 0.0f));
            glUniformMatrix4fv(tModel, 1, GL_FALSE, glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        for (int i = 0; i < RING_N; i++) {
            float a = glm::radians(360.0f / RING_N * i);
            glm::vec3 p(RING_R * cos(a), FLOOR_Y + 0.5f + (i % 3), RING_R * sin(a));
            glm::mat4 model = glm::translate(glm::mat4(1.0f), p);
            model = glm::rotate(model, angle * 0.4f, glm::vec3(0.0f, 1.0f, 0.0f));
            glUniformMatrix4fv(tModel, 1, GL_FALSE, glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // Қ4: үлкен жалпақ куб — еден. Үстіңгі беті торға тимейді (z-fighting болмасын).
        // Сурет созылып кетпесін деп UV-ді 40 есе көбейтеміз (GL_REPEAT керек).
        if (tex.floor) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, FLOOR_Y - 0.06f, -20.0f));
            model = glm::scale(model, glm::vec3(80.0f, 0.1f, 80.0f));
            glUniformMatrix4fv(tModel, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1f(tUv, tex.uvScale * 40.0f);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // Еден-торы мен прицел — түсті шейдермен
        glUseProgram(colorShader);
        glm::mat4 identity(1.0f);
        glUniformMatrix4fv(cProj, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(cView, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(cModel, 1, GL_FALSE, glm::value_ptr(identity));
        if (!tex.floor) {
            glBindVertexArray(gridVao);
            glDrawArrays(GL_LINES, 0, gridVerts);
        }

        float aspect = (float)gWidth / (float)gHeight;
        glm::mat4 crossModel = glm::scale(identity, glm::vec3(1.0f / aspect, 1.0f, 1.0f));
        glUniformMatrix4fv(cModel, 1, GL_FALSE, glm::value_ptr(crossModel));
        glUniformMatrix4fv(cView,  1, GL_FALSE, glm::value_ptr(identity));
        glUniformMatrix4fv(cProj,  1, GL_FALSE, glm::value_ptr(identity));
        glDisable(GL_DEPTH_TEST);
        glBindVertexArray(crossVao);
        glDrawArrays(GL_LINES, 0, 4);
        glEnable(GL_DEPTH_TEST);

        if (screenshotMode && ++frame % 10 == 0) {
            saveScreenshot(std::string(SCREENSHOT_DIR) + "/" + SHOTS[shotIndex].file);
            if (++shotIndex == SHOT_COUNT) glfwSetWindowShouldClose(window, true);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &cubeVao);
    glDeleteVertexArrays(1, &gridVao);
    glDeleteVertexArrays(1, &crossVao);
    glDeleteBuffers(1, &cubeVbo);
    glDeleteBuffers(1, &gridVbo);
    glDeleteBuffers(1, &crossVbo);
    GLuint texs[3] = { texContainer, texNoMip, texOwn };
    glDeleteTextures(3, texs);
    glDeleteProgram(texShader);
    glDeleteProgram(colorShader);

    glfwTerminate();
    return 0;
}
