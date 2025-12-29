
## Part 3: Element Heirarchy

Window element has exactly one child, the layout root. It takes up the full bounds of the window. This way, creating a window does not make any assumptions about how the layout will start. 

It is up to the user (of the library) to attach some elements to the window that will provide the layout they want, e.g. the `layout root`

 Window element

 └─ layout root (window->element->children[0])

 	└─ rest of UI


## Part 4: Messaging Infrastructure

Do not use `_WindowMessage` for anything except the layout root element.
Nakst will clarify the concept of "layouts" in later chapters but I've noticed the above.


## Part 5: Messaging Infrastructure
Code flow when a window resize happens

User resizes -> platform layer update windows bounds and clip of the window element -> platform layer sends MSG_LAYOUT to window element -> an element receives MSG_LAYOUT message, it looks at its new bounds field, and updates the bounds and clip fields of all its children -> element sends MSG_LAYOUT mesage to all its children whose fields were modified

##### Step 1: Platform resize event
- Win32:
```cpp
WM_SIZE
```
- Linux
```cpp
ConfigureNotify
```

##### Step 2: Platform updates window element
- Win32
```cpp
window->e.bounds = RectangleMake(0, window->width, 0, window->height);
window->e.clip = RectangleMake(0, window->width, 0, window->height);
```


##### Step 3: Platform sends layout message
```cpp
ElementMessage(&window->e, MSG_LAYOUT, 0, 0);
```
This is the single entry point. Note, the window element's singlular layout root element is sent a MSG_LAYOUT message, not the window element.


##### Step 4: Window element handles MSG_LAYOUT
```cpp
if (message == MSG_LAYOUT && element->childCount)
{
	ElementMove(element->children[0], element->bounds, false);
}
```
Window:
- Does not compute layout itself
- Delegates layout to its single child

##### Step 5: `ElementMove` on root child
```cpp
element->clip = RectangleIntersection(element->parent->clip, bounds);
```
Then:
```cpp
ElementMessage(element, MSG_LAYOUT, 0, 0);
```

##### Step 6: Each child handles MSG_LAYOUT
Each element:
- Reads its own `bounds`
- Positions its children
- Calls `ElementMove` on them

This repeats recursively

##### Step 7: Layout propagation completes
- Enture tree updates
- All bounds and clips are valid
- Rendering and hit-testing are now correct