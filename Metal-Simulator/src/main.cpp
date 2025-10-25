#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <iostream>
#include <optional>
#include <variant>

int main() {
    std::cout << "=== Simple Metal Backend Test ===" << std::endl;
    
    // Very basic window setup for Metal compatibility
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.majorVersion = 2;  // OpenGL 2.1 for maximum compatibility
    settings.minorVersion = 1;
    settings.attributeFlags = sf::ContextSettings::Default;
    
    sf::RenderWindow window(sf::VideoMode({800, 600}), 
                           "Metal Test", 
                           sf::Style::Default,
                           sf::State::Windowed,
                           settings);
    
    bool activeResult = window.setActive(true);
    if (!activeResult) {
        std::cerr << "Warning: Could not activate OpenGL context" << std::endl;
    }
    
    std::cout << "\n=== OpenGL Information ===" << std::endl;
    std::cout << "Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "Vendor: " << glGetString(GL_VENDOR) << std::endl;
    
    window.setVerticalSyncEnabled(true);
    window.setFramerateLimit(60);
    
    std::cout << "\n=== Starting Tests ===" << std::endl;
    std::cout << "You should see:" << std::endl;
    std::cout << "1. Changing background colors" << std::endl;
    std::cout << "2. A colored triangle using immediate mode" << std::endl;
    std::cout << "Press ESC to exit, SPACE to cycle tests" << std::endl;
    
    int testPhase = 0;
    sf::Clock clock;
    
    // Event handlers
    const auto onClose = [&](const sf::Event::Closed&) {
        std::cout << "Window close requested" << std::endl;
        window.close();
    };
    
    const auto onKeyPressed = [&](const sf::Event::KeyPressed& keyPressed) {
        if (keyPressed.scancode == sf::Keyboard::Scancode::Escape) {
            std::cout << "Escape pressed - exiting" << std::endl;
            window.close();
        } else if (keyPressed.scancode == sf::Keyboard::Scancode::Space) {
            testPhase = (testPhase + 1) % 6;
            const char* phases[] = {
                "Red Background", 
                "Green Background", 
                "Blue Background", 
                "Immediate Mode Triangle", 
                "Color Cycling",
                "VBO Triangle Test"
            };
            std::cout << "Test phase: " << phases[testPhase] << std::endl;
        }
    };
    
    // Main render loop
    while (window.isOpen()) {
        float time = clock.getElapsedTime().asSeconds();
        
        // Handle events using SFML 3.0 style
        window.handleEvents(onClose,
                           [](const sf::Event::Resized&){},
                           onKeyPressed,
                           [](const sf::Event::KeyReleased&){},
                           [](const sf::Event::MouseButtonPressed&){},
                           [](const sf::Event::MouseButtonReleased&){},
                           [](const sf::Event::MouseMoved&){},
                           [](const sf::Event::MouseWheelScrolled&){});
        
        // Auto-advance phases every 2 seconds (except for triangle tests)
        if (testPhase < 3 && (int)time % 2 == 0 && time > (testPhase + 1) * 2) {
            testPhase = (testPhase + 1) % 6;
        }
        
        switch(testPhase) {
            case 0: // Red background
                glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
                break;
                
            case 1: // Green background
                glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
                break;
                
            case 2: // Blue background
                glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
                break;
                
            case 3: // Immediate mode triangle
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black background
                glClear(GL_COLOR_BUFFER_BIT);
                
                // Use old-school immediate mode - should work on any OpenGL
                glBegin(GL_TRIANGLES);
                    glColor3f(1.0f, 0.0f, 0.0f); // Red vertex
                    glVertex3f(0.0f, 0.5f, 0.0f);
                    
                    glColor3f(0.0f, 1.0f, 0.0f); // Green vertex
                    glVertex3f(-0.5f, -0.5f, 0.0f);
                    
                    glColor3f(0.0f, 0.0f, 1.0f); // Blue vertex
                    glVertex3f(0.5f, -0.5f, 0.0f);
                glEnd();
                break;
                
            case 4: // Color cycling
                {
                    float r = (sin(time) + 1.0f) * 0.5f;
                    float g = (sin(time + 2.0f) + 1.0f) * 0.5f;
                    float b = (sin(time + 4.0f) + 1.0f) * 0.5f;
                    glClearColor(r, g, b, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT);
                }
                break;
                
            case 5: // VBO test
                {
                    static GLuint VBO = 0;
                    static bool vboCreated = false;
                    
                    if (!vboCreated) {
                        float vertices[] = {
                             0.0f,  0.3f, 0.0f,  // Top
                            -0.3f, -0.3f, 0.0f,  // Bottom Left
                             0.3f, -0.3f, 0.0f   // Bottom Right
                        };
                        
                        glGenBuffers(1, &VBO);
                        glBindBuffer(GL_ARRAY_BUFFER, VBO);
                        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
                        glBindBuffer(GL_ARRAY_BUFFER, 0);
                        vboCreated = true;
                        std::cout << "VBO created with ID: " << VBO << std::endl;
                    }
                    
                    glClearColor(0.2f, 0.2f, 0.2f, 1.0f); // Dark gray
                    glClear(GL_COLOR_BUFFER_BIT);
                    
                    if (VBO > 0) {
                        // Try VBO rendering with vertex arrays (pre-shader)
                        glBindBuffer(GL_ARRAY_BUFFER, VBO);
                        glEnableClientState(GL_VERTEX_ARRAY);
                        glVertexPointer(3, GL_FLOAT, 0, 0);
                        
                        glColor3f(1.0f, 1.0f, 0.0f); // Yellow triangle
                        glDrawArrays(GL_TRIANGLES, 0, 3);
                        
                        glDisableClientState(GL_VERTEX_ARRAY);
                        glBindBuffer(GL_ARRAY_BUFFER, 0);
                    }
                }
                break;
        }
        
        window.display();
        
        // Check for OpenGL errors
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << "OpenGL Error in phase " << testPhase << ": " << error << std::endl;
        }
    }
    
    std::cout << "=== Test Results ===" << std::endl;
    std::cout << "Which tests worked for you?" << std::endl;
    std::cout << "1. Background colors (red, green, blue)?" << std::endl;
    std::cout << "2. Colored triangle (immediate mode)?" << std::endl;
    std::cout << "3. Color cycling animation?" << std::endl;
    std::cout << "4. Yellow triangle (VBO mode)?" << std::endl;
    
    return 0;
}