#pragma once

#include <SDL.h>

//#include <GL/glu.h>
#include <iostream>

#include <filesystem>
namespace fs = std::filesystem;

#include "CppGL/GLHeaders.h"

#include "imgui_include.h"
#include "imgui_util.h"

#include "ControllerConfigWindow.h"
#include "ShortcutManager.h"

#ifdef __EMSCRIPTEN__
    #include "EmscriptenUtil.h"
#else
    #include <nfd.h>
#endif

#include <vector>

#include <functional>
#include <iterator>
#include <regex>

#include <glm/glm.hpp>

#include <nlohmann/json.hpp>

#include "logger/logger.h"

#include "CppGL/CppGL.h"
#include "SDLOpenGLWindow.h"

#include "GeraNES/GeraNESEmu.h"

#include "GeraNES/defines.h"

#ifdef __EMSCRIPTEN__
    #include "GeraNESApp/OpenALAudioOutput.h"
    typedef OpenALAudioOutput AudioOutput;
#else   
    #include "GeraNESApp/SDLAudioOutput.h"
    typedef SDLAudioOutput AudioOutput;
#endif


#include "GeraNESApp/InputManager.h"
#include "GeraNESApp/ControllerInfo.h"
#include "GeraNESApp/AppSettings.h"

#include "GeraNES/util/CircularBuffer.h"

#include "signal/signal.h"

#include "cmrc/cmrc.hpp"
#include "util/const_util.h"

CMRC_DECLARE(resources);

#include "TouchControls.h"

const std::string LOG_FILE = "log.txt";

class GeraNESApp : public SDLOpenGLWindow, public SigSlot::SigSlotBase {

private:

    ControllerConfigWindow m_controllerConfigWindow;

    std::vector<std::string> m_audioDevices;

    bool m_horizontalStretch = false;
    int m_clipHeightValue = 8;

    GLVertexArrayObject m_vao;
    GLVertexBufferObject m_vbo;

    GLShaderProgram m_shaderProgram;

    bool m_updateObjectsFlag = true;

    GLuint m_texture = 0;

    bool m_fullScreen = false;

    glm::mat4x4 m_mvp = glm::mat4x4(1.0f);

    AudioOutput m_audioOutput;

    GeraNESEmu m_emu;

    ControllerInfo m_controller1;
    ControllerInfo m_controller2; 

    bool m_emuInputEnabled = true;

    enum VSyncMode {OFF, SYNCRONIZED, ADAPTATIVE};
    VSyncMode m_vsyncMode = OFF;

    enum FilterMode {NEAREST, BILINEAR};
    FilterMode m_filterMode = NEAREST;

    bool m_showImprovementsWindow = false;
    bool m_showAboutWindow = false;

    bool m_showMenuBar = true;

    std::string m_errorMessage = "";
    bool m_showErrorWindow = false;

    ShortcutManager m_shortcuts;

    int m_menuBarHeight = 0;

    std::vector<char> m_logBuf = {'\0'};
    std::string m_log = "";
    bool m_showLogWindow = false;

    Uint64 m_mainLoopLastTime = 0;

    // FPS vars    
    Uint64 m_fpsTimer = 0;
    int m_fps = 0;
    int m_frameCounter = 0;

    static constexpr std::array<const char*, 3> VSYNC_TYPE_LABELS {"Off", "Syncronized", "Adaptative"};
    static constexpr std::array<const char*, 3> FILTER_TYPE_LABELS {"Nearest", "Bilinear"};    

    struct ShaderItem {
        std::string label;
        std::string path;
    };

    std::vector<ShaderItem> shaderList;

    std::unique_ptr<TouchControls> m_touch;

    void updateMVP() {
        glm::mat4 proj = glm::ortho(0.0f, (float)width(), (float)height(), 0.0f, -1.0f, 1.0f);           
        m_mvp = proj * glm::mat4(1.0f);
    }    

    void onLog(const std::string& msg, Logger::Type type) {
        std::ofstream file(LOG_FILE, std::ios_base::app);
        file << msg << std::endl;
        std::cout << msg << std::endl;

        if(type == Logger::Type::ERROR) {
            m_errorMessage = msg;
            m_showErrorWindow = true;
        }

        std::string msgType = "";

        switch(type) {
            case Logger::Type::INFO: msgType = "[Info] "; break;
            case Logger::Type::WARNING: msgType = "[Warning] "; break;
            case Logger::Type::ERROR: msgType = "[Error] "; break;
            case Logger::Type::DEBUG: msgType = "[Debug] "; break;
        }

        m_log += msgType + msg + "\n";
        
        size_t needed = m_log.size() + 1;
        if (m_logBuf.capacity() < needed) {
            m_logBuf.reserve( std::max(needed, m_logBuf.capacity() * 2) );
        }

        m_logBuf.resize(needed);
        memcpy(m_logBuf.data(), m_log.c_str(), needed);        
    }

