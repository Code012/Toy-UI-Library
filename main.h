/*  date = December 25th 2025 08:54 PM */ 

#include <stddef.h>
#include <cstring>
#include <cstdlib>

////////////////////////////////////
//- Helpers

struct Rectangle
{
	int l, r, t, b;
};

Rectangle RectangleMake(int l, int r, int t, int b);
bool RectangleValid(Rectangle a);							// valid if width and height are positive
Rectangle RectangleIntersection(Rectangle a, Rectangle b); 	// Compute the intersection of the rectangles, i.e. the biggest rectangle that fits into both. If the rectangles don't overlap, an invalid rectangle is returned (as per RectangleValid).
Rectangle RectangleBounding(Rectangle a, Rectangle b); 		// Compute the smallest rectangle containing both of the input rectangles.
bool RectangleEquals(Rectangle a, Rectangle b); 			// Returns true if all sides are equal.
bool RectangleContains(Rectangle a, int x, int y); 			// Returns true if the pixel with its top-left at the given coordinate is contained inside the rectangle.

void StringCopy(char **destination, size_t *destinationBytes, const char *source, ptrdiff_t sourceBytes);

struct GlobalState
{

};

GlobalState global;