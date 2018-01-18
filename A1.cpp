#include "A1.hpp"
#include "cs488-framework/GlErrorCheck.hpp"

#include <iostream>

#include <imgui/imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace glm;
using namespace std;

static const size_t DIM = 16;
static const size_t MAX_HEIGHT = 20;
static const size_t NUM_COLOUR = 8;

//----------------------------------------------------------------------------------------
// Constructor
A1::A1()
    : current_col( 0 ),
    m_grid( DIM ),
    m_rotating( false )

{
    colour = new float[ NUM_COLOUR * 3 ];
    reset();
}

//----------------------------------------------------------------------------------------
// Destructor
A1::~A1()
{
    delete []colour;
}

//----------------------------------------------------------------------------------------
/*
 * Called once, at program start.
 */
void A1::init()
{
    // Set the background colour.
    glClearColor( 0.3, 0.5, 0.7, 1.0 );

    // Build the shader
    m_shader.generateProgramObject();
    m_shader.attachVertexShader(
        getAssetFilePath( "VertexShader.vs" ).c_str() );
    m_shader.attachFragmentShader(
        getAssetFilePath( "FragmentShader.fs" ).c_str() );
    m_shader.link();

    // Set up the uniforms
    P_uni = m_shader.getUniformLocation( "P" );
    V_uni = m_shader.getUniformLocation( "V" );
    M_uni = m_shader.getUniformLocation( "M" );
    col_uni = m_shader.getUniformLocation( "colour" );

    initGrid();

    // Set up initial view and projection matrices (need to do this here,
    // since it depends on the GLFW window being set up correctly).
    view = glm::lookAt(
        glm::vec3( 0.0f, float(DIM)*2.0*M_SQRT1_2, float(DIM)*2.0*M_SQRT1_2 ),
        glm::vec3( 0.0f, 0.0f, 0.0f ),
        glm::vec3( 0.0f, 1.0f, 0.0f ) );

    proj = glm::perspective(
        glm::radians( 45.0f ),
        float( m_framebufferWidth ) / float( m_framebufferHeight ),
        1.0f, 1000.0f );
}

void A1::initGrid()
{
    /*
     * Grid VBO, VAO setup
     */

    size_t sz = 3 * 2 * 2 * (DIM+3);

    float *verts = new float[ sz ];
    size_t ct = 0;
    for( int idx = 0; idx < DIM+3; ++idx ) {
        verts[ ct ] = -1;
        verts[ ct+1 ] = 0;
        verts[ ct+2 ] = idx-1;
        verts[ ct+3 ] = DIM+1;
        verts[ ct+4 ] = 0;
        verts[ ct+5 ] = idx-1;
        ct += 6;

        verts[ ct ] = idx-1;
        verts[ ct+1 ] = 0;
        verts[ ct+2 ] = -1;
        verts[ ct+3 ] = idx-1;
        verts[ ct+4 ] = 0;
        verts[ ct+5 ] = DIM+1;
        ct += 6;
    }

    // Create the vertex array to record buffer assignments.
    glGenVertexArrays( 1, &m_grid_vao );
    glBindVertexArray( m_grid_vao );

    // Create the grid vertex buffer
    glGenBuffers( 1, &m_grid_vbo );
    glBindBuffer( GL_ARRAY_BUFFER, m_grid_vbo );
    glBufferData( GL_ARRAY_BUFFER, sz*sizeof(float),
        verts, GL_STATIC_DRAW );

    // Specify the means of extracting the position values properly.
    GLint posAttrib = m_shader.getAttribLocation( "position" );
    glEnableVertexAttribArray( posAttrib );
    glVertexAttribPointer( posAttrib, 3, GL_FLOAT, GL_FALSE, 0, nullptr );

    // Reset state to prevent rogue code from messing with *my*
    // stuff!
    glBindVertexArray( 0 );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
    CHECK_GL_ERRORS;

    /*
     * Cube VBO, VAO setup
     */

    /*
     *
     *    *------ x
     *    |
     *    |
     *    |
     *    z
     *
     *
     *  Inner square below is when y = 0
     *  Outer square belowis when y = 1
     *
     *    4            5
     *
     *       0     1
     *
     *       3     2
     *
     *    7            6
     */

    // specify coordinate of a unit cube in element buffer
    float cube_verts[] = {
        0.0f, 0.0f, 0.0f, // y=0, top left
        1.0f, 0.0f, 0.0f, // y=0, top right
        1.0f, 0.0f, 1.0f, // y=0, bot right
        0.0f, 0.0f, 1.0f, // y=0, bot left

        0.0f, 1.0f, 0.0f, // y=1, top left
        1.0f, 1.0f, 0.0f, // y=1, top right
        1.0f, 1.0f, 1.0f, // y=1, bot right
        0.0f, 1.0f, 1.0f  // y=1, bot left
    };
    GLuint indices[] = {
        0, 1, 2, //bottom square
        2, 3, 0,

        4, 5, 6, //bottom square
        6, 7, 4,

        0, 4, 7, //side square 1
        7, 3, 0,

        1, 5, 6, //side square 2
        6, 2, 1,

        0, 1, 5, //side square 3
        5, 4, 0,

        3, 2, 6, //side square 4
        6, 7, 3,
    };


    // Set up new VAO "environment" for cubes
    glGenVertexArrays( 1, &m_cube_vao );
    glBindVertexArray( m_cube_vao );

    // Create the cube vertex buffer
    glGenBuffers( 1, &m_cube_vbo );
    glBindBuffer( GL_ARRAY_BUFFER, m_cube_vbo );
    glBufferData( GL_ARRAY_BUFFER, sizeof(cube_verts),
        cube_verts, GL_STATIC_DRAW );

    // Create the cube vertex element buffer
    glGenBuffers( 1, &m_cube_ebo );
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, m_cube_ebo );
    glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof(indices),
        indices, GL_STATIC_DRAW );


    // Specify the means of extracting the position values properly.
    posAttrib = m_shader.getAttribLocation( "position" );
    glEnableVertexAttribArray( posAttrib );
    glVertexAttribPointer( posAttrib, 3, GL_FLOAT, GL_FALSE, 0, nullptr );


    // OpenGL has the buffer now, there's no need for us to keep a copy.
    delete [] verts;

    glBindVertexArray( 0 );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
    CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, before guiLogic().
 */
