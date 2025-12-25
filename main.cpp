/*  date = December 25th 2025 08:52 PM */

#include "main.h"

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

int main(void)
{
	return 0;
}
