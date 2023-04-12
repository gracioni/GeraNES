#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

//#include <GL/glu.h>
#include <iostream>

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glext.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#define IMGUI_IMPL_OPENGL_LOADER_GLEW
#include <imgui_impl_opengl3.h>

#include <nfd.h>

#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "CppGL/CppGL.h"
#include "SDLOpenGLWindow.h"

#include "GeraNes/GeraNesEmu.h"
#include "GeraNes/Logger.h"

#include "GeraNesUI/AudioOutput.h"
#include "GeraNesUI/InputManager.h"


// C++11. 
std::string shaderProgramText=R"html(
#if __VERSION__ >= 130
#define COMPAT_VARYING out
#define COMPAT_ATTRIBUTE in
#define COMPAT_TEXTURE texture
#else
#define COMPAT_VARYING varying
#define COMPAT_ATTRIBUTE attribute
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif

#define M_PI 3.1415926535897932384626433832795


#ifdef VERTEX

COMPAT_ATTRIBUTE vec2 VertexCoord;
COMPAT_ATTRIBUTE vec2 TexCoord;

uniform mat4 MVPMatrix;

COMPAT_VARYING vec2 uv;

void main() {
    gl_Position = MVPMatrix * vec4(VertexCoord,0.0,1.0);
    uv = TexCoord;
}

#endif

#ifdef FRAGMENT

#ifdef GL_ES
    #ifdef GL_FRAGMENT_PRECISION_HIGH
    precision highp float;
    #else
    precision mediump float;
    #endif
#endif

uniform sampler2D Texture;
uniform int Scanlines;
uniform bool GrayScale;

COMPAT_VARYING vec2 uv;

vec4 toGray(vec4 color)
{
    float c = (color.r+color.g+color.b)/3.0;
    return vec4(c,c,c,color.a);
}

void main() {

    if(Scanlines != 0) {

        gl_FragColor = vec4(0.0,0.0,0.0,1.0);

        float freq = float(Scanlines);
        float phase = freq*0.25;
        const float lineIntensity = 0.33;
        const float bright = 1.0 + 0.707/2.0 * 0.25;

        float value = sin(2.0 * M_PI * freq * uv.y + phase);
        value = clamp(value, 0.0, 1.0);
        value = 1.0 - lineIntensity *value;

        gl_FragColor.xyz += value * bright * COMPAT_TEXTURE(Texture,uv).xyz;
        //gl_FragColor = vec4(1.0,1.0,1.0,1.0);
    }
    else gl_FragColor = COMPAT_TEXTURE(Texture,uv);

    if(GrayScale) gl_FragColor = toGray(gl_FragColor);

}

#endif
)html";

float GetTime()
{
    return (float)SDL_GetTicks64()/1000.0f; 
}


GLuint loadTextureFromFile(const char* path) {

    GLuint ret = 0;

    IMG_Init(IMG_INIT_PNG);

    SDL_Surface* surface = IMG_Load(path);

    glGenTextures(1, &ret);
    glBindTexture(GL_TEXTURE_2D, ret);    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    GLuint externalFormat, internalFormat;
    SDL_PixelFormat *format = surface->format;
    if (surface->format->BytesPerPixel == 4) {

        if (surface->format->Rmask == 0x000000ff)
            externalFormat = GL_RGBA;
        else
            externalFormat = GL_BGRA;
    }
    else {

        // no alpha
        if (surface->format->Rmask == 0x000000ff)
            externalFormat = GL_RGB;
        else
            externalFormat = GL_BGR;
    }
    internalFormat = (surface->format->BytesPerPixel == 4) ? GL_RGBA : GL_RGB;
	
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, surface->w, surface->h, 0, externalFormat, GL_UNSIGNED_BYTE, surface->pixels);

    glBindTexture(GL_TEXTURE_2D, 0);

    SDL_FreeSurface(surface);

    return ret;
}


class ControllerConfigWindow {

private:

