/*  date = December 25th 2025 08:54 PM */ 
// main.h
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

enum Message
{
	// Framework Messages
	//------------------
	MSG_PAINT,			// dp = pointer to Painter
	MSG_LAYOUT,
	//------------------

	// User Messages
	//-----------------
	MSG_USER,
	//-----------------
};

struct Rectangle
{
	int l, r, t, b;
};

struct Painter
{
	Rectangle clip;		// The rectangle the element should draw into
	uint32_t *bits;		// The bitmap itself. bits[y * painter->width + x] gives the RGB value of pixel (x, y).
	int width, height;	// width and height of bitmap
};

// element is the specific element that's receiving the message, making it possible
// for different elements to use the same message hanadler function. This is good
// because there will usually be more than one of a specific type of element.
// For e.g., an interface will usually contain lots of different labels, all of 
// which could share the same message handler. When responding to say, the pain
// message, the essage handler for the label would presumably cast the element
// to a Label * and draw the tex in a text field, or similar.

// The message field if the identifier of the specific message; Message is an enum.
// For now, we haven't got any messages, but let's put MSG_USER in the enum. This 
// will always be the last message in the enum, and indicates the message identifiers
// that the user of the lirbary are free to define themselves: MSG_USER + 0, MSG_USER + 1,
// etc.

// The di and dp fields of the callback are used to pass the parameters of the message. 
// I have found that it is typical to want to send either an integer, pointer or both 
// in a message, which is how I settled on this configuration. You could remove di and 
// just store the data for all messages within the dp pointer, but that would be less 
// easy to use. The return value int of the callback is again defined by the message type, 
// although 0 should be returned by the element if it doesn't recognize or respond to a
// particular message. In this way, an empty message handler looks like this:
// int EmptyMessageHandler(Element *element, Message message, int di, void *dp) {return 0; }
typedef int (*MessageHandler)(struct Element *element, Message message, int di, void *dp);


struct Element
{
	uint32_t flags;			// First 16 bits are specific to the type of element (button, label, etc.). The higher order 16 bits are common to all elements.
	uint32_t childCount;	// The number of child elements
	Rectangle bounds, clip;	// bounds indicate where the element exists in the window. clip stores the subrectangle of the element's bounds that is actually visibile and interactable.
	Element *parent;
	Element **children;
	struct Window *window;	// Window at the root of the heirarchy
	void *cp;				// Context pointer (for the user of the library)
	MessageHandler messageClass, messageUser;	// messageClass: class handler, default behaviour; messageUser: optional override
};

struct Window
{
	Element e;
	uint32_t *bits;		// The bitmap image of the window's content
	int width, height;	// drawable size
	Rectangle updateRegion;


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


// @os_per_backend functions
void Initialise();
int MessageLoop();
Window *WindowCreate(const char *cTitle, int width, int height);

////////////////////////////////////
//- Core UI Logic

Element *ElementCreate(size_t bytes, Element *parent, uint32_t flags, MessageHandler messageClass);
void ElementRepaint(Element *element, Rectangle *region);
void ElementMove(Element *element, Rectangle bounds, bool alwaysLayout);
int ElementMessage(Element *element, Message message, int di, void *dp);

////////////////////////////////////
//- Helpers

Rectangle RectangleMake(int l, int r, int t, int b);
Rectangle RectangleIntersection(Rectangle a, Rectangle b); 	// Compute the intersection of the rectangles, i.e. the biggest rectangle that fits into both. If the rectangles don't overlap, an invalid rectangle is returned (as per RectangleValid).
Rectangle RectangleBounding(Rectangle a, Rectangle b); 		// Compute the smallest rectangle containing both of the input rectangles.
bool RectangleValid(Rectangle a);							// valid if width and height are positive
bool RectangleEquals(Rectangle a, Rectangle b); 			// Returns true if all sides are equal.
bool RectangleContains(Rectangle a, int x, int y); 			// Returns true if the pixel with its top-left at the given coordinate is contained inside the rectangle.

void StringCopy(char **destination, size_t *destinationBytes, const char *source, ptrdiff_t sourceBytes);

void DrawBlock(Painter *painter, Rectangle r, uint32_t fill);
