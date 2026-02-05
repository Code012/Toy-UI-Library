// /*  date = December 25th 2025 08:52 PM */
// main.cpp
#include "context_cracking.h"
#include "main.h"

//////////////////////////////
//- Globals
GlobalState global;

////////////////////////////////////
//- Core UI Logic

void _WindowEndPaint(Window *window, Painter *painter);

void _ElementPaint(Element *element, Painter *painter)
{
	// Compute the intersection of where the element is allowed to draw, element->clip,
	// with the area requested to be drawn, painter->clip
	Rectangle clip = RectangleIntersection(element->clip, painter->clip);

	// If the above regions do not overlap, return here,
	// and do not recurse into our descendant elements
	// (since their clip rectangles are contained within element->clip)
	if (!RectangleValid(clip))
	{
		return;
	}

	// Set the pointer's clip and ask the element to paint itself
	painter->clip = clip;
	ElementMessage(element, MSG_PAINT, 0, painter);

	// Recurse into each child, restoring the clip each time
	for (uintptr_t i = 0; i < element->childCount; i++)
	{
		painter->clip = clip;
		_ElementPaint(element->children[i], painter);
	}
}

void _Update()
{
	for (uintptr_t i = 0; i < global.windowCount; i++)
	{
		Window *window = global.windows[i];

		// Is there anything marked for repaint?
		if (RectangleValid(window->updateRegion))
		{
			// Setup the painter using the window's buffer
			Painter painter;
			painter.bits = window->bits;
			painter.width = window->width;
			painter.height = window->height;
			painter.clip = RectangleIntersection(RectangleMake(0, window->width, 0, window->height), window->updateRegion);

			// Paint everything in the update region
			_ElementPaint(&window->e, &painter);

			// Tell the platform layer to put the result onto the screen
			_WindowEndPaint(window, &painter);

			// Clear the update region, ready for the next input event cycle
			window->updateRegion = RectangleMake(0, 0, 0, 0);
		}
	}
}

// Invariant:  each element is responsible for the positioning of its children and nothing more.
// Moves an element and triggers layout if needed
void ElementMove(Element *element, Rectangle bounds, bool alwaysLayout)
{
	// save the previous visible area to detect changes
	Rectangle oldClip = element->clip;
	// compute the new visible region:
	//	- element's bounds
	// 	- clipped by parent's visible area
	// Enforces clipping automatically
	element->clip = RectangleIntersection(element->parent->clip, bounds);

	// only re-layout if:
	// 	- bounds changed, OR
	// 	- visible region changed, OR
	// 	- caller forces it (alwaysLayout)
	// to prevent unnecessary layout propagration
	if (!RectangleEquals(element->bounds, bounds)
					|| !RectangleEquals(element->clip, oldClip)
					|| alwaysLayout)
	{
		// commit new bounds
		element->bounds = bounds;
		// notify the element: "Your bounds/clip changed; reposition your children"
		ElementMessage(element, MSG_LAYOUT, 0, 0);
	}
}

void ElementRepaint(Element *element, Rectangle *region)
{
	if (!region)
	{
		// If the region to repaint was not specified, use the whole bounds of the element
		region = &element->bounds;
	}

	// Intersect the region to repaint with the element's clip
	Rectangle r = RectangleIntersection(*region, element->clip);

	// if the intersection is non-empty...
	if (RectangleValid(r))
	{
		// Set the window's updateRegion to be the smallest rectangle containing both
		// the previous value of the updateRegion and the new rectangle we need to repaint
		if (RectangleValid(element->window->updateRegion))
		{
			element->window->updateRegion = RectangleBounding(element->window->updateRegion, r);
		}
		else
		{
			element->window->updateRegion = r;
		}
	}
}

