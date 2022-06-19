#include <jo/jo.h>

// Buffer size and offsets definition
#define BUFFER_WIDTH (160)
#define BUFFER_HEIGHT (112)
#define BUFFER_VSKIP (10)  //Shift left value. Example : 224 lines, only keep 56, you take 1 line out of 4, but you also need to take into account the buffer max width.

// Frame buffer in HWRam (this is our image we copy the VDP1 render to)
jo_img FrameBuffer;

// Window location and clipping definition
#define ZLIMIT 200
#define TV_NTSC_WIDTH (704)     // Do not edit this one!
#define TV_NTSC_WIDTH_2 (352)   // Do not edit this one!

// Scene camera
jo_camera Cam;

// Cube that will be rendered outside of the visible area
jo_vertice CubeOutsideVertices[] = JO_3D_CUBE_VERTICES(28);
jo_3d_quad CubeOutsideQuads[6];
jo_rot3D CubeOutsideRotation;

// Are we rendering first frame?
bool FirstFrame = true;

/** @brief This will fetch the data from VDP1 frame buffer into our frame buffer in HWRAM.
 */
void GetFrameBufferVDP1()
{
    // Wait for VDP1 to finish drawing
    // This is not needed if your rendering code takes longer than 16ms (not rendering at 60fps), as this could make your game crash
    // There can also be graphical errors, which are result of incorrect timing with VDP1. to solve this try to move this function GetFrameBufferVDP1();
    // to be called at end of your rendering loop instead of vblank (that might require some more trickery tho)
    while (!JO_VDP1_EDSR);

    // Copy part of the frame buffer
    for (int i = 0; i < BUFFER_HEIGHT; i++)
    {
        DMA_ScuMemCopy((void*)(&FrameBuffer.data[i * BUFFER_WIDTH]), (void*)(JO_VDP1_FB + TV_NTSC_WIDTH + (i << BUFFER_VSKIP)), BUFFER_WIDTH * 2);
    }
}

/** @brief Copy frame buffer of VDP1 either as a texture or to VDP2 NBG1 layer. This function is called during vblank
 */
void CopyBufferVDP1(void)
{
    // Fetch VDP1 frame buffer into our frame buffer in HWRam
    GetFrameBufferVDP1();

    // Set rendered frame as NBG1 layer
    jo_vdp2_set_nbg1_image(&FrameBuffer, 0, 0);
}

/** @brief Draw cube that is outside of visible area
 */
void DrawOutsideCube()
{
    jo_3d_push_matrix();
    {
        jo_3d_rotate_matrix(CubeOutsideRotation.rx, CubeOutsideRotation.ry, CubeOutsideRotation.rz);
        jo_3d_draw_array(CubeOutsideQuads, 6);
    }
    jo_3d_pop_matrix();
}

/** @brief Make cube that will be in the outside area andwill be rendered to an image
 */
void CreateOutsideCube()
{
    // Initialize cube rotation
    CubeOutsideRotation.rx = 0;
    CubeOutsideRotation.ry = 0;
    CubeOutsideRotation.rz = 0;

    // Initialize cube quads
    jo_3d_create_cube(CubeOutsideQuads, CubeOutsideVertices);

    for (int i = 0; i < 6; ++i)
    {
        jo_3d_set_texture(&CubeOutsideQuads[i], 0);

        // This will make quad draw only inside of slWindow (jo_3d_window)
        JO_ADD_FLAG(CubeOutsideQuads[i].data.attbl[0].atrb, Window_In);
    }
}

/** @brief This will draw a solid colored sprite to clear the frame buffer
 *  @param color Color to clear buffer with
 */
void ClearFrameBuffer(const jo_color color, bool halfTransparent)
{
    // Define sprite attributes (solid color polygon)
    SPR_ATTR attribute = SPR_ATTRIBUTE(No_Texture, color, No_Gouraud, Window_In, sprPolygon);
    
    if (halfTransparent)
    {
        attribute.atrb |= CL_Trans;
    }

    // Define sprite points
    FIXED x = jo_int2fixed(BUFFER_WIDTH >> 1);
    FIXED y = jo_int2fixed(BUFFER_HEIGHT >> 1);
    FIXED fourPoints[4][2] = {
        { -x, -y },
        { x, -y },
        { x, y },
        { -x, y }
    };

    // Draw this solid colored sprite, that will clear frame buffer
    slDispSprite4P((FIXED *)fourPoints, (FIXED)jo_int2fixed(ZLIMIT), &attribute);
}

