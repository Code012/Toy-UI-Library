/*  date = December 25th 2025 08:54 PM */ 

#include <stdint.h>
#include <stddef.h>
#include <cstring>
#include <cstdlib>

#if OS_WINDOWS
#define Rectangle W32Rectangle
#include <windows.h>
#undef Rectangle
#endif

#if OS_LINUX
#define Window X11Window
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#undef Window
#endif


////////////////////////////////////
//- Definitions

struct Rectangle
{
	int l, r, t, b;
};

struct Window
{
	uint32_t *bits;		// The bitmap image of the window's content
	int width, height;	// drawable size


#if OS_WINDOWS
	HWND hwnd;
	bool trackingLeave; // for mouse input
#endif

#if OS_LINUX
	X11Window window;
	XImage *image;
#endif

};

struct GlobalState
{
	Window **windows;
	size_t windowCount;	// number of open windows; number of pointers in the windows array above.

#if OS_LINUX
	Display *display;
	Visual *visual;
	Atom windowClosedID;
#endif
};

////////////////////////////////////
//- Helpers

Rectangle RectangleMake(int l, int r, int t, int b);
Rectangle RectangleIntersection(Rectangle a, Rectangle b); 	// Compute the intersection of the rectangles, i.e. the biggest rectangle that fits into both. If the rectangles don't overlap, an invalid rectangle is returned (as per RectangleValid).
Rectangle RectangleBounding(Rectangle a, Rectangle b); 		// Compute the smallest rectangle containing both of the input rectangles.
bool RectangleValid(Rectangle a);							// valid if width and height are positive
bool RectangleEquals(Rectangle a, Rectangle b); 			// Returns true if all sides are equal.
bool RectangleContains(Rectangle a, int x, int y); 			// Returns true if the pixel with its top-left at the given coordinate is contained inside the rectangle.

void StringCopy(char **destination, size_t *destinationBytes, const char *source, ptrdiff_t sourceBytes);