// Two-layer message dispatch with user override and class falback
// Dispatch a messeage to an element
// User handler is given first refusal:
// 	- non-zero return means "handled" -> stop dispatch
// 	- zero return means "not handled" -> fall through
// If the user handler declines, the class (default) handler runs
// The return value indicates whether the message was untlimately handled
int ElementMessage(Element *element, Message message, int di, void *dp)
{
	if (element->messageUser)
	{
		int result = element->messageUser(element, message, di, dp);

		if (result)
		{
			return result;
		}
		else
		{
			// keep going!
		}
	}

	if (element->messageClass)
	{
		return element->messageClass(element, message, di, dp);
	}
	else
	{
		return 0;
	}
}


Element *ElementCreate(size_t bytes, Element *parent, uint32_t flags, MessageHandler messageClass)
{
	Element *element = (Element *) calloc(1, bytes);
	element->flags = flags;
	element->messageClass = messageClass;

	if (parent)		// element is not the root
	{
		element->window = parent->window;
		element->parent = parent;
		parent->childCount++;
		parent->children = (Element **)realloc(parent->children, sizeof(Element *) * parent->childCount);
		parent->children[parent->childCount - 1] = element;
	}
	return element;
}


////////////////////////////////////
//- Helpers
Rectangle RectangleMake(int l, int r, int t, int b)
{
	return Rectangle{l, r, t, b};
}

// valid if width and height are positive
bool RectangleValid(Rectangle a)
{
	bool valid = true;

	if ((a.r - a.l) < 0) valid = false;
	if ((a.b - a.t) < 0) valid = false;

	return valid;
}

// Compute the intersection of the rectangles, i.e. the biggest rectangle that fits 
// into both. If the rectangles don't overlap, an invalid rectangle is returned 
// (as per RectangleValid).
Rectangle RectangleIntersection(Rectangle a, Rectangle b)
{
	// where do both rectangles exist at the same time, an AND operation
	if (a.l < b.l) a.l = b.l;
	if (a.t < b.t) a.t = b.t;
	if (a.r > b.r) a.r = b.r;
	if (a.b > b.b) a.b = b.b;

	return a;
}

// Compute the smallest rectangle containing both of the input rectangles.
Rectangle RectangleBounding(Rectangle a, Rectangle b)
{
	// what is the minimum rectangle that encloses both, an OR operation
	if (a.l > b.l) a.l = b.l;
	if (a.t > b.t) a.t = b.t;
	if (a.r < b.r) a.r = b.r;
	if (a.b < b.b) a.b = b.b;

	return a; 
}

// Returns true if all sides are equal.
bool RectangleEquals(Rectangle a, Rectangle b)
{
	bool equal = false;
	if (a.l == b.l && a.t == b.t && a.r == b.r && a.b == b.b)
		equal = true;

	return equal;
}

// Returns true if the pixel with its top-left at the given coordinate is contained 
// inside the rectangle.
bool RectangleContains(Rectangle a, int x, int y)
{
	// (x, y) gives the top-left corner of the pixel. (treating pixel as 1x1 pixel square/rectangle: l,r,t,b [x, x), [y, y) )
	// x ∈ [a.l, a.r)
	// y ∈ [a.t, a.b)

	// Therefore we use strict inequalities when comparing against the right 
	// and bottom sides of the rectangle.
	return a.l <= x && a.r > x && a.t <= y && a.b > y;
}

void StringCopy(char **destination, size_t *destinationBytes, const char *source, ptrdiff_t sourceBytes)
{
	if (sourceBytes == -1) sourceBytes = strlen(source);
	*destination = (char *) realloc(*destination, sourceBytes);
	*destinationBytes = sourceBytes;
	memcpy(*destination, source, sourceBytes);
}

////////////////////////////////////
//- Painting