/** @brief Main demo loop
 */
void DemoLogic()
{
    // Rotate cube in outside area
    CubeOutsideRotation.rx += 2;
    CubeOutsideRotation.ry -= 2;
    CubeOutsideRotation.rz += 2;
}

/** @brief Redering loop
 */
void DemoDraw()
{
    // Set camera transform
    jo_3d_camera_look_at(&Cam);

    // We will render the outside area first to have it ready for transfer by the time rendering loop ends or vblank fires
    // Set window to outside region
    slCurWindow(winFar);
    jo_3d_window(
        TV_NTSC_WIDTH_2,                        // This sets X offset from left
        0,                                      // This sets Y offset from top
        (TV_NTSC_WIDTH + BUFFER_WIDTH) - 1,     // This sets X offset from right
        BUFFER_HEIGHT - 1,                      // This sets Y offset from bottom
        ZLIMIT,                                 // This sets projection limit (how deep of a space from forward boundry will be projected)
        TV_NTSC_WIDTH_2 + (BUFFER_WIDTH >> 1),  // This sets projection center point X coordinate
        BUFFER_HEIGHT >> 1);                    // This sets projection center point Y coordinate

    // We need to clear frame buffer before drawing anything
    // Clearing frame buffer is not needed if our scene covers it whole. Here we need it as our cube covers only portion.
    // On very first frame we need to draw solid black polygon, since the framebuffer in that outside region can be whatever, but most likely white
    ClearFrameBuffer(JO_COLOR_Black, !FirstFrame);

    // Draw cube
    DrawOutsideCube();

    // Set window to fullscreen and draw in visible area
    jo_3d_window(0,0,JO_TV_WIDTH,JO_TV_HEIGHT, ZLIMIT, JO_TV_WIDTH_2, JO_TV_HEIGHT_2);

    if (FirstFrame)
    {
        FirstFrame = false;
    }
}

/** @brief Application entry point
 */
void jo_main(void)
{
    // Initialize Jo-Engine with default back background
    jo_core_init(JO_COLOR_Black);

    // Set window size to 512 (this allows us to have VDP1 render outside of the visible area)
    *(Uint16*)(JO_VDP1_VRAM+20)=511;

    // Initialize our frame buffer (image where render from VDP1 will be)
    FrameBuffer.width = BUFFER_WIDTH;
    FrameBuffer.height = BUFFER_HEIGHT;
    FrameBuffer.data = jo_malloc(sizeof(jo_color) * (BUFFER_WIDTH * BUFFER_HEIGHT));

    // Initialize camera
    jo_3d_camera_init(&Cam);

    // Load texture for box drawn outside of visible area
    jo_sprite_add_tga(JO_ROOT_DIR, "BOX.TGA", JO_COLOR_Transparent);

    // Initialize cubes
    CreateOutsideCube();

    // Bind START+ABC to go to cd player
    jo_core_set_restart_game_callback(jo_goto_boot_menu);

    // Register callbacks
    jo_core_add_vblank_callback(CopyBufferVDP1);
    jo_core_add_callback(DemoDraw);
    jo_core_add_callback(DemoLogic);

    // Title text
    jo_printf(9,1,"demo - blur");

    // Description
    jo_printf(1,3,"This demo shows VDP1 rendering");
    jo_printf(1,4,"a rotating cube offscreen and then");
    jo_printf(1,5,"bluring it by rendering half");
    jo_printf(1,6,"transparent quad over it.");

    // Credits
    jo_set_printf_color_index(JO_COLOR_INDEX_Red);
    jo_printf(1,22,"Demo made by Reye");

    // Reset printf color
    jo_set_printf_color_index(JO_COLOR_INDEX_White);

    // Move NBG1 so that the cube rendered on it is in middle of the screen
    jo_move_background(-((JO_TV_WIDTH - BUFFER_WIDTH) / 2), -((JO_TV_HEIGHT - BUFFER_HEIGHT) / 2));

    // Start demo
    jo_core_run();
}