    void onFrameStart() { 

        if(m_emuInputEnabled) {

            InputManager& im = InputManager::instance();        

            im.updateInputs();

            // Player1
            m_emu.setController1Buttons(
                im.isPressed(m_controller1.a) || m_touch->buttons().a,
                im.isPressed(m_controller1.b) || m_touch->buttons().b,
                im.isPressed(m_controller1.select) || m_touch->buttons().select,
                im.isPressed(m_controller1.start) || m_touch->buttons().start,
                im.isPressed(m_controller1.up) || m_touch->buttons().up,
                im.isPressed(m_controller1.down) || m_touch->buttons().down,
                im.isPressed(m_controller1.left) || m_touch->buttons().left,
                im.isPressed(m_controller1.right) || m_touch->buttons().right
            );

            if(im.isJustPressed(m_controller1.saveState)) m_emu.saveState();
            if(im.isJustPressed(m_controller1.loadState)) m_emu.loadState();            

            // Player 2
            m_emu.setController2Buttons(
                im.isPressed(m_controller2.a), im.isPressed(m_controller2.b),
                im.isPressed(m_controller2.select), im.isPressed(m_controller2.start),
                im.isPressed(m_controller2.up), im.isPressed(m_controller2.down),
                im.isPressed(m_controller2.left), im.isPressed(m_controller2.right)
            );

            if(im.isJustPressed(m_controller2.saveState)) m_emu.saveState();
            if(im.isJustPressed(m_controller2.loadState)) m_emu.loadState();

            // Rewind
            m_emu.setRewind(
                im.isPressed(m_controller1.rewind) ||
                im.isPressed(m_controller2.rewind) ||
                m_touch->buttons().rewind
            );            
        }

    }

    void openFile(const char* path) {

        AppSettings::instance().data.addRecentFile(path);
        AppSettings::instance().data.setLastFolder(path);
        m_emu.open(path);
        const std::string filename = fs::path(path).filename().string();
        setTitle((std::string("GeraNES (") + filename + ")").c_str());    
    }

public:

    GeraNESApp() : m_emu(m_audioOutput) {

        //reset log file content
        std::ofstream file(LOG_FILE);
        file.close();

        Logger::instance().signalLog.bind(&GeraNESApp::onLog, this);
        m_emu.signalFrameStart.bind(&GeraNESApp::onFrameStart, this);    

        m_controllerConfigWindow.signalShow.bind(&GeraNESApp::onCaptureBegin, this);
        m_controllerConfigWindow.signalClose.bind(&GeraNESApp::onCaptureEnd, this);

        m_audioDevices = m_audioOutput.getAudioList();

        auto cfg = AppSettings::instance().data;

        m_audioOutput.config(cfg.audio.audioDevice);      
        cfg.audio.audioDevice = m_audioOutput.currentDeviceName(); 
        m_audioOutput.setVolume(cfg.audio.volume);
        
        cfg.input.getControllerInfo(0, m_controller1);
        cfg.input.getControllerInfo(1, m_controller2);

        m_emu.setupRewindSystem(cfg.improvements.maxRewindTime > 0, cfg.improvements.maxRewindTime);
        m_emu.disableSpriteLimit(cfg.improvements.disableSpritesLimit);
        m_emu.enableOverclock(cfg.improvements.overclock);

        m_vsyncMode = (VSyncMode)cfg.video.vsyncMode;
        m_filterMode = (FilterMode)cfg.video.filterMode;
        m_horizontalStretch = cfg.video.horizontalStretch;
        m_fullScreen = cfg.video.fullScreen;

        // std::string key;
        // std::string label;
        // std::string shortcut;
        // std::function<void()> action;
        m_shortcuts.add(ShortcutManager::Data{"fullscreen", "Fullscreen", "Alt+F", [this]() {
            m_fullScreen = !isFullScreen();
            setFullScreen(m_fullScreen);            
            AppSettings::instance().data.video.fullScreen = m_fullScreen;
        }});

        m_shortcuts.add(ShortcutManager::Data{"openRom", "Open Rom", "Alt+O", [this]() {
            openRom();
        }});

        m_shortcuts.add(ShortcutManager::Data{"quit", "Quit", "Alt+Q", [this]() {
            quit();
        }});

        m_shortcuts.add(ShortcutManager::Data{"horizontalStretch", "Horizontal Stretch", "Alt+H", [this]() {
            m_horizontalStretch = !m_horizontalStretch;
            AppSettings::instance().data.video.horizontalStretch = m_horizontalStretch;
            m_updateObjectsFlag = true;
        }});

        m_shortcuts.add(ShortcutManager::Data{"saveState", "Save State", "Alt+S", [this]() {
            m_emu.saveState();
        }});

        m_shortcuts.add(ShortcutManager::Data{"loadState", "Load State", "Alt+L", [this]() {
            m_emu.loadState();
        }});

        loadShaderList();
        
        #ifdef __EMSCRIPTEN__
            emcriptenRegisterAudioReset(reinterpret_cast<int>(this));
        #endif
        
    }