void DrawBlock(Painter *painter, Rectangle rectangle, uint32_t colour)
{
	// Intersect the rectangle we want to fill with the clip, i.e. the rectangle we're allowed to draw into
	rectangle = RectangleIntersection(painter->clip, rectangle);

	// for every pixel inside the rectangle
	for (int y = rectangle.t; y < rectangle.b; y++)			// row
	{
		for (int x = rectangle.l; x < rectangle.r; x++)		// column
		{
			// Set the pixel to the given colour
			painter->bits[y * painter->width + x] = colour;	// 1-d array as 2-d array y*painter->width computes row, + x computes column
		}
	}

	// Note that the y loop is the outer one, so that memory access to painter->bits is more sequential
	// (i.e. it's slightly faster this way)
}

////////////////////////////////////
//- Platform code

#if OS_WINDOWS
LRESULT CALLBACK _WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	Window *window = (Window *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

	if (!window)
	{
		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	if (message == WM_CLOSE)
	{
		PostQuitMessage(0);
	}
	else if (message == WM_SIZE)
	{
		OutputDebugStringA("WM_SIZE\n");
		RECT client;
		GetClientRect(hwnd, &client);
		window->width = client.right;
		window->height = client.bottom;
		window->bits = (uint32_t *) realloc(window->bits, window->width * window->height * 4);
		window->e.bounds = RectangleMake(0, window->width, 0, window->height);
		window->e.clip = RectangleMake(0, window->width, 0, window->height);
		ElementMessage(&window->e, MSG_LAYOUT, 0, 0);
		_Update();
	}
	else if (message == WM_PAINT)
	{
		PAINTSTRUCT paint;
		HDC dc = BeginPaint(hwnd, &paint);
		BITMAPINFOHEADER info = { 0 };
		info.biSize = sizeof(info);
		info.biWidth = window->width, info.biHeight = -window->height;
		info.biPlanes = 1, info.biBitCount = 32;
		StretchDIBits(dc, 0, 0, window->e.bounds.r - window->e.bounds.l, window->e.bounds.b - window->e.bounds.t, 
				0, 0, window->e.bounds.r - window->e.bounds.l, window->e.bounds.b - window->e.bounds.t,
				window->bits, (BITMAPINFO *) &info, DIB_RGB_COLORS, SRCCOPY);
		EndPaint(hwnd, &paint);
	}
	else
	{
		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	return 0;
}

int _WindowMessage(Element *element, Message message, int di, void *dp)
{
	(void) di;
	(void) dp;

	if (message == MSG_LAYOUT && element->childCount)
	{
		ElementMove(element->children[0], element->bounds, false);
		ElementRepaint(element, NULL);
	}

	return 0;
}

void _WindowEndPaint(Window *window, Painter *painter)
{
	(void) painter;
	HDC dc = GetDC(window->hwnd);
	BITMAPINFOHEADER info = { 0 };
	info.biSize = sizeof(info);
	info.biWidth = window->width, info.biHeight = window->height;
	info.biPlanes = 1, info.biBitCount = 32;
	// Note: biHeight is positive, so the DIB is bottom-up.
	// GDI treats y=0 as the bottom of the bitmap, while our renderer
	// treats y=0 as the top. The unusual ySrc / SrcHeight values
	// compensate for this inverted Y axis.
	StretchDIBits(dc, 
		window->updateRegion.l, window->updateRegion.t, 
		window->updateRegion.r - window->updateRegion.l, window->updateRegion.b - window->updateRegion.t,
		window->updateRegion.l, window->updateRegion.b + 1, 
		window->updateRegion.r - window->updateRegion.l, window->updateRegion.t - window->updateRegion.b,
		window->bits, (BITMAPINFO *) &info, DIB_RGB_COLORS, SRCCOPY);
	ReleaseDC(window->hwnd, dc);
}

Window *WindowCreate(const char *cTitle, int width, int height)
{
	// Window *window = (Window *) calloc(1, sizeof(Window));
	Window *window = (Window *) ElementCreate(sizeof(Window), NULL, 0, _WindowMessage);
	window->e.window = window;
	global.windowCount++;
	global.windows = (Window **)realloc(global.windows, sizeof(Window *) * global.windowCount);
	global.windows[global.windowCount - 1] = window;

	window->hwnd = CreateWindow("UILibraryTutorial", cTitle, WS_OVERLAPPEDWINDOW,
					CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, NULL, NULL);
	SetWindowLongPtr(window->hwnd, GWLP_USERDATA, (LONG_PTR) window);
	ShowWindow(window->hwnd, SW_SHOW);
	PostMessage(window->hwnd, WM_SIZE, 0, 0);
	return window;
}

int MessageLoop()
{
	MSG message = {};

	while (GetMessage(&message, NULL, 0, 0))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);
	}

	return message.wParam;
}

