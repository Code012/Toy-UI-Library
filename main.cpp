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


Element *ElementFindByPoint(Element *element, int x, int y)
{
	// Assumption: Sibling elements cannot overlap (presumably to make this simpler).

	// Check which child contains the point
	for (uintptr_t i{}; i < element->childCount; ++i)
	{
		if (RectangleContains(element->children[i]->clip, x, y))
		{
			// Then find the deepest descendent element that contains the point.
			return ElementFindByPoint(element->children[i], x, y);
		}
	}

	return element;
}

// Sets currently pressed element and last pressed button
// and sends update event to elements just pressed and released
void _WindowSetPressed(Window *window, Element *element /* NULL if the mouse button is not being pressed*/, int button /* the button that went up or down*/)
{
	// NOTE(sb): mouse button up and drag messages are sent to the same message that received the button down message, not the element the mouse is hovering over.
	// NOTE(sb): we will ignore mouse events if anohter mouse button is already down, and the corresponding events will be similarly ignored.

	Element *previous = window->pressed;

	// Set the pressed and pressedButtons fields to the new values.
	window->pressed = element;
	window->pressedButton = button;

	// Send out MSG_UPDATE messages.
	if (previous) ElementMessage(previous, MSG_UPDATE, UPDATE_PRESSED, 0);
	if (element) ElementMessage(element, MSG_UPDATE, UPDATE_PRESSED, 0);
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

void _WindowInputEvent(Window *window, Message message, int di, void *dp)
{

	// Is a mouse button pressed?
	if (window->pressed)
	{
		if (message == MSG_MOUSE_MOVE)
		{
			// Mouse move events become mouse drag messages, sent to the element we pressed the mouse button down over.
			ElementMessage(window->pressed, MSG_MOUSE_DRAG, di, dp);
		}
		else if (message == MSG_LEFT_UP && window->pressedButton == 1)
		{	// If the left mouse button was released, and this was the button that was pressed to begin with..

			if (window->hovered == window->pressed)
			{	// If the mouse cursor is over the pressed element, send it the MSG_CLICKED message.
				ElementMessage(window->pressed, MSG_CLICKED, di, dp);
			}

			// Stop pressing the element.
			ElementMessage(window->pressed, MSG_LEFT_UP, di, dp);
			_WindowSetPressed(window, nullptr, 1);
		}
		else if (message == MSG_MIDDLE_UP && window->pressedButton == 2)
		{	// If the middle mouse button was released, and this was the button that was pressed to begin with,
			// stop pressing the element.
			ElementMessage(window->pressed, MSG_MIDDLE_UP, di, dp);
			_WindowSetPressed(window, nullptr, 2);
		}
		else if (message == MSG_RIGHT_UP && window->pressedButton == 3)
		{	// If the right mouse button was released, and this was the button that was pressed to begin with,
			// stop pressing the element.
			ElementMessage(window->pressed, MSG_RIGHT_DOWN, di, dp);
			_WindowSetPressed(window, nullptr, 3);
		}
	}

	if (window->pressed)
	{	// While a mouse button is held, the hovered element is either the pressed element,
		// or the window element ( at the root of the heirarchy).
		// Other elements are not allowed to be considered hovered until the button is released.
		// Here, we update the hovered fields and send out MSG_UPDATE messages as necessary.
		
		bool inside = RectangleContains(window->pressed->clip, window->cursorX, window->cursorY);

		if (inside && window->hovered == &window->e)	// Cursor re-enters pressed element
		{	// If you were outside before, switch hover back to pressed element
			window->hovered = window->pressed;
			ElementMessage(window->pressed, MSG_UPDATE, UPDATE_HOVERED, dp);
		}
		else if (!inside && window->hovered == window->pressed)		// Cursor leaves pressed element
		{	
			window->hovered = &window->e;
			ElementMessage(window->pressed, MSG_UPDATE, UPDATE_HOVERED, dp);
		}
	}
	else
	{
		// No element is currently pressed.
		// Find the element we're hovering over.
		Element *hovered = ElementFindByPoint(&window->e, window->cursorX, window->cursorY);

		if (message == MSG_MOUSE_MOVE)
		{	// If the mouse was moved, tell the hovered element
			ElementMessage(hovered, MSG_MOUSE_MOVE, di, dp);
		}
		else if (message == MSG_LEFT_DOWN)
		{	// If the left mouse button is pressed, start pressing the hovered element
			_WindowSetPressed(window, hovered, 1);
			ElementMessage(hovered, message, di, dp);
		}
		else if (message == MSG_MIDDLE_DOWN)
		{	// If the middle mouse button is pressed, start pressing the hovered element.
			_WindowSetPressed(window, hovered, 2);
			ElementMessage(hovered, message, di, dp);
		}
		else if (message == MSG_RIGHT_DOWN)
		{	// If the right mouse button is pressed, start pressing the hovered element.
			_WindowSetPressed(window, hovered, 3);
			ElementMessage(hovered, message, di, dp);
		}

		// Update the hovered element if necessary:
		// Send an update event when a new element is hovered over
		// this particular update event is an UPDATE_HOVERED event
		if (hovered != window->hovered)
		{
			Element *previous = window->hovered;
			window->hovered = hovered;
			// update event for elements start/stopped hovering 
			ElementMessage(previous, MSG_UPDATE, UPDATE_HOVERED, 0);
			ElementMessage(window->hovered, MSG_UPDATE, UPDATE_HOVERED, 0);
		}

	}

	// Repaint the marked region of the window.
	_Update();
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
	// (x, y) gives the top-left corner of the pixel. (treating pixel as 1x1 pixel square/rectangle: [x, x+1), [y, y+1) )

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

void DrawRectangle(Painter *painter, Rectangle r, uint32_t mainColour, uint32_t borderColour)
{
	// r = RectangleIntersection(painter->clip, r);
	// Have to pass in clipped rects ourselves I assume

	// Borders
	// Top-border
	DrawBlock(painter, RectangleMake(r.l, r.r, r.t, r.t+1), borderColour);
	// Left-border
	DrawBlock(painter, RectangleMake(r.l, r.l+1, r.t+1, r.b-1), borderColour);	// doesn't draw corners
	// Right-border
	DrawBlock(painter, RectangleMake(r.r-1, r.r, r.t+1, r.b-1), borderColour);	// doesn't draw corners
	// Bottom-border
	DrawBlock(painter, RectangleMake(r.l, r.r, r.b-1, r.b), borderColour);

	// Inner rect
	DrawBlock(painter, RectangleMake(r.l+1, r.r-1, r.t+1, r.b-1), mainColour);
}

void DrawString(Painter *painter, Rectangle bounds, const char *string, size_t bytes, uint32_t colour, bool centerAlign) {
	// Setup the clipping region
	Rectangle oldClip = painter->clip;
	painter->clip = RectangleIntersection(bounds, oldClip);

	// Work out where to start drawing the text within the provided bounds
	int x = bounds.l;
	int y = (bounds.t + bounds.b - GLYPH_HEIGHT) / 2;
	if (centerAlign) { x += (bounds.r - bounds.l - bytes * GLYPH_WIDTH) / 2; }

	// for every character int he string...
	for (uintptr_t i{}; i < bytes; ++i)
	{
		uint8_t c = string[i];
		if (c > 127) c = '?'; // only support ASCII

		// work out where the corresponding glyph is to be drawn.
		Rectangle rectangle = RectangleIntersection(painter->clip, RectangleMake(x, x + 8, y, y + 16));
		uint8_t const* data = (uint8_t const*) _font + c * 16;

		// Blit the glyph bits
		for (int i = rectangle.t; i < rectangle.b; ++i)
		{
			uint32_t* bits = painter->bits + i * painter->width + rectangle.l;
			uint8_t byte = data[i - y];

			for (int j = rectangle.l; j < rectangle.r; ++j)
			{
				if (byte & (1 << (j - x)))
				{
					*bits = colour;
				}

				++bits;
			}
		}

		// Advance to the position of the next glyph.
		x += GLYPH_WIDTH;

	}

	// Restore the old clipping region.
	painter->clip = oldClip;
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
	else if (message == WM_MOUSEMOVE)	// mouse moved inside the window
	{
		if (!window->trackingLeave)
		{
			window->trackingLeave = true;
			TRACKMOUSEEVENT leave{};
			leave.cbSize = sizeof(TRACKMOUSEEVENT);
			leave.dwFlags = TME_LEAVE;
			leave.hwndTrack = hwnd;
			TrackMouseEvent(&leave);
		}

		POINT cursor;
		GetCursorPos(&cursor);
		ScreenToClient(hwnd, &cursor);
		window->cursorX = cursor.x;
		window->cursorY = cursor.y;
		_WindowInputEvent(window, MSG_MOUSE_MOVE, 0, 0);
	}
	else if (message == WM_MOUSELEAVE)	// mouse leaves the window
	{
		window->trackingLeave = false;

		// Only do this when window->pressed is null
		// So that when the mouse is being dragged, the cursor's position is still accurate,
		// regardless of whether the cursor is actually in the window.
		if (!window->pressed)
		{
			window->cursorX = -1;
			window->cursorY = -1;
		}

		_WindowInputEvent(window, MSG_MOUSE_MOVE, 0, 0);
	}
	else if (message == WM_LBUTTONDOWN) 
	{
		SetCapture(hwnd);
		_WindowInputEvent(window, MSG_LEFT_DOWN, 0, 0);
	} 
	else if (message == WM_LBUTTONUP) 
	{
		if (window->pressedButton == 1) ReleaseCapture();
		_WindowInputEvent(window, MSG_LEFT_UP, 0, 0);
	} 
	else if (message == WM_MBUTTONDOWN) 
	{
		SetCapture(hwnd);
		_WindowInputEvent(window, MSG_MIDDLE_DOWN, 0, 0);
	} 
	else if (message == WM_MBUTTONUP) 
	{
		if (window->pressedButton == 2) ReleaseCapture();
		_WindowInputEvent(window, MSG_MIDDLE_UP, 0, 0);
	} 
	else if (message == WM_RBUTTONDOWN) 
	{
		SetCapture(hwnd);
		_WindowInputEvent(window, MSG_RIGHT_DOWN, 0, 0);
	} 
	else if (message == WM_RBUTTONUP) 
	{
		if (window->pressedButton == 3) ReleaseCapture();
		_WindowInputEvent(window, MSG_RIGHT_UP, 0, 0);
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
	// treats y=0 as the top. The unusual ySrc, SrcHeight values
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
	window->hovered = &window->e;
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
	window->hovered = &window->e;
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
		} else if (event.type == ConfigureNotify) {
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
		} else if (event.type == MotionNotify) {
			Window *window = _FindWindow(event.xmotion.window);
			if (!window) continue;
			window->cursorX = event.xmotion.x;
			window->cursorY = event.xmotion.y;
			_WindowInputEvent(window, MSG_MOUSE_MOVE, 0, 0);
		} else if (event.type == LeaveNotify) {
			Window *window = _FindWindow(event.xcrossing.window);
			if (!window) continue;

			if (!window->pressed) {
				window->cursorX = -1;
				window->cursorY = -1;
			}

			_WindowInputEvent(window, MSG_MOUSE_MOVE, 0, 0);
		} else if (event.type == ButtonPress || event.type == ButtonRelease) {
			Window *window = _FindWindow(event.xbutton.window);
			if (!window) continue;
			window->cursorX = event.xbutton.x;
			window->cursorY = event.xbutton.y;

			if (event.xbutton.button >= 1 && event.xbutton.button <= 3) {
				_WindowInputEvent(window, (Message) ((event.type == ButtonPress ? MSG_LEFT_DOWN : MSG_LEFT_UP) 
					+ event.xbutton.button * 2 - 2), 0, 0);
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

Element *parentElement, *childElement;

int ParentElementMessage(Element *element, Message message, int di, void *dp) {
	if (message == MSG_PAINT) {
		DrawBlock((Painter *) dp, element->bounds, 0xFFCCFF);
	} else if (message == MSG_LAYOUT) {
		fprintf(stderr, "layout with bounds (%d->%d;%d->%d)\n", element->bounds.l, element->bounds.r, element->bounds.t, element->bounds.b);
		ElementMove(childElement, RectangleMake(50, 100, 50, 100), false);
	} else if (message == MSG_MOUSE_MOVE) {
		fprintf(stderr, "mouse move at (%d,%d)\n", element->window->cursorX, element->window->cursorY);
	} else if (message == MSG_MOUSE_DRAG) {
		fprintf(stderr, "mouse drag at (%d,%d)\n", element->window->cursorX, element->window->cursorY);
	} else if (message == MSG_UPDATE) {
		fprintf(stderr, "update %d\n", di);
	} else if (message == MSG_LEFT_DOWN) {
		fprintf(stderr, "left down\n");
	} else if (message == MSG_RIGHT_DOWN) {
		fprintf(stderr, "right down\n");
	} else if (message == MSG_MIDDLE_DOWN) {
		fprintf(stderr, "middle down\n");
	} else if (message == MSG_LEFT_UP) {
		fprintf(stderr, "left up\n");
	} else if (message == MSG_RIGHT_UP) {
		fprintf(stderr, "right up\n");
	} else if (message == MSG_MIDDLE_UP) {
		fprintf(stderr, "middle up\n");
	} else if (message == MSG_CLICKED) {
		fprintf(stderr, "clicked\n");
	}

	return 0;
}

int ChildElementMessage(Element *element, Message message, int di, void *dp) {
	(void) di;

	if (message == MSG_PAINT) {
		DrawBlock((Painter *) dp, element->bounds, 0x444444);
	}

	return 0;
}

int main() {
	Initialise();
	Window *window = WindowCreate("Hello, world", 300, 200);
	parentElement = ElementCreate(sizeof(Element), &window->e, 0, ParentElementMessage);
	childElement = ElementCreate(sizeof(Element), parentElement, 0, ChildElementMessage);
	return MessageLoop();
}