    void loadShaderList() {

        const char* SHADER_DIR = "shaders/";

        std::string dir = fs::path(SHADER_DIR).parent_path().string();
        if(!fs::exists(dir)) fs::create_directory(dir);

        for (const auto& entry : fs::directory_iterator(dir)) {
            if (fs::is_regular_file(entry.path())) {
                ShaderItem item = {entry.path().filename().string(), entry.path().string()};         
                shaderList.push_back(item);
            }
        }

        std::sort(shaderList.begin(), shaderList.end(), [](const ShaderItem& a, const ShaderItem& b) {
        return a.label < b.label;
    });
    }

    /**
     * Process the file uploaded in browser
    */
    void processFile(const char* fileName, size_t fileSize, const uint8_t* fileContent) {

        FILE* file = fopen(fileName, "w");

        if (file) {
            
            size_t written = fwrite(fileContent, sizeof(uint8_t), fileSize, file);

            if (written != fileSize) {
                Logger::instance().log("Failed writing file in processFile call", Logger::Type::ERROR);
            }

            fclose(file);

            openFile(fileName);

        } else {
            Logger::instance().log("Failed to open file for writing in processFile call", Logger::Type::ERROR);
        }

    }

    void restartAudioModule() {
        m_audioOutput.restart();
    }

    void onCaptureBegin() {
        m_emuInputEnabled = false;
    }

    void onCaptureEnd() {

        m_emuInputEnabled = true;        

        //save both
        AppSettings::instance().data.input.setControllerInfo(0, m_controller1);
        AppSettings::instance().data.input.setControllerInfo(1, m_controller2);
    }

