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

#define UPDATE_HOVERED (1)	// when an element starts and stops being hovered
#define UPDATE_PRESSED (2)	// when an element starts and stops being pressed

enum Message
{
	// Framework Messages
	//------------------
	MSG_PAINT,			// dp = pointer to Painter
	MSG_LAYOUT,
	MSG_UPDATE, 		// di = UPDATE_... constant
	MSG_LEFT_DOWN,   	// Left mouse button pressed. (Sent to the element the mouse cursor is over.)
	MSG_LEFT_UP,     	// Left mouse button released. (Sent to the element MSG_LEFT_DOWN was sent to.)
	MSG_MIDDLE_DOWN, 	// Middle mouse button pressed. (Sent to the element the mouse cursor is over.)
	MSG_MIDDLE_UP,   	// Middle mouse button released. (Sent to the element MSG_MIDDLE_DOWN was sent to.)
	MSG_RIGHT_DOWN,  	// Right mouse button pressed. (Sent to the element the mouse cursor is over.)
	MSG_RIGHT_UP,    	// Right mouse button released. (Sent to the element MSG_RIGHT_DOWN was sent to.)
	MSG_MOUSE_MOVE,
	MSG_MOUSE_DRAG,  	// Mouse moved while holding buttons. (Sent to the element MSG_*_DOWN was sent to.)
	MSG_CLICKED,     	// Left mouse button released while hovering over the element that MSG_LEFT_UP was sent to.
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
	Element *hovered, *pressed;
	int pressedButton; 	// 1 = left, 2 = middle, 3 = right
	int cursorX, cursorY;
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
Element *ElementFindByPoint(Element *element, int x, int y);

////////////////////////////////////
//- Helpers

Rectangle RectangleMake(int l, int r, int t, int b);
Rectangle RectangleIntersection(Rectangle a, Rectangle b); 	// Compute the intersection of the rectangles, i.e. the biggest rectangle that fits into both. If the rectangles don't overlap, an invalid rectangle is returned (as per RectangleValid).
Rectangle RectangleBounding(Rectangle a, Rectangle b); 		// Compute the smallest rectangle containing both of the input rectangles.
bool RectangleValid(Rectangle a);							// valid if width and height are positive
bool RectangleEquals(Rectangle a, Rectangle b); 			// Returns true if all sides are equal.
bool RectangleContains(Rectangle a, int x, int y); 			// Returns true if the pixel with its top-left at the given coordinate is contained inside the rectangle.

void StringCopy(char **destination, size_t *destinationBytes, const char *source, ptrdiff_t sourceBytes);
void DrawString(Painter *painter, Rectangle r, const char *string, size_t bytes, uint32_t colour, bool centerAlign);
void DrawRectangle(Painter *painter, Rectangle r, uint32_t fill, uint32_t outline);
void DrawBlock(Painter *painter, Rectangle r, uint32_t fill);

// Taken from https://commons.wikimedia.org/wiki/File:Codepage-437.png
// Public domain.

#define GLYPH_WIDTH (9)		// 8px of data in the font, followed by a 1px gap between glyphs
#define GLYPH_HEIGHT (16)

// one glyph is 8*16*8 (width*height*bits in one byte) = 16 bytes. 
// ignore the uint64_t thats just a convenient way to bitpack
// think in terms of uint8_t
// we interpret a glyph as, a row of 8 bits stacked 16 times
// for e.g.:
/*
	the letter H:

	11000011                          11    11
	11000011                          11    11
	11000011                          11    11
	11000011                          11    11
	11000011                          11    11
	11000011                          11    11
	11000011                          11    11
	11111111       ------->           11111111 
	11111111       ------->           11111111
	11000011                          11    11
	11000011                          11    11
	11000011                          11    11
	11000011                          11    11
	11000011                          11    11
	11000011                          11    11
	11000011                          11    11

	
*/
const uint64_t _font[] = {
	0x0000000000000000UL, 0x0000000000000000UL, 0xBD8181A5817E0000UL, 0x000000007E818199UL, 0xC3FFFFDBFF7E0000UL, 0x000000007EFFFFE7UL, 0x7F7F7F3600000000UL, 0x00000000081C3E7FUL, 
	0x7F3E1C0800000000UL, 0x0000000000081C3EUL, 0xE7E73C3C18000000UL, 0x000000003C1818E7UL, 0xFFFF7E3C18000000UL, 0x000000003C18187EUL, 0x3C18000000000000UL, 0x000000000000183CUL, 
	0xC3E7FFFFFFFFFFFFUL, 0xFFFFFFFFFFFFE7C3UL, 0x42663C0000000000UL, 0x00000000003C6642UL, 0xBD99C3FFFFFFFFFFUL, 0xFFFFFFFFFFC399BDUL, 0x331E4C5870780000UL, 0x000000001E333333UL, 
	0x3C666666663C0000UL, 0x0000000018187E18UL, 0x0C0C0CFCCCFC0000UL, 0x00000000070F0E0CUL, 0xC6C6C6FEC6FE0000UL, 0x0000000367E7E6C6UL, 0xE73CDB1818000000UL, 0x000000001818DB3CUL, 
	0x1F7F1F0F07030100UL, 0x000000000103070FUL, 0x7C7F7C7870604000UL, 0x0000000040607078UL, 0x1818187E3C180000UL, 0x0000000000183C7EUL, 0x6666666666660000UL, 0x0000000066660066UL, 
	0xD8DEDBDBDBFE0000UL, 0x00000000D8D8D8D8UL, 0x6363361C06633E00UL, 0x0000003E63301C36UL, 0x0000000000000000UL, 0x000000007F7F7F7FUL, 0x1818187E3C180000UL, 0x000000007E183C7EUL, 
	0x1818187E3C180000UL, 0x0000000018181818UL, 0x1818181818180000UL, 0x00000000183C7E18UL, 0x7F30180000000000UL, 0x0000000000001830UL, 0x7F060C0000000000UL, 0x0000000000000C06UL, 
	0x0303000000000000UL, 0x0000000000007F03UL, 0xFF66240000000000UL, 0x0000000000002466UL, 0x3E1C1C0800000000UL, 0x00000000007F7F3EUL, 0x3E3E7F7F00000000UL, 0x0000000000081C1CUL, 
	0x0000000000000000UL, 0x0000000000000000UL, 0x18183C3C3C180000UL, 0x0000000018180018UL, 0x0000002466666600UL, 0x0000000000000000UL, 0x36367F3636000000UL, 0x0000000036367F36UL, 
	0x603E0343633E1818UL, 0x000018183E636160UL, 0x1830634300000000UL, 0x000000006163060CUL, 0x3B6E1C36361C0000UL, 0x000000006E333333UL, 0x000000060C0C0C00UL, 0x0000000000000000UL, 
	0x0C0C0C0C18300000UL, 0x0000000030180C0CUL, 0x30303030180C0000UL, 0x000000000C183030UL, 0xFF3C660000000000UL, 0x000000000000663CUL, 0x7E18180000000000UL, 0x0000000000001818UL, 
	0x0000000000000000UL, 0x0000000C18181800UL, 0x7F00000000000000UL, 0x0000000000000000UL, 0x0000000000000000UL, 0x0000000018180000UL, 0x1830604000000000UL, 0x000000000103060CUL, 
	0xDBDBC3C3663C0000UL, 0x000000003C66C3C3UL, 0x1818181E1C180000UL, 0x000000007E181818UL, 0x0C183060633E0000UL, 0x000000007F630306UL, 0x603C6060633E0000UL, 0x000000003E636060UL, 
	0x7F33363C38300000UL, 0x0000000078303030UL, 0x603F0303037F0000UL, 0x000000003E636060UL, 0x633F0303061C0000UL, 0x000000003E636363UL, 0x18306060637F0000UL, 0x000000000C0C0C0CUL, 
	0x633E6363633E0000UL, 0x000000003E636363UL, 0x607E6363633E0000UL, 0x000000001E306060UL, 0x0000181800000000UL, 0x0000000000181800UL, 0x0000181800000000UL, 0x000000000C181800UL, 
	0x060C183060000000UL, 0x000000006030180CUL, 0x00007E0000000000UL, 0x000000000000007EUL, 0x6030180C06000000UL, 0x00000000060C1830UL, 0x18183063633E0000UL, 0x0000000018180018UL, 
	0x7B7B63633E000000UL, 0x000000003E033B7BUL, 0x7F6363361C080000UL, 0x0000000063636363UL, 0x663E6666663F0000UL, 0x000000003F666666UL, 0x03030343663C0000UL, 0x000000003C664303UL, 
	0x66666666361F0000UL, 0x000000001F366666UL, 0x161E1646667F0000UL, 0x000000007F664606UL, 0x161E1646667F0000UL, 0x000000000F060606UL, 0x7B030343663C0000UL, 0x000000005C666363UL, 
	0x637F636363630000UL, 0x0000000063636363UL, 0x18181818183C0000UL, 0x000000003C181818UL, 0x3030303030780000UL, 0x000000001E333333UL, 0x1E1E366666670000UL, 0x0000000067666636UL, 
	0x06060606060F0000UL, 0x000000007F664606UL, 0xC3DBFFFFE7C30000UL, 0x00000000C3C3C3C3UL, 0x737B7F6F67630000UL, 0x0000000063636363UL, 0x63636363633E0000UL, 0x000000003E636363UL, 
	0x063E6666663F0000UL, 0x000000000F060606UL, 0x63636363633E0000UL, 0x000070303E7B6B63UL, 0x363E6666663F0000UL, 0x0000000067666666UL, 0x301C0663633E0000UL, 0x000000003E636360UL, 
	0x18181899DBFF0000UL, 0x000000003C181818UL, 0x6363636363630000UL, 0x000000003E636363UL, 0xC3C3C3C3C3C30000UL, 0x00000000183C66C3UL, 0xDBC3C3C3C3C30000UL, 0x000000006666FFDBUL, 
	0x18183C66C3C30000UL, 0x00000000C3C3663CUL, 0x183C66C3C3C30000UL, 0x000000003C181818UL, 0x0C183061C3FF0000UL, 0x00000000FFC38306UL, 0x0C0C0C0C0C3C0000UL, 0x000000003C0C0C0CUL, 
	0x1C0E070301000000UL, 0x0000000040607038UL, 0x30303030303C0000UL, 0x000000003C303030UL, 0x0000000063361C08UL, 0x0000000000000000UL, 0x0000000000000000UL, 0x0000FF0000000000UL, 
	0x0000000000180C0CUL, 0x0000000000000000UL, 0x3E301E0000000000UL, 0x000000006E333333UL, 0x66361E0606070000UL, 0x000000003E666666UL, 0x03633E0000000000UL, 0x000000003E630303UL, 
	0x33363C3030380000UL, 0x000000006E333333UL, 0x7F633E0000000000UL, 0x000000003E630303UL, 0x060F0626361C0000UL, 0x000000000F060606UL, 0x33336E0000000000UL, 0x001E33303E333333UL, 
	0x666E360606070000UL, 0x0000000067666666UL, 0x18181C0018180000UL, 0x000000003C181818UL, 0x6060700060600000UL, 0x003C666660606060UL, 0x1E36660606070000UL, 0x000000006766361EUL, 
	0x18181818181C0000UL, 0x000000003C181818UL, 0xDBFF670000000000UL, 0x00000000DBDBDBDBUL, 0x66663B0000000000UL, 0x0000000066666666UL, 0x63633E0000000000UL, 0x000000003E636363UL, 
	0x66663B0000000000UL, 0x000F06063E666666UL, 0x33336E0000000000UL, 0x007830303E333333UL, 0x666E3B0000000000UL, 0x000000000F060606UL, 0x06633E0000000000UL, 0x000000003E63301CUL, 
	0x0C0C3F0C0C080000UL, 0x00000000386C0C0CUL, 0x3333330000000000UL, 0x000000006E333333UL, 0xC3C3C30000000000UL, 0x00000000183C66C3UL, 0xC3C3C30000000000UL, 0x0000000066FFDBDBUL, 
	0x3C66C30000000000UL, 0x00000000C3663C18UL, 0x6363630000000000UL, 0x001F30607E636363UL, 0x18337F0000000000UL, 0x000000007F63060CUL, 0x180E181818700000UL, 0x0000000070181818UL, 
	0x1800181818180000UL, 0x0000000018181818UL, 0x18701818180E0000UL, 0x000000000E181818UL, 0x000000003B6E0000UL, 0x0000000000000000UL, 0x63361C0800000000UL, 0x00000000007F6363UL, 
};