void Initialise()
{
	WNDCLASS windowClass = {};
	windowClass.lpfnWndProc = _WindowProcedure;
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.lpszClassName = "UILibraryTutorial";
	RegisterClass(&windowClass);
}

#endif

#if OS_LINUX

Window *_FindWindow(X11Window window) {
	for (uintptr_t i = 0; i < global.windowCount; i++) {
		if (global.windows[i]->window == window) {
			return global.windows[i];
		}
	}

	return NULL;
}

void _WindowEndPaint(Window *window, Painter *painter) {
	(void) painter;

	XPutImage(global.display, window->window, DefaultGC(global.display, 0), window->image, 
		window->updateRegion.l, window->updateRegion.t, window->updateRegion.l, window->updateRegion.t,
		window->updateRegion.r - window->updateRegion.l, window->updateRegion.b - window->updateRegion.t);
}

int _WindowMessage(Element *element, Message message, int di, void *dp)
{
	(void) di;
	(void) dp;

	if (message == MSG_LAYOUT && element->childCount)
	{
		ElementMove(element->children[0], element->bounds, false);
		ElementRepaint(element, NULL);
	}

	return 0;
}

Window *WindowCreate(const char *cTitle, int width, int height) {
	// Window *window = (Window *) calloc(1, sizeof(Window));
	Window *window = (Window *) ElementCreate(sizeof(Window), NULL, 0, _WindowMessage);
	window->e.window = window;
	global.windowCount++;
	global.windows = realloc(global.windows, sizeof(Window *) * global.windowCount);
	global.windows[global.windowCount - 1] = window;

	XSetWindowAttributes attributes = {};
	window->window = XCreateWindow(global.display, DefaultRootWindow(global.display), 0, 0, width, height, 0, 0, 
		InputOutput, CopyFromParent, CWOverrideRedirect, &attributes);
	XStoreName(global.display, window->window, cTitle);
	XSelectInput(global.display, window->window, SubstructureNotifyMask | ExposureMask | PointerMotionMask 
		| ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask
		| EnterWindowMask | LeaveWindowMask | ButtonMotionMask | KeymapStateMask | FocusChangeMask | PropertyChangeMask);
	XMapRaised(global.display, window->window);
	XSetWMProtocols(global.display, window->window, &global.windowClosedID, 1);
	window->image = XCreateImage(global.display, global.visual, 24, ZPixmap, 0, NULL, 10, 10, 32, 0);
	return window;
}

int MessageLoop() {
	_Update();

	while (true) {
		XEvent event;
		XNextEvent(global.display, &event);

		if (event.type == ClientMessage && (Atom) event.xclient.data.l[0] == global.windowClosedID) {
			return 0;
		} else if (event.type == Expose) {
			Window *window = _FindWindow(event.xexpose.window);
			if (!window) continue;
			XPutImage(global.display, window->window, DefaultGC(global.display, 0), 
					window->image, 0, 0, 0, 0, window->width, window->height);
		}else if (event.type == ConfigureNotify) {
			Window *window = _FindWindow(event.xconfigure.window);
			if (!window) continue;

			if (window->width != event.xconfigure.width || window->height != event.xconfigure.height) {
				window->width = event.xconfigure.width;
				window->height = event.xconfigure.height;
				window->bits = (uint32_t *) realloc(window->bits, window->width * window->height * 4);
				window->image->width = window->width;
				window->image->height = window->height;
				window->image->bytes_per_line = window->width * 4;
				window->image->data = (char *) window->bits;
				window->e.bounds = RectangleMake(0, window->width, 0, window->height);
				window->e.clip = RectangleMake(0, window->width, 0, window->height);
				ElementMessage(&window->e, MSG_LAYOUT, 0, 0);
				_Update();
			}
		}
	}
}