    virtual ~GeraNESApp() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }

    void openRom() {

        if(isFullScreen()) minimizeWindow();

        #ifdef __EMSCRIPTEN__
            emcriptenFileDialog(reinterpret_cast<int>(this));
        #else

        NFD_Init();     

        nfdchar_t *outPath;
        nfdfilteritem_t filterItem[] = { { "iNes", "nes,zip" }, "Patch", "ips,ups,bps" };

        nfdresult_t result = NFD_OpenDialog(&outPath, filterItem, sizeof(filterItem)/sizeof(nfdfilteritem_t),
            (AppSettings::instance().data.getLastFolder()).c_str());

        if (result == NFD_OKAY)
        {
            openFile(outPath);                        
            NFD_FreePath(outPath);
        }
        else if (result == NFD_CANCEL)
        {
            //puts("User pressed cancel.");
        }
        else 
        {
            Logger::instance().log(NFD_GetError(), Logger::Type::ERROR);
        }

        NFD_Quit();

        #endif

        if(m_fullScreen) restoreWindow();
    } 

    void updateVSyncConfig() {
        switch(m_vsyncMode) {
            case OFF: setVSync(0); break;
            case SYNCRONIZED: setVSync(1); break;
            case ADAPTATIVE: setVSync(-1); break;
        }
    }

    void updateFilterConfig() {
        switch(m_filterMode) {
            case NEAREST:
                glBindTexture(GL_TEXTURE_2D, m_texture); 
                //glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE); //legacy
                glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                break;
            case BILINEAR:
                glBindTexture(GL_TEXTURE_2D, m_texture); 
                //glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE); //legacy
                glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                break;   
        }
    }

    void updateShaderConfig() {

        bool loaded = false;

        const std::string& shaderName = AppSettings::instance().data.video.shaderName;

        if(shaderName != "") {
        
            for(const ShaderItem& item : shaderList) {
                if(item.label == shaderName) {
                    loaded = loadShader(item.path);
                    break;
                }
            }
        }

        if(!loaded) {

            if(shaderName != "") {
                Logger::instance().log("Failed to load shader " + shaderName + ". Using default shader.", Logger::Type::INFO);
                AppSettings::instance().data.video.shaderName = "";
            }
            loadShader(""); //default
        }
    }

    virtual bool initGL() override {

        setFullScreen(m_fullScreen);

        if (SDL_Init(SDL_INIT_TIMER) < 0) {
            Logger::instance().log("SDL_Init error", Logger::Type::ERROR);
            return false;
        }

        #ifndef __EMSCRIPTEN__
        GLenum err = glewInit();
        if (GLEW_OK != err)
        {
            Logger::instance().log((const char*)(glewGetErrorString(err)), Logger::Type::ERROR);
            return false;
        }
        #endif    

        updateVSyncConfig();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        const char* glsl_version = "#version 100";

        // Setup Platform/Renderer bindings
        ImGui_ImplSDL2_InitForOpenGL(sdlWindow(), glContext());
        ImGui_ImplOpenGL3_Init(glsl_version);


        glClearColor(0.0, 0.0, 0.0, 0.0);

        updateShaderConfig();

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glDisable(GL_DEPTH_TEST);

        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);

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

        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        updateFilterConfig();

        updateMVP();

        m_touch = std::make_unique<TouchControls>(m_controller1, width(), height(), GetWindowDPI());

        // ImGuiIO& io = ImGui::GetIO();
        // io.Fonts->Clear();  // limpa qualquer fonte existente

        // ImFontConfig cfg;
        // cfg.SizePixels = 18.0f;

        // // default font
        // io.Fonts->AddFontDefault(&cfg);

        // // rebuild
        // io.Fonts->Build();

        return true;
    }

    bool loadShader(const std::string& path)
    {   

        auto fs2 = cmrc::resources::get_filesystem();
        auto shaderFile = fs2.open("resources/default.glsl");
        std::string shaderText(shaderFile.begin(), shaderFile.end());        

        if(path != "") {
            std::ifstream file(path);            
            shaderText = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        }     

        std::string vertexText;
        std::string fragmentText;

        std::regex versionPattern(R"(^#version[^\n]*\n)", std::regex::icase);

        if(std::regex_search(shaderText, versionPattern)) {
            vertexText = std::regex_replace(shaderText, versionPattern, R"($&#define VERTEX)" "\n");
            fragmentText = std::regex_replace(shaderText, versionPattern, R"($&#define FRAGMENT)" "\n");
        }
        else {
            vertexText = "#define VERTEX\n" + shaderText;
            fragmentText = "#define FRAGMENT\n" + shaderText;
        }

        m_shaderProgram.destroy();

        m_shaderProgram.create();      

        if(!m_shaderProgram.addShaderFromSourceCode(GLShaderProgram::Vertex, vertexText.c_str())){
            Logger::instance().log(std::string("vertex shader errors:\n") + m_shaderProgram.lastError(), Logger::Type::ERROR);
            m_shaderProgram.destroy();
            return false;
        }

        if(!m_shaderProgram.addShaderFromSourceCode(GLShaderProgram::Fragment, fragmentText.c_str())){
            Logger::instance().log(std::string("fragment shader errors:\n") + m_shaderProgram.lastError(), Logger::Type::ERROR);    
            m_shaderProgram.destroy();
            return false;
        }

        m_shaderProgram.bindAttributeLocation("VertexCoord", 0);
        m_shaderProgram.bindAttributeLocation("TexCoord", 1);

        if(!m_shaderProgram.link()){
            Logger::instance().log(std::string("shader link error: ") + m_shaderProgram.lastError(), Logger::Type::ERROR);   
            m_shaderProgram.destroy();
            return false;
        }

        return true;
    }

    void updateBuffers()
    {
        if(!m_vbo.isCreated()){
            m_vbo.create();
        }

        std::vector<GLfloat> data;

        SDL_Rect clientArea = { 0, m_menuBarHeight, width(), std::max(0, height() - m_menuBarHeight) };

        if( width()/256.0 >= height()/(240.0-2*m_clipHeightValue))
        {
            GLfloat drawWidth = m_horizontalStretch ? clientArea.w : 256.0/(240.0-2*m_clipHeightValue) * clientArea.h;
            GLfloat offsetX = (clientArea.w - drawWidth)/2.0;

            //glVertex2f(offsetX,0);]
            data.push_back(clientArea.x+offsetX);
            data.push_back(clientArea.y);
            //glTexCoord2f(0.0,m_clipHeightValue/256.0 ); //top left
            data.push_back(0.0);
            data.push_back(m_clipHeightValue/256.0);

            //glVertex2f(offsetX,height());
            data.push_back(clientArea.x+offsetX);
            data.push_back(clientArea.y+clientArea.h);
            //glTexCoord2f(0.0,240.0/256.0 - m_clipHeightValue/256.0 ); //bottom left
            data.push_back(0.0);
            data.push_back(240.0/256.0 - m_clipHeightValue/256.0);

            //glVertex2f(offsetX+screenWidth,0);
            data.push_back(clientArea.x+offsetX+drawWidth);
            data.push_back(clientArea.y);
            //glTexCoord2f(1.0, m_clipHeightValue/256.0 ); //top right
            data.push_back(1.0);
            data.push_back(m_clipHeightValue/256.0);

            //glVertex2f(offsetX+screenWidth,height());
            data.push_back(clientArea.x+offsetX+drawWidth);
            data.push_back(clientArea.y+clientArea.h);
            //glTexCoord2f(1.0,240.0/256.0 - m_clipHeightValue/256.0 ); //bottom right
            data.push_back(1.0);
            data.push_back(240.0/256.0 - m_clipHeightValue/256.0);
        }
        else
        {
            GLfloat drawHeight = (240.0-2*m_clipHeightValue)/256.0 * clientArea.w;
            GLfloat offsetY = (clientArea.h - drawHeight)/2.0;

            //glVertex2f(0,offsetY);
            data.push_back(clientArea.x);
            data.push_back(clientArea.y+offsetY);
            //glTexCoord2f(0.0, m_clipHeightValue/256.0 ); //top left
            data.push_back(0.0);
            data.push_back(m_clipHeightValue/256.0);

            //glVertex2f(0,offsetY+screenHeight);
            data.push_back(clientArea.x);
            data.push_back(clientArea.y+offsetY+drawHeight);
            //glTexCoord2f(0.0,240.0/256.0 - m_clipHeightValue/256.0 ); //bottom left
            data.push_back(0.0);
            data.push_back(240.0/256.0 - m_clipHeightValue/256.0);

            //glVertex2f(width(),offsetY);
            data.push_back(clientArea.x+clientArea.w);
            data.push_back(clientArea.y+offsetY);
            //glTexCoord2f(1.0,0.0 + m_clipHeightValue/256.0 ); //top right
            data.push_back(1.0);
            data.push_back(0.0 + m_clipHeightValue/256.0);

            //glVertex2f(width(),offsetY+screenHeight);
            data.push_back(clientArea.x+clientArea.w);
            data.push_back(clientArea.y+offsetY+drawHeight);
            //glTexCoord2f(1.0,240.0/256.0 - m_clipHeightValue/256.0 ); //bottom right
            data.push_back(1.0);
            data.push_back(240.0/256.0 - m_clipHeightValue/256.0);
        }  

        m_vbo.bind();
        if( (size_t)m_vbo.size() != data.size()*sizeof(GLfloat))
            m_vbo.allocate(&data[0], data.size()*sizeof(GLfloat));
        else m_vbo.write(0,&data[0], data.size()*sizeof(GLfloat));
        m_vbo.release();

        if(m_touch) m_touch->setTopMargin(m_menuBarHeight);

    }

    virtual bool onEvent(SDL_Event& event) override {

        ImGui_ImplSDL2_ProcessEvent(&event);

        ImGuiIO &io = ImGui::GetIO();

        bool imGuiWantsMouse = io.WantCaptureMouse;
        bool imGuiWantsKeyboard = io.WantCaptureKeyboard;

        switch(event.type) {

            case SDL_KEYDOWN: {

                if(imGuiWantsKeyboard) break;

                std::string keyName = SDL_GetKeyName(event.key.keysym.sym);

                if(event.key.keysym.mod & KMOD_ALT) keyName = "Alt+" + keyName;

                m_shortcuts.invokeShortcut(keyName);

                if(keyName == "Escape" && m_emuInputEnabled) {
                    m_showMenuBar = !m_showMenuBar;
                    m_updateObjectsFlag = true;
                }

                break;
            }                

            case SDL_WINDOWEVENT:

                switch(event.window.event) {

                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        updateMVP();
                        m_touch->onResize(width(),height());               
                        m_updateObjectsFlag = true;
                        break;
                }
                break;  
                
        }

        if(!imGuiWantsMouse) m_touch->onEvent(event);

        return SDLOpenGLWindow::onEvent(event);
    }    

    void mainLoop()
    {
        int displayFrameRate = getDisplayFrameRate();

        Uint64 tempTime = SDL_GetTicks64();

        Uint64 dt = tempTime - m_mainLoopLastTime;

        if(dt == 0) return;

        m_touch->update(dt);

        m_mainLoopLastTime = tempTime;
 
        m_fpsTimer += dt;        

        if(m_fpsTimer >= 1000)
        {
            int cycles = m_fpsTimer / 1000;
            m_fps = m_frameCounter / cycles;
            m_frameCounter = 0;
            m_fpsTimer %= 1000; 
        }

        if(m_vsyncMode == OFF || displayFrameRate != m_emu.getRegionFPS()) {          
            if( m_emu.update(dt) ) render();
        }
        else {
            m_emu.updateUntilFrame(dt);
            render();      
        }

        m_frameCounter++;
    }

    void render()
    {
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, m_clipHeightValue, 256, 240-2*m_clipHeightValue, GL_RGBA, GL_UNSIGNED_BYTE, m_emu.getFramebuffer()+m_clipHeightValue*256);
    }    

    void menuBar() {

        bool show_menu = true;

        if (show_menu && ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            { 
                auto sc = m_shortcuts.get("openRom");
                if( sc != nullptr) {

                    if (ImGui::MenuItem(sc->label.c_str(), sc->shortcut.c_str()))
                    {
                        sc->action();                        
                    }
                }

                auto recentFiles = AppSettings::instance().data.getRecentFiles();
                if (ImGui::BeginMenu("Recent Files", recentFiles.size() > 0))
                {
                    for(int i = 0; i < recentFiles.size(); i++) {
                        if(ImGui::MenuItem(recentFiles[i].c_str())) {
                            openFile(recentFiles[i].c_str());
                        } 
                    }
                    ImGui::EndMenu();
                }

                ImGui::Separator();

                sc = m_shortcuts.get("quit");
                if( sc != nullptr) {

                    if (ImGui::MenuItem(sc->label.c_str(), sc->shortcut.c_str()))
                    {
                        sc->action();                        
                    }
                }
                      
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Emulator"))
            {
                auto sc = m_shortcuts.get("saveState");
                if( sc != nullptr) {

                    if (ImGui::MenuItem(sc->label.c_str(), sc->shortcut.c_str()))
                    {
                        sc->action();                        
                    }
                }

                sc = m_shortcuts.get("loadState");
                if( sc != nullptr) {

                    if (ImGui::MenuItem(sc->label.c_str(), sc->shortcut.c_str()))
                    {
                        sc->action();                        
                    }
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Improvements")) {
                    m_showImprovementsWindow = true;                                       
                }

                ImGui::Separator();

                if (ImGui::BeginMenu("Region")) {

                    if(ImGui::MenuItem("NTSC", nullptr, m_emu.region() == Settings::Region::NTSC)) {
                        m_emu.setRegion(Settings::Region::NTSC);
                    }

                    if(ImGui::MenuItem("PAL", nullptr, m_emu.region() == Settings::Region::PAL)) {
                        m_emu.setRegion(Settings::Region::PAL);
                    }

                    ImGui::EndMenu();                                      
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Video"))
            {
                if (ImGui::BeginMenu("VSync")) {

                    for(int i = OFF; i <= ADAPTATIVE ; i++) {                        
                        if(ImGui::MenuItem(VSYNC_TYPE_LABELS[i], nullptr, m_vsyncMode == i)) {
                            m_vsyncMode = (VSyncMode)i;
                            AppSettings::instance().data.video.vsyncMode = i;
                            updateVSyncConfig();
                        }      
                    }              
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Filter")) {

                    for(int i = NEAREST; i <= BILINEAR ; i++) {                        
                        if(ImGui::MenuItem(FILTER_TYPE_LABELS[i], nullptr, m_filterMode == i)) {
                            m_filterMode = (FilterMode)i;
                            AppSettings::instance().data.video.filterMode = i;
                            updateFilterConfig();
                        }      
                    }              
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Shader")) {

                    if(ImGui::MenuItem("default", nullptr, AppSettings::instance().data.video.shaderName == "")) {                            
                        AppSettings::instance().data.video.shaderName = "";
                        updateShaderConfig();
                    }

                    if(shaderList.size() > 0) ImGui::Separator();

                    for(const ShaderItem& item: shaderList) {
                        if(ImGui::MenuItem(item.label.c_str(), nullptr, item.label == AppSettings::instance().data.video.shaderName)) {                            
                            AppSettings::instance().data.video.shaderName = item.label;
                            updateShaderConfig();
                        } 
                    }               
                    ImGui::EndMenu();
                }

                auto sc = m_shortcuts.get("horizontalStretch");
                if( sc != nullptr) {

                    if (ImGui::MenuItem(sc->label.c_str(), sc->shortcut.c_str(), m_horizontalStretch))
                    {
                        sc->action();                        
                    }
                }

                sc = m_shortcuts.get("fullscreen");
                if( sc != nullptr) {

                    if (ImGui::MenuItem(sc->label.c_str(), sc->shortcut.c_str(), isFullScreen()))
                    {
                        sc->action();                        
                    }
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Audio"))
            {
                if (ImGui::BeginMenu("Device")) {

                    for(int i = 0; i < m_audioDevices.size(); i++) {

                        bool checked = m_audioOutput.currentDeviceName() == m_audioDevices[i].c_str();

                        if(ImGui::MenuItem(m_audioDevices[i].c_str(), nullptr, checked)) {
                            m_audioOutput.config(m_audioDevices[i]);
                            AppSettings::instance().data.audio.audioDevice = m_audioOutput.currentDeviceName();
                        }      
                    }              
                    ImGui::EndMenu();
                }

                float volume = m_audioOutput.getVolume();
                if(ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f, "%.2f")) {
                    m_audioOutput.setVolume(volume);
                    AppSettings::instance().data.audio.volume = volume;
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Input"))
            {
                if (ImGui::BeginMenu("Controller")) {

                    if (ImGui::MenuItem("1"))
                    {
                        m_controllerConfigWindow.show("Controller 1 config", m_controller1);                    
                    }
                    if (ImGui::MenuItem("2"))
                    {
                        m_controllerConfigWindow.show("Controller 2 config", m_controller2);
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Touch controls")) {

                    bool enabled = AppSettings::instance().data.input.touchControls.enabled;
                    if(ImGui::MenuItem("Enabled", nullptr, enabled)) {
                        AppSettings::instance().data.input.touchControls.enabled = !enabled;
                    }   

                    if (ImGui::BeginMenu("Digital pad mode")) {
                        int digitalPadMode = (int)AppSettings::instance().data.input.touchControls.digitalPadMode;
                        for(int i = (int)DigitaPadMode::Absolute; i <= (int)DigitaPadMode::Relative ; i++) {                        
                            if(ImGui::MenuItem(DigitaPadModeLabels[i], nullptr, digitalPadMode == i)) {
                                AppSettings::instance().data.input.touchControls.digitalPadMode = (DigitaPadMode)i;
                            }      
                        }              
                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu("Buttons mode")) {
                        int buttonsMode = (int)AppSettings::instance().data.input.touchControls.buttonsMode;
                        for(int i = (int)ButtonsMode::Absolute; i <= (int)ButtonsMode::Column ; i++) {                        
                            if(ImGui::MenuItem(ButtonsModeLabels[i], nullptr, buttonsMode == i)) {
                                AppSettings::instance().data.input.touchControls.buttonsMode = (ButtonsMode)i;
                            }      
                        }              
                        ImGui::EndMenu();
                    }

                    if(ImGui::SliderFloat("Transparency", &AppSettings::instance().data.input.touchControls.transparency, 0.0f, 1.0f, "%.2f")) {
                    }

                    ImGui::EndMenu();
                }


                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Debug"))
            {
                if (ImGui::MenuItem("Show FPS", nullptr, &AppSettings::instance().data.debug.showFps))
                {             
                }   

                if (ImGui::MenuItem("Log"))
                {
                    m_showLogWindow = true;    
                }   
                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("About"))
            {
                m_showAboutWindow = true;              
            }

            ImGui::EndMainMenuBar();
        }

        m_menuBarHeight = ImGui::GetFrameHeight();
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

            m_shaderProgram.setUniformValue("FrameDirection", m_emu.isRewinding() ? -1 : 1);
            m_shaderProgram.setUniformValue("FrameCount", m_emu.frameCount());
            m_shaderProgram.setUniformValue("OutputSize", glm::vec2((float)width(),(float)height()));
            m_shaderProgram.setUniformValue("TextureSize", glm::vec2(256,256));
            m_shaderProgram.setUniformValue("InputSize", glm::vec2(256,256));

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);            

            m_shaderProgram.release();
        }

        m_vao.release();      
            
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        //ImGui::ShowDemoWindow();

        showOverlay();
        showGui();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());       
    }

    void showGui()
    {
        float lastMenuBarHeight = m_menuBarHeight;

        if(m_showMenuBar) menuBar();
        else  m_menuBarHeight = 0;
        
        if(lastMenuBarHeight != m_menuBarHeight) updateBuffers(); 
 
        m_controllerConfigWindow.update();

        if(m_showImprovementsWindow) {

            ImGui::SetNextWindowSize(ImVec2(320, 0));   

            if(ImGui::Begin("Improvements", &m_showImprovementsWindow, ImGuiWindowFlags_NoResize)) {
                
                bool disableSpritesLimit = m_emu.spriteLimitDisabled();
                if(ImGui::Checkbox("Disable Sprites Limit", &disableSpritesLimit)) { 
                    m_emu.disableSpriteLimit(disableSpritesLimit);                        
                }
                AppSettings::instance().data.improvements.disableSpritesLimit = m_emu.spriteLimitDisabled();

                bool overclock = m_emu.overclocked();                     
                if(ImGui::Checkbox("Overclock", &overclock)) {                   
                    m_emu.enableOverclock(overclock);                        
                }
                AppSettings::instance().data.improvements.overclock = m_emu.overclocked();

                ImGui::SetNextItemWidth(100);

                int value = AppSettings::instance().data.improvements.maxRewindTime;               
                if(ImGui::InputInt("Max Rewind Time(s)", &value)) {
                    value = std::max(0,value);                       
                    AppSettings::instance().data.improvements.maxRewindTime = value;
                    m_emu.setupRewindSystem(value > 0, value);
                }
            }                     
            
            ImGui::End();
        }

        if(m_showAboutWindow) {

            ImGui::SetNextWindowSize(ImVec2(320, 0));         

            if (ImGui::Begin("About", &m_showAboutWindow, ImGuiWindowFlags_NoResize)) {
        
                std::string txt = std::string(GERANES_NAME) + " " + GERANES_VERSION;
                
                TextCenteredWrapped(txt);

                txt = std::string("Racionisoft 2015 - ") + std::to_string(compileTimeYear());

                ImGui::NewLine();

                TextCenteredWrapped(txt);    

                ImGui::NewLine();
                ImGui::NewLine();

                txt = "geraldoracioni@gmail.com";

                TextCenteredWrapped(txt);           
            }

            ImGui::End();
        }
        
        if(m_showErrorWindow) {

            ImGui::SetNextWindowSize(ImVec2(320, 0));

            bool lastState = m_showErrorWindow;

            if (ImGui::Begin("Error", &m_showErrorWindow))
            {
                float windowWidth = ImGui::GetContentRegionAvail().x;

                TextCenteredWrapped(m_errorMessage.c_str());

                ImGui::Spacing();
                ImGui::Spacing();            
                
                const char* btnLabel = "OK";

                // Button size
                ImVec2 btnSize = ImGui::CalcTextSize(btnLabel);
                btnSize.x += ImGui::GetStyle().FramePadding.x * 2.0f;
                btnSize.y += ImGui::GetStyle().FramePadding.y * 2.0f;

                //Calculate center x
                float posX = (windowWidth - btnSize.x) * 0.5f;

                ImGui::SetCursorPosX(posX);

                if (ImGui::Button(btnLabel, btnSize))
                {
                    m_showErrorWindow = false; // fecha janela
                }
            }
            ImGui::End();

            if(lastState && !m_showErrorWindow) m_showErrorWindow = false; 
        }

        if(m_showLogWindow) {

            ImGui::SetNextWindowSize(ImVec2(600, 0), ImGuiCond_Once);

            if (ImGui::Begin("Log", &m_showLogWindow))
            {
                ImGui::InputTextMultiline("MyMultilineInput", m_logBuf.data(), m_logBuf.size(),
                        ImVec2(-1, 400), ImGuiInputTextFlags_ReadOnly);

                if (!(ImGui::IsItemActive() || ImGui::IsItemEdited()))
                {
                    ImGuiContext& g = *GImGui;
                    const char* child_window_name = NULL;
                    ImFormatStringToTempBuffer(&child_window_name, NULL, "%s/%s_%08X", g.CurrentWindow->Name, "MyMultilineInput", ImGui::GetID("MyMultilineInput"));
                    ImGuiWindow* child_window = ImGui::FindWindowByName(child_window_name);

                    if (child_window)
                    {
                        ImGui::SetScrollY(child_window, child_window->ScrollMax.y);
                    }
                }

                ImGui::Spacing();

                const char* btnLabel = "Clear";
                ImVec2 btnTextSize = ImGui::CalcTextSize(btnLabel);
                ImVec2 btnSize = ImVec2(btnTextSize.x + ImGui::GetStyle().FramePadding.x * 2.0f,
                                        btnTextSize.y + ImGui::GetStyle().FramePadding.y * 2.0f);

                float windowWidth = ImGui::GetContentRegionAvail().x;
                float posX = (windowWidth - btnSize.x) * 0.5f;
                ImGui::SetCursorPosX(posX);

                if (ImGui::Button(btnLabel, btnSize))
                {
                    m_log.clear();
                    m_logBuf.clear();
                    m_logBuf.push_back('\0');
                }
            }

            ImGui::End();
        }
    }

    void showOverlay()
    {
        ImDrawList* drawList = ImGui::GetForegroundDrawList();

        if(AppSettings::instance().data.debug.showFps) {

            const int fontSize = 32;            

            std::string fpsText = std::to_string(m_fps);
            ImVec2 fpsTextSize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0, fpsText.c_str());
        
            const ImVec2 pos = ImVec2(width()- fpsTextSize.x - 32, 40);
            
            DrawTextOutlined(drawList, nullptr, fontSize, pos, 0xFFFFFFFF, 0xFF000000, fpsText.c_str());
        }

        m_touch->draw(drawList);
    }   
 
};