    const static constexpr size_t N_BUTTONS = InputInfo::BUTTONS.size();
    const float CAPTURE_TIME = 3.0f;
    
    int selected[N_BUTTONS] = {0};    

    enum {NONE, WAIT_EMPTY, WAIT_BUTTON} m_captureState = NONE;

    bool m_show = false;
    bool m_lastShow = false;

    InputInfo* m_inputInfo = nullptr;

    int m_captureIndex = -1;
    float m_lastTime = 0.0f;
    float m_captureTime = 0.0f;

    void startCapture(int index) {
        m_captureIndex = index;
        m_captureState = WAIT_EMPTY;
        m_captureTime = CAPTURE_TIME;
        m_lastTime = GetTime();

        ImGuiIO& io = ImGui::GetIO();
        // Desativa o input de teclado
        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    }

    void stopCapture() {
        m_captureState = NONE;
        m_captureIndex = -1;

        ImGuiIO& io = ImGui::GetIO();
        // Desativa o input de teclado
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    }


public:

    SigSlot::Signal<> signalShow;
    SigSlot::Signal<> signalClose;
    

    void show(InputInfo& input) {

        if(m_inputInfo != nullptr) {
            stopCapture();
        }

        m_inputInfo = &input;
        m_show = true;
        m_captureIndex = -1;
    }

    void hide() {
        m_show = false;
        stopCapture();
        m_inputInfo = nullptr;
    }

    void update() {    

        if(m_show != m_lastShow) {
            if(m_show) signalShow();
            else signalClose();

            m_lastShow = m_show;
        }

        if(!m_show) return;        

        if(m_captureState != NONE) {

            float time = GetTime();
            float dt = time - m_lastTime;
            m_captureTime -= dt;
            m_lastTime = time;

            if(m_captureTime <= 0) {
                stopCapture();
            }

            InputManager::instance().updateInputs();
            auto capture = InputManager::instance().capture();

            switch(m_captureState) {

                case WAIT_EMPTY:
                    if(capture.size() == 0) m_captureState = WAIT_BUTTON;                    
                    break;

                case WAIT_BUTTON:
                    if(capture.size() > 0) {
                        m_inputInfo->setByButtonIndex(m_captureIndex, capture[0]);             
                        if(m_captureIndex+1 < N_BUTTONS) startCapture(m_captureIndex+1);
                        else stopCapture();                      
                    }
                    break;
            }
            
        }

        ImGui::SetNextWindowSize(ImVec2(340, 0));
    
        ImGui::Begin("Controller Config", &m_show, ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoResize);

        if(ImGui::BeginTable("Tabela", 2, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders)){

            ImGui::TableSetupColumn("Button");
            ImGui::TableSetupColumn("Input");
            ImGui::TableHeadersRow();

            // Adicionar linhas à tabela
            for (int i = 0; i < N_BUTTONS; i++) {

                ImGui::TableNextRow();

                auto style = ImGui::GetStyle();
                auto color = style.Colors[ImGuiCol_TabHovered];

                if(m_captureState != NONE && i == m_captureIndex) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::ColorConvertFloat4ToU32(color));
                     
                ImGui::TableNextColumn(); 

                // Coluna 1        
                if(m_captureState == NONE) ImGui::Selectable(InputInfo::BUTTONS[i], &selected[i], ImGuiSelectableFlags_SpanAllColumns);
                else ImGui::Text(InputInfo::BUTTONS[i]);

                if (m_captureState == NONE && ImGui::IsItemActive()) {
                    startCapture(i);                    
                }                    

                ImGui::TableNextColumn();   
            
                // Coluna 2
                ImGui::Text(m_inputInfo->getByButtonName(InputInfo::BUTTONS[i]).c_str());      
                
                

                

                // Resetar a cor de fundo da linha
                // if (hover) {
                //     ImGui::PopStyleColor();
                // }
            }


            // Finalizar a janela do ImGui
            ImGui::EndTable();
        }

        char aux[128];

