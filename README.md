
## Part 3: Element Heirarchy

Window element has exactly one child, the layout root. It takes up the full bounds of the window. This way, creating a window does not make any assumptions about how the layout will start. 

It is up to the user (of the library) to attach some elements to the window that will provide the layout they want, e.g. the `layout root`

```
 Window element
 └─ layout root (window->element->children[0])
 	└─ rest of UI
```

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

## Part 6: Painting

The layout bounds for Element A (the root layout elemnt) are not exactly equal to the window's bounds because Element A is limited to the client rect and the window's bounds is the outer bounds of the entire window

- A is pink rect covering entire screen
- B is grey rect centred mid 
- C is blue 
- D is green

##### Back buffer
- Each window owns a pixel baxk buffer `window->bits` representing the entire client area.
- UI elements render into this buffer
- Pixel `(x, y)` in window space maps directly to: `bits[y * window->width + x]` this is how we can pass the rect coords into stretchdibits directly without issue 

##### Dirty rectangle (`updateRegion`)
- When elements change (layout, content, movement), they call `ElementRepaint`
- This does **not repaint immediately**
- Insteads, the affected rectangle is **unioned** (`RectangleBounding`) into `window->updateRegion`
- `updateRegion` is the smallest rectangle containing all changes for the current update cycle

##### Update phase (`_Update`)
- Called at controlled points (e.g. on Windows after `WM_SIZE`; in later tutorials, after input processing).
- If `updateRegion` is valid:
  1. A `Painter` is set up with `clip = updateRegion`.
  2. The element tree is recursively painted **only within that clip**, so only the pixels inside the dirty rectangle are modified in `window->bits`.
  3. The rendered result is written into the back buffer; all pixels outside `updateRegion` are left untouched.
  4. `_WindowEndPaint` copies **only the dirty rectangle** from the back buffer to the OS window (on Windows this is done via `StretchDIBits`), since updating anything else would be redundant.
- Afterward, `updateRegion` is cleared, ready for the next update cycle.
