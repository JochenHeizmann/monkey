
// Window title.
//
#define WINDOW_TITLE "Monkey Game"

// Window width.
// Set to 0 to use desktop width.
//
#define WINDOW_WIDTH 640

// Window height.
// Set to 0 to use desktop height.
//
#define WINDOW_HEIGHT 480					//set to 0 for desktop height

// Window mode.
// Set to GLFW_WINDOW for windowed mode, or GLFW_FULLSCREEN for fullscreen mode.
//
#define WINDOW_MODE GLFW_WINDOW				//set to GLFW_FULLSCREEN for a fullscreen window

// Window sizing mode.
// Set to GL_TRUE for a 'locked' window, GL_FALSE for a resizeable window.
//
#define WINDOW_NO_RESIZE GL_TRUE			//set to GL_FALSE for a resizeable window

// Garbage collection mode.
// Set to 1 for incremental GC (experimental), 0 for mark-and-sweep (stable).
//
#define INCREMENTAL_GC 0					//set to 1 to enable incremental GC