        if (m_captureState != NONE) {            
            sprintf(aux, "Waiting input for button '%s'... (%0.1fs)", InputInfo::BUTTONS[m_captureIndex],std::max(0.0f, m_captureTime));
        }
        else sprintf(aux, "");
        ImGui::Text(aux);

        

        ImGui::End();

        ImGui::SetWindowFocus("Controller Config");

    }
    
};


class MyApp : public SDLOpenGLWindow, public SigSlot::SigSlotBase {

private:

    ControllerConfigWindow m_controllerConfigWindow;

    std::vector<std::string> m_audioDevices;
    int m_currentAudioDeviceIndex = -1;

    bool m_horizontalStretch = false;
    int m_clipHeightValue = 8;

    GLVertexArrayObject m_vao;
    GLVertexBufferObject m_vbo;

    GLShaderProgram m_shaderProgram;

    bool m_updateObjectsFlag = true;

    GLuint m_texture = 0;

    bool m_scanlinesFlag = false;

    glm::mat4x4 m_mvp = glm::mat4x4(1.0f);

    AudioOutput audioOutput;

    GeraNesEmu m_emu;

    InputInfo m_controller1;
    InputInfo m_controller2; 

    bool m_emuInputEnabled = true;

    enum VSyncType {OFF, SYNCRONIZED, ADAPTATIVE};
    VSyncType m_vsync = OFF;

    static constexpr std::array<const char*, 3> VSYNC_TYPE_LABELS {"Off", "Syncronized", "Adaptative"};

    void updateMVP() {
        glm::mat4 proj = glm::ortho(0.0f, (float)width(), (float)height(), 0.0f, -1.0f, 1.0f);           
        m_mvp = proj * glm::mat4(1.0f);
    }

public:

    MyApp() : m_emu(audioOutput) {

        m_controllerConfigWindow.signalShow.bind(MyApp::onCaptureBegin, this);
        m_controllerConfigWindow.signalClose.bind(MyApp::onCaptureEnd, this);

        m_audioDevices = audioOutput.getAudioList();

        for(auto d : m_audioDevices) {
            std::cout << d << std::endl;
        }

        audioOutput.config("");

        Logger::instance().signalLog.bind(&MyApp::onLog,this);
        m_emu.signalFrameStart.bind(&MyApp::onFrameStart, this);

        m_controller1.a = "S";
        m_controller1.b = "A";
        m_controller1.select = "Space";
        m_controller1.start = "Return";
        m_controller1.rewind = "Backspace";
        m_controller1.up = "Up";
        m_controller1.down = "Down";
        m_controller1.left = "Left";
        m_controller1.right = "Right";
    }

    void onCaptureBegin() {
        m_emuInputEnabled = false;
    }

    void onCaptureEnd() {
        m_emuInputEnabled = true;
    }

    void onLog(const std::string& msg) {
        std::cout << msg << std::endl;
    }