void Initialise() {
	global.display = XOpenDisplay(NULL);
	global.visual = XDefaultVisual(global.display, 0);
	global.windowClosedID = XInternAtom(global.display, "WM_DELETE_WINDOW", 0);
}

#endif

////////////////////////////////////
//- Test Usage Code
#include <stdio.h>

Element *elementA, *elementB, *elementC, *elementD;	// A is pink rect covering entire screen, B is grey rect centred mid, C is blue, D is green

int ElementAMessage(Element *element, Message message, int di, void *dp) {
	(void) di;

	Rectangle bounds = element->bounds;

	if (message == MSG_PAINT) {
		DrawBlock((Painter *) dp, bounds, 0xFF77FF);
	} else if (message == MSG_LAYOUT) {
		fprintf(stderr, "layout A with bounds (%d->%d;%d->%d)\n", bounds.l, bounds.r, bounds.t, bounds.b);
		ElementMove(elementB, RectangleMake(bounds.l + 20, bounds.r - 20, bounds.t + 20, bounds.b - 20), false);
	}

	return 0;
}

int ElementBMessage(Element *element, Message message, int di, void *dp) {
	(void) di;

	Rectangle bounds = element->bounds;

	if (message == MSG_PAINT) {
		DrawBlock((Painter *) dp, bounds, 0xDDDDE0);
	} else if (message == MSG_LAYOUT) {
		fprintf(stderr, "layout B with bounds (%d->%d;%d->%d)\n", bounds.l, bounds.r, bounds.t, bounds.b);
		ElementMove(elementC, RectangleMake(bounds.l - 40, bounds.l + 20, bounds.t + 40, bounds.b - 40), false);
		ElementMove(elementD, RectangleMake(bounds.r - 20, bounds.r + 40, bounds.t + 40, bounds.b - 40), false);
	}

	return 0;
}

int ElementCMessage(Element *element, Message message, int di, void *dp) {
	(void) di;

	Rectangle bounds = element->bounds;

	if (message == MSG_PAINT) {
		DrawBlock((Painter *) dp, bounds, 0x3377FF);
	} else if (message == MSG_LAYOUT) {
		fprintf(stderr, "layout C with bounds (%d->%d;%d->%d)\n", bounds.l, bounds.r, bounds.t, bounds.b);
	}

	return 0;
}

int ElementDMessage(Element *element, Message message, int di, void *dp) {
	(void) di;

	Rectangle bounds = element->bounds;

	if (message == MSG_PAINT) {
		DrawBlock((Painter *) dp, bounds, 0x33CC33);
	} else if (message == MSG_LAYOUT) {
		fprintf(stderr, "layout D with bounds (%d->%d;%d->%d)\n", bounds.l, bounds.r, bounds.t, bounds.b);
	}

	return 0;
}

int main() {
	Initialise();
	Window *window = WindowCreate("Hello, world", 300, 200);
	elementA = ElementCreate(sizeof(Element), &window->e, 0, ElementAMessage);
	elementB = ElementCreate(sizeof(Element), elementA, 0, ElementBMessage);
	elementC = ElementCreate(sizeof(Element), elementB, 0, ElementCMessage);
	elementD = ElementCreate(sizeof(Element), elementB, 0, ElementDMessage);
	return MessageLoop();
}

// Add rendering.