void A1::appLogic()
{
    // Place per frame, application logic here ...
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after appLogic(), but before the draw() method.
 */
void A1::guiLogic()
{
    // We already know there's only going to be one window, so for
    // simplicity we'll store button states in static local variables.
    // If there was ever a possibility of having multiple instances of
    // A1 running simultaneously, this would break; you'd want to make
    // this into instance fields of A1.
    static bool showTestWindow(false);
    static bool showDebugWindow(true);

    ImGuiWindowFlags windowFlags(ImGuiWindowFlags_AlwaysAutoResize);
    float opacity(0.5f);

    ImGui::Begin("Debug Window", &showDebugWindow, ImVec2(100,100), opacity, windowFlags);
        if( ImGui::Button( "Quit Application" ) ) {
            glfwSetWindowShouldClose(m_window, GL_TRUE);
        }

        if( ImGui::Button( "Reset Controls" ) ) {
            reset();
        }

        // Eventually you'll create multiple colour widgets with
        // radio buttons.  If you use PushID/PopID to give them all
        // unique IDs, then ImGui will be able to keep them separate.
        // This is unnecessary with a single colour selector and
        // radio button, but I'm leaving it in as an example.

        // Prefixing a widget name with "##" keeps it from being
        // displayed.

        for ( int i = 0; i < NUM_COLOUR; i++ ) {
            ImGui::PushID( i );
            ImGui::ColorEdit3( "##Colour", (colour+i*3) );
            ImGui::SameLine();
            if( ImGui::RadioButton( "##Col", &current_col, i ) ) {
                // Select this colour.
                m_grid.setColour( m_active_x, m_active_z, current_col );
            }
            ImGui::PopID();
        }

/*
        // For convenience, you can uncomment this to show ImGui's massive
        // demonstration window right in your application.  Very handy for
        // browsing around to get the widget you want.  Then look in
        // shared/imgui/imgui_demo.cpp to see how it's done.
        if( ImGui::Button( "Test Window" ) ) {
            showTestWindow = !showTestWindow;
        }
*/

        ImGui::Text( "Framerate: %.1f FPS", ImGui::GetIO().Framerate );

    ImGui::End();

    if( showTestWindow ) {
        ImGui::ShowTestWindow( &showTestWindow );
    }
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after guiLogic().
 */
void A1::draw()
{
    // Create a global transformation for the model (centre it).
    vec3 y_axis(0.0f, 1.0f, 0.0f);

    mat4 W;
    W = glm::rotate( W, m_rot_angle, y_axis );
    W = glm::scale( W, vec3(m_scale) );
    W = glm::translate( W, vec3( -float(DIM)/2.0f, 0, -float(DIM)/2.0f ) );

    m_shader.enable();

    {
        glEnable( GL_DEPTH_TEST );



        glUniformMatrix4fv( P_uni, 1, GL_FALSE, value_ptr( proj ) );
        glUniformMatrix4fv( V_uni, 1, GL_FALSE, value_ptr( view ) );
        glUniformMatrix4fv( M_uni, 1, GL_FALSE, value_ptr( W ) );

        // draw the grid
        glBindVertexArray( m_grid_vao );
        glUniform3f( col_uni, 1, 1, 1 );
        glDrawArrays( GL_LINES, 0, (3+DIM)*4 );

        glBindVertexArray( 0 );
        CHECK_GL_ERRORS;



        // draw cubes by repeatly drawing a unit cube
        // transformed into desired position
        glBindVertexArray( m_cube_vao );
        for ( int x = 0; x < DIM; x++ ) {
            for ( int z = 0; z < DIM; z++ ) {

                if ( x == m_active_x && z == m_active_z ) {
                    continue;
                }

                int h = m_grid.getHeight( x, z );
                int c = m_grid.getColour( x, z );
                for ( int y = 0; y < h; y++ ) {

                    mat4 Trans = W;
                    // honestly, maybe I should undo the global translation,
                    // perform this translation, then redo the global translation
                    // but since translations are communative, I'll just do this
                    Trans = glm::translate( Trans, vec3( x, y, z ) );
                    glUniformMatrix4fv( M_uni, 1, GL_FALSE, value_ptr( Trans ) );

                    float r = colour[ 3*c ];
                    float g = colour[ 3*c + 1 ];
                    float b = colour[ 3*c + 2 ];
                    glUniform3f( col_uni, r, g, b );
                    glDrawElements( GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

                }

            }
        }

        {
            // outline active column in green, add skeleton of EXTRA cube on top
            // this draw an EXTRA SKELETON CUBE
            glDisable( GL_DEPTH_TEST );
            int h = m_grid.getHeight( m_active_x, m_active_z );
            int c = m_grid.getColour( m_active_x, m_active_z );
            float r = colour[ 3*c ];
            float g = colour[ 3*c + 1 ];
            float b = colour[ 3*c + 2 ];
            for ( int y = 0; y < h+1; y++ ) {

                mat4 Trans = W;
                // honestly, maybe I should undo the global translation,
                // perform this translation, then redo the global translation
                // but since translations are communative, I'll just do this
                Trans = glm::translate( Trans, vec3( m_active_x, y, m_active_z ) );
                glUniformMatrix4fv( M_uni, 1, GL_FALSE, value_ptr( Trans ) );


                if ( y < h ) {
                    glUniform3f( col_uni, r, g, b );
                    glDrawElements( GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
                }

                glUniform3f( col_uni, 0, 0, 0 );
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glDrawElements( GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }
            glEnable( GL_DEPTH_TEST );
        }



        glBindVertexArray( 0 );
        CHECK_GL_ERRORS;
    }

    m_shader.disable();

    // Restore defaults
    glBindVertexArray( 0 );
    CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
/*
 * Called once, after program is signaled to terminate.
 */
void A1::cleanup()
{}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles cursor entering the window area events.
 */
bool A1::cursorEnterWindowEvent (
        int entered
) {
    bool eventHandled(false);

    // Fill in with event handling code...

    return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse cursor movement events.
 */
bool A1::mouseMoveEvent(double xPos, double yPos)
{
    bool eventHandled(false);

    if (!ImGui::IsMouseHoveringAnyWindow()) {
        // Put some code here to handle rotations.  Probably need to
        // check whether we're *dragging*, not just moving the mouse.
        // Probably need some instance variables to track the current
        // rotation amount, and maybe the previous X position (so
        // that you can rotate relative to the *change* in X.

        if ( m_rotating ) {
            m_rot_angle += ( xPos - m_mouse_x ) / 100.0f;
        }

        m_mouse_x = xPos;
        m_mouse_y = yPos;

        eventHandled = true;
    }

    return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse button events.
 */
bool A1::mouseButtonInputEvent(int button, int actions, int mods) {
    bool eventHandled(false);

    if (!ImGui::IsMouseHoveringAnyWindow()) {
        // The user clicked in the window.  If it's the left
        // mouse button, initiate a rotation.

        if ( button == GLFW_MOUSE_BUTTON_LEFT && actions == GLFW_PRESS ) {
            m_rotating = true;
            m_rot_init_x = m_mouse_x;
            eventHandled = true;

        } else if ( button == GLFW_MOUSE_BUTTON_LEFT && actions == GLFW_RELEASE ) {
            m_rotating = false;
            eventHandled = true;
        }

    }

    return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse scroll wheel events.
 */
bool A1::mouseScrollEvent(double xOffSet, double yOffSet) {
    bool eventHandled(false);

    // Zoom in or out.
    m_scale = m_scale + yOffSet * 0.5f;
    m_scale = m_scale + xOffSet * 0.5f;

    m_scale = glm::clamp(m_scale, 0.2f, 5.0f);

    eventHandled = true;

    return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles window resize events.
 */
bool A1::windowResizeEvent(int width, int height) {
    bool eventHandled(false);

    // Fill in with event handling code...

    return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles key input events.
 */
bool A1::keyInputEvent(int key, int action, int mods) {
    bool eventHandled(false);

    // Fill in with event handling code...
    if( action == GLFW_PRESS ) {
        // Respond to some key events.
        if ( key == GLFW_KEY_Q ) {
            glfwSetWindowShouldClose(m_window, GL_TRUE);
            eventHandled = true;

        } else if ( key == GLFW_KEY_R ) {
            reset();
            eventHandled = true;

        } else if ( key == GLFW_KEY_BACKSPACE ) {
            int h = m_grid.getHeight( m_active_x, m_active_z );
            h = glm::clamp( h-1, 0, (int)MAX_HEIGHT );
            m_grid.setHeight( m_active_x, m_active_z, h );
            eventHandled = true;

        } else if ( key == GLFW_KEY_SPACE ) {
            int h = m_grid.getHeight( m_active_x, m_active_z );
            h = glm::clamp( h+1, 0, (int)MAX_HEIGHT );
            m_grid.setHeight( m_active_x, m_active_z, h );
            m_grid.setColour( m_active_x, m_active_z, current_col );
            eventHandled = true;

        } else if ( key == GLFW_KEY_UP ) {
            int new_z = glm::clamp( m_active_z-1, 0, (int)(DIM-1) );

            if ( m_active_z != new_z && (mods & GLFW_MOD_SHIFT) ) {
                int h = m_grid.getHeight( m_active_x, m_active_z );
                int c = m_grid.getColour( m_active_x, m_active_z );
                m_grid.setHeight( m_active_x, new_z, h);
                m_grid.setColour( m_active_x, new_z, c);
            }

            m_active_z = new_z;
            eventHandled = true;

        } else if ( key == GLFW_KEY_DOWN ) {
            int new_z = glm::clamp( m_active_z+1, 0, (int)(DIM-1) );

            if ( m_active_z != new_z && (mods & GLFW_MOD_SHIFT) ) {
                int h = m_grid.getHeight( m_active_x, m_active_z );
                int c = m_grid.getColour( m_active_x, m_active_z );
                m_grid.setHeight( m_active_x, new_z, h);
                m_grid.setColour( m_active_x, new_z, c);
            }

            m_active_z = new_z;
            eventHandled = true;

        } else if ( key == GLFW_KEY_LEFT ) {
            int new_x = glm::clamp( m_active_x-1, 0, (int)(DIM-1) );

            if ( m_active_x != new_x && (mods & GLFW_MOD_SHIFT) ) {
                int h = m_grid.getHeight( m_active_x, m_active_z );
                int c = m_grid.getColour( m_active_x, m_active_z );
                m_grid.setHeight( new_x, m_active_z, h);
                m_grid.setColour( new_x, m_active_z, c);
            }

            m_active_x = new_x;
            eventHandled = true;

        } else if ( key == GLFW_KEY_RIGHT ) {
            int new_x = glm::clamp( m_active_x+1, 0, (int)(DIM-1) );

            if ( m_active_x != new_x && (mods & GLFW_MOD_SHIFT) ) {
                int h = m_grid.getHeight( m_active_x, m_active_z );
                int c = m_grid.getColour( m_active_x, m_active_z );
                m_grid.setHeight( new_x, m_active_z, h);
                m_grid.setColour( new_x, m_active_z, c);
            }

            m_active_x = new_x;
            eventHandled = true;
        }

    }

    return eventHandled;
}

// helper functions

/*
 * Reset View, move active block back to (0,0)
 */

void A1::reset() {
    m_active_x = 0;
    m_active_z = 0;

    m_rot_angle = 0;
    m_scale = 1;

    current_col = 0;
    static const float default_colours[] = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.4f, 1.0f,
        0.4f, 0.2f, 1.0f,
        0.6f, 0.0f, 1.0f,

        1.0f, 1.0f, 0.0f,
        1.0f, 0.8f, 0.6f,
        1.0f, 0.6f, 0.2f,
        1.0f, 0.4f, 1.4f,
    };
    memcpy(colour, default_colours, sizeof(default_colours));

    for ( int x = 0; x < DIM; x++ ) {
        for ( int z = 0; z < DIM; z++ ) {
            m_grid.setHeight( x, z, 0 );
            m_grid.setColour( x, z, 0 );
        }
    }
}