    virtual ~MyApp() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }

    void onFrameStart() {

        if(m_emuInputEnabled) {

            InputManager& im = InputManager::instance();        

            im.updateInputs();

            m_emu.setController1Buttons(
            im.get(m_controller1.a), im.get(m_controller1.b),
            im.get(m_controller1.select),im.get(m_controller1.start),
            im.get(m_controller1.up),im.get(m_controller1.down),
            im.get(m_controller1.left),im.get(m_controller1.right)
            );
        }      

    }

    virtual void initGL() override {

        if (SDL_Init(SDL_INIT_TIMER) < 0) {
            // error handling
        }

        GLenum err = glewInit();
        if (GLEW_OK != err)
        {
            /* Problem: glewInit failed, something is seriously wrong. */
            printf("Error: %s\n", glewGetErrorString(err));
        }

        //vsync 0(disabled) 1(enabled) -1(adaptative)
        SDL_GL_SetSwapInterval(0);


        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        const char* glsl_version = "#version 330";

        // Setup Platform/Renderer bindings
        ImGui_ImplSDL2_InitForOpenGL(sdlWindow(), glContext());
        ImGui_ImplOpenGL3_Init(glsl_version);


        glClearColor(0.0, 0.0, 0.0, 0.0);

        shaderInit();

        //glEnable(GL_CULL_FACE);
        //glCullFace(GL_BACK);
        glEnable(GL_TEXTURE_2D) ;
        glDisable(GL_DEPTH_TEST);

        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);

        //setFilter(m_currentBasicFilter,m_currentSpecialFilter);

        updateBuffers(); //create vbo

        if(!m_vao.isCreated()) {
            m_vao.create();
        }

        m_vao.bind();

        m_vbo.bind();

        GLsizei stride = 4*sizeof(GLfloat);

        glEnableVertexAttribArray(0); //position
        glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,stride,0);

        glEnableVertexAttribArray(1); //texture uv
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,stride, (void*)(2*sizeof(GLfloat)));

        m_vbo.release();

        m_vao.release();

        //m_texture = loadTextureFromFile("teste.png");

        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        updateMVP();
    }

    void shaderInit()
    {   
        std::string shaderText = shaderProgramText;
        std::string vertexText = "#define VERTEX\n" + shaderText;
        std::string fragmentText = "#define FRAGMENT\n" + shaderText;

        m_shaderProgram.create();

        if(!m_shaderProgram.addShaderFromSourceCode(GLShaderProgram::Vertex, vertexText.c_str())){
            //signalError(m_shaderProgram.log().toStdString());
            std::cout << "erro vertex\n";
            return;
        }

        if(!m_shaderProgram.addShaderFromSourceCode(GLShaderProgram::Fragment, fragmentText.c_str())){
            //signalError(m_shaderProgram->log().toStdString());
            std::cout << "erro fragment\n";       
            return;
        }

        m_shaderProgram.bindAttributeLocation("VertexCoord", 0);
        m_shaderProgram.bindAttributeLocation("TexCoord", 1);

        if(!m_shaderProgram.link()){
            //signalError(m_shaderProgram->log().toStdString());
            std::cout << "erro link\n";      
            return;
        }
    }

    void updateBuffers()
    {
        if(!m_vbo.isCreated()){
            m_vbo.create();
        }

        std::vector<GLfloat> data;

        if( width()/256.0 >= height()/(240.0-2*m_clipHeightValue))
        {
            GLfloat screenWidth = m_horizontalStretch ? width() : 256.0/(240.0-2*m_clipHeightValue) * height();
            GLfloat offsetX = (width() - screenWidth)/2.0;

            //glVertex2f(offsetX,0);]
            data.push_back(offsetX);
            data.push_back(0);
            //glTexCoord2f(0.0,m_clipHeightValue/256.0 ); //top left
            data.push_back(0.0);
            data.push_back(m_clipHeightValue/256.0);

            //glVertex2f(offsetX,height());
            data.push_back(offsetX);
            data.push_back(height());
            //glTexCoord2f(0.0,240.0/256.0 - m_clipHeightValue/256.0 ); //bottom left
            data.push_back(0.0);
            data.push_back(240.0/256.0 - m_clipHeightValue/256.0);

            //glVertex2f(offsetX+screenWidth,0);
            data.push_back(offsetX+screenWidth);
            data.push_back(0);
            //glTexCoord2f(1.0, m_clipHeightValue/256.0 ); //top right
            data.push_back(1.0);
            data.push_back(m_clipHeightValue/256.0);

            //glVertex2f(offsetX+screenWidth,height());
            data.push_back(offsetX+screenWidth);
            data.push_back(height());
            //glTexCoord2f(1.0,240.0/256.0 - m_clipHeightValue/256.0 ); //bottom right
            data.push_back(1.0);
            data.push_back(240.0/256.0 - m_clipHeightValue/256.0);
        }
        else
        {
            GLfloat screenHeight = (240.0-2*m_clipHeightValue)/256.0 * width();
            GLfloat offsetY = (height() - screenHeight)/2.0;

            //glVertex2f(0,offsetY);
            data.push_back(0);
            data.push_back(offsetY);
            //glTexCoord2f(0.0, m_clipHeightValue/256.0 ); //top left
            data.push_back(0.0);
            data.push_back(m_clipHeightValue/256.0);

            //glVertex2f(0,offsetY+screenHeight);
            data.push_back(0);
            data.push_back(offsetY+screenHeight);
            //glTexCoord2f(0.0,240.0/256.0 - m_clipHeightValue/256.0 ); //bottom left
            data.push_back(0.0);
            data.push_back(240.0/256.0 - m_clipHeightValue/256.0);

            //glVertex2f(width(),offsetY);
            data.push_back(width());
            data.push_back(offsetY);
            //glTexCoord2f(1.0,0.0 + m_clipHeightValue/256.0 ); //top right
            data.push_back(1.0);
            data.push_back(0.0 + m_clipHeightValue/256.0);

            //glVertex2f(width(),offsetY+screenHeight);
            data.push_back(width());
            data.push_back(offsetY+screenHeight);
            //glTexCoord2f(1.0,240.0/256.0 - m_clipHeightValue/256.0 ); //bottom right
            data.push_back(1.0);
            data.push_back(240.0/256.0 - m_clipHeightValue/256.0);
        }  

        m_vbo.bind();
        if( (size_t)m_vbo.size() != data.size()*sizeof(GLfloat))
            m_vbo.allocate(&data[0], data.size()*sizeof(GLfloat));
        else m_vbo.write(0,&data[0], data.size()*sizeof(GLfloat));
        m_vbo.release();

    }

    virtual bool onEvent(SDL_Event& event) override {

        ImGui_ImplSDL2_ProcessEvent(&event);

        switch(event.type) {

            // case SDL_KEYDOWN:
            //     if(event.key.keysym.sym == SDLK_ESCAPE) {             
            //         std::cout << "quit" << std::endl;
            //     }
            //     break;

            case SDL_WINDOWEVENT:

                switch(event.window.event) {

                    //case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        updateMVP();                   
                        m_updateObjectsFlag = true;
                        break;
                }
                break;

            // case SDL_KEYDOWN:
            //         switch (event.key.keysym.sym)
            //         {
            //             case SDLK_ESCAPE:
            //                 quit();
            //                 break;
            //         }
            //         break;                                

            // case SDL_MOUSEBUTTONDOWN: {

            //     if (ImGui::GetIO().WantCaptureMouse) break;

            //     int mouseX = event.button.x;
            //     int mouseY = event.button.y;
            //     // Lidar com o evento do botão do mouse pressionado nas coordenadas (mouseX, mouseY)
            //     std::cout << mouseX << "," << mouseY << std::endl;
            //     break;
            // }
                
        }

        return SDLOpenGLWindow::onEvent(event);
    }

    float m_lastTime = 0;
    Uint64 m_fpsTimer = 0;
    int m_fps = 0;
    int m_frameCounter = 0;

    void mainLoop()
    {
        float tempTime = GetTime();

        float dt = tempTime - m_lastTime;

        m_lastTime = tempTime;
 
        m_fpsTimer += dt;

        if(m_fpsTimer >= 1000)
        {
            m_fps = m_frameCounter;
            m_frameCounter = 0;
            m_fpsTimer -= 1000;
        }    

        if( m_emu.update(dt) ) render();
    }

    void render()
    {
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, m_clipHeightValue, 256, 240-2*m_clipHeightValue, GL_RGBA, GL_UNSIGNED_BYTE, m_emu.getFramebuffer()+m_clipHeightValue*256);
    }  

    void DrawNESButton(const char* label, bool pressed)
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec2 size(48, 48);
    ImVec2 padding(6, 6);
    ImVec4 color = ImVec4(0.62f, 0.62f, 0.62f, 1.0f);

    if (pressed)
    {
        color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    }

    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, padding);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::Button(label, size);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
}

void NESControllerDraw() {

    const ImVec2 GRID_SIZE = ImVec2(11,5);
    const ImVec2 GRID_PADDING = ImVec2(10,10);

    

     // Draw NES buttons
    ImGui::Begin("NES Controller");

        ImDrawList* drawList = ImGui::GetWindowDrawList(); 

        ImVec2 vMin = ImGui::GetWindowContentRegionMin();
        ImVec2 vMax = ImGui::GetWindowContentRegionMax();

        vMin += ImGui::GetWindowPos();
        vMax += ImGui::GetWindowPos();
        
        ImVec2 unitSize =  ImVec2((vMax-vMin)/GRID_SIZE);

        ImVec2 cursorOffset = ImGui::GetCursorScreenPos() - ImGui::GetWindowPos();

        
        /*
        ImGui::Columns(3);
        ImGui::SetColumnWidth(0, 64.0f);
        ImGui::SetColumnWidth(1, 64.0f);
        ImGui::SetColumnWidth(2, 64.0f);
        */

        // Draw Up button
        //ImGui::Dummy(ImVec2(0, 16));
        //ImGui::SameLine();
        ImGui::SetCursorPos(cursorOffset + unitSize * ImVec2(2, 1) + GRID_PADDING/2);
        drawList->AddText(ImGui::GetCursorScreenPos()+ImVec2( ImVec2( (unitSize.x-ImGui::CalcTextSize("Up").x)/2,0).x,-unitSize.y/2), ImGui::GetColorU32(ImGuiCol_Text), "Up");
        if (ImGui::Button("Up", unitSize - GRID_PADDING))
        {
            // Handle Up button pressed
        }
        

        // Draw Left button
        ImGui::SetCursorPos(cursorOffset + unitSize * ImVec2(1, 2) + GRID_PADDING/2);
        if (ImGui::Button("Left", unitSize - GRID_PADDING))
        {
            // Handle Left button pressed
        }

        // Draw Right button
        ImGui::SetCursorPos(cursorOffset + unitSize * ImVec2(3, 2) + GRID_PADDING/2);
        if (ImGui::Button("Right", unitSize - GRID_PADDING))
        {
            // Handle Right button pressed
        }

        // Draw Down button
        ImGui::SetCursorPos(cursorOffset + unitSize * ImVec2(2, 3) + GRID_PADDING/2);
        if (ImGui::Button("Down", unitSize - GRID_PADDING))
        {
            // Handle Down button pressed
        }

        ImGui::SetCursorPos(cursorOffset + unitSize * ImVec2(5, 3) + GRID_PADDING/2);
        if (ImGui::Button("Select", unitSize - GRID_PADDING))
        {
            // Handle Down button pressed
        }

        ImGui::SetCursorPos(cursorOffset + unitSize * ImVec2(6, 3) + GRID_PADDING/2);
        if (ImGui::Button("Start", unitSize - GRID_PADDING))
        {
            // Handle Down button pressed
        }

        ImGui::SetCursorPos(cursorOffset + unitSize * ImVec2(8, 3) + GRID_PADDING/2);
        if (ImGui::Button("B", unitSize - GRID_PADDING))
        {
            // Handle Down button pressed
        }

        ImGui::SetCursorPos(cursorOffset + unitSize * ImVec2(9, 3) + GRID_PADDING/2);
        if (ImGui::Button("A", unitSize - GRID_PADDING))
        {
            // Handle Down button pressed
        }



        ImGui::End();
    }

    void menuBar() {

        bool show_menu = true;

        if (show_menu && ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            { 
                if (ImGui::MenuItem("Open", "Ctrl+O"))
                {
                    NFD_Init();

                    nfdchar_t *outPath;
                    nfdfilteritem_t filterItem[] = { { "iNes Files", "nes,zip" } };
                    nfdresult_t result = NFD_OpenDialog(&outPath, filterItem, sizeof(filterItem)/sizeof(nfdfilteritem_t), NULL);
                    if (result == NFD_OKAY)
                    {
                        m_emu.open(outPath);
                        NFD_FreePath(outPath);
                    }
                    else if (result == NFD_CANCEL)
                    {
                        puts("User pressed cancel.");
                    }
                    else 
                    {
                        printf("Error: %s\n", NFD_GetError());
                    }

                    NFD_Quit();
                    
                }
                if (ImGui::MenuItem("Quit", "Ctrl+Q"))
                {
                    quit();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Video"))
            {
                if (ImGui::BeginMenu("VSync")) {

                    for(int i = OFF; i <= ADAPTATIVE ; i++) {                        
                        if(ImGui::MenuItem(VSYNC_TYPE_LABELS[i], nullptr, i == m_currentAudioDeviceIndex)) {
                            m_vsync = (VSyncType)i;
                        }      
                    }              
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem("Scanlines", "Ctrl+S", m_scanlinesFlag))
                {
                    m_scanlinesFlag = !m_scanlinesFlag;
                }
                if (ImGui::MenuItem("Horizontal Stretch", "Ctrl+H", m_horizontalStretch))
                {
                    m_horizontalStretch = !m_horizontalStretch;
                    m_updateObjectsFlag = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Audio"))
            {
                if (ImGui::BeginMenu("Device")) {

                    for(int i = 0; i < m_audioDevices.size(); i++) {                        
                        if(ImGui::MenuItem(m_audioDevices[i].c_str(), nullptr, i == m_currentAudioDeviceIndex)) {
                            m_currentAudioDeviceIndex = i;
                            audioOutput.config(m_audioDevices[i]);
                        }      
                    }              
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Input"))
            {
                if (ImGui::MenuItem("Controller 1"))
                {
                    m_controllerConfigWindow.show(m_controller1);                    
                }
                if (ImGui::MenuItem("Controller 2"))
                {
                    m_controllerConfigWindow.show(m_controller2);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }

    virtual void paintGL()  override {

        mainLoop();

        glClear(GL_COLOR_BUFFER_BIT);

        if(m_updateObjectsFlag) {
            m_updateObjectsFlag = false;
            updateBuffers();
        }

        m_vao.bind();

        glBindTexture(GL_TEXTURE_2D, m_texture);

        if(m_shaderProgram.bind()) {
            m_shaderProgram.setUniformValue("MVPMatrix", m_mvp);
            m_shaderProgram.setUniformValue("Texture", 0);
            m_shaderProgram.setUniformValue("Scanlines", m_scanlinesFlag ? 256 : 0);
            //m_shaderProgram.setUniformValue("GrayScale", m_grayScaleOnRewind && m_emu.isRewinding());
            m_shaderProgram.setUniformValue("GrayScale", false);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            //std::cout << "draw" << std::endl;

            m_shaderProgram.release();
        }

        m_vao.release();      
            
        
        ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

        menuBar();        

        //NESControllerDraw();

        m_controllerConfigWindow.update();


        /*
        ImGui::Begin("another");
            ImDrawList* drawList = ImGui::GetWindowDrawList();           
            

            ImVec2 vMin = ImGui::GetWindowContentRegionMin();
			ImVec2 vMax = ImGui::GetWindowContentRegionMax();

			vMin.x += ImGui::GetWindowPos().x;
			vMin.y += ImGui::GetWindowPos().y;
			vMax.x += ImGui::GetWindowPos().x;
			vMax.y += ImGui::GetWindowPos().y;

            ImVec2 size = vMax - vMin;
       
            float ref= std::min(size.x,size.y);

            drawList->AddImage((void*)m_texture, vMin, vMax);
            drawList->AddCircle(vMin+size/2,ref/2,0xFFFFFFFF);

            

        ImGui::End();    
        */

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        

    }
};

int main(int argc, char* argv[]) {

    MyApp app;

    app.create("Meu App", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_RESIZABLE);

    app.run();

}
