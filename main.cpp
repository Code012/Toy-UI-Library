/*  date = December 25th 2025 08:52 PM */

#include "context_cracking.h"
#include "main.h"

//////////////////////////////
//- Globals
GlobalState global;

////////////////////////////////////
//- Core UI Logic

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
		RECT client;
		GetClientRect(hwnd, &client);
		window->width = client.right;
		window->height = client.bottom;
	}
	else
	{
		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	return 0;
}

int _WindowMessage(Element *element, Message message, int di, void *dp)
{
	(void) element;
	(void) message;
	(void) di;
	(void) dp;

	return 0;
}

Window *WindowCreate(const char *cTitle, int width, int height)
{
	// Window *window = (Window *) calloc(1, sizeof(Window));
	Window *window = (Window *) ElementCreate(sizeof(Window), NULL, 0, _WindowMessage);
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

int _WindowMessage(Element *element, Message message, int di, void *dp)
{
	(void) element;
	(void) message;
	(void) di;
	(void) dp;

	return 0;
}

Window *WindowCreate(const char *cTitle, int width, int height) {
	// Window *window = (Window *) calloc(1, sizeof(Window));
	Window *window = (Window *) ElementCreate(sizeof(Window), NULL, 0, _WindowMessage);
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
	while (true) {
		XEvent event;
		XNextEvent(global.display, &event);

		if (event.type == ClientMessage && (Atom) event.xclient.data.l[0] == global.windowClosedID) {
			return 0;
		} else if (event.type == ConfigureNotify) {
			Window *window = _FindWindow(event.xconfigure.window);
			if (!window) continue;

			if (window->width != event.xconfigure.width || window->height != event.xconfigure.height) {
				window->width = event.xconfigure.width;
				window->height = event.xconfigure.height;
				window->image->width = window->width;
				window->image->height = window->height;
				window->image->bytes_per_line = window->width * 4;
				window->image->data = (char *) window->bits;
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

Element *elementA, *elementB;

int ElementAMessageClass(Element *element, Message message, int di, void *dp)
{
	(void) element;
	printf("A class: %d, %d, %p\n", message, di, dp);
	return message == MSG_USER + 1;
}

int ElementAMessageUser(Element *element, Message message, int di, void *dp) {
	(void) element;
	printf("A user: %d, %d, %p\n", message, di, dp);
	return message == MSG_USER + 2;
}

int ElementBMessageClass(Element *element, Message message, int di, void *dp) {
	(void) element;
	printf("B class: %d, %d, %p\n", message, di, dp);
	return 0;
}

int main(void)
{
	Initialise();
	Window *window = WindowCreate("Hello, world", 300, 200);
	elementA = ElementCreate(sizeof(Element), &window->e, 0, ElementAMessageClass);
	elementA->messageUser = ElementAMessageUser;
	elementB = ElementCreate(sizeof(Element), elementA, 0, ElementBMessageClass);
	printf("%d\n", ElementMessage(elementA, static_cast<Message>(MSG_USER + 1), 1, NULL));
	printf("%d\n", ElementMessage(elementA, static_cast<Message>(MSG_USER + 2), 2, NULL));
	printf("%d\n", ElementMessage(elementA, static_cast<Message>(MSG_USER + 3), 3, NULL));
	printf("%d\n", ElementMessage(elementB, static_cast<Message>(MSG_USER + 1), 1, NULL));
	printf("%d\n", ElementMessage(elementB, static_cast<Message>(MSG_USER + 2), 2, NULL));
	printf("%d\n", ElementMessage(elementB, static_cast<Message>(MSG_USER + 3), 3, NULL));


	return MessageLoop();	// Pass control over to the platform layer until the application exits.
}