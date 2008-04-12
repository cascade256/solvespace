
#ifndef __UI_H
#define __UI_H

class TextWindow {
public:
    static const int MAX_COLS = 150;
    static const int MAX_ROWS = 300;

#ifndef RGB
#define RGB(r, g, b) ((r) | ((g) << 8) | ((b) << 16))
#endif
    static const int COLOR_BG_DEFAULT       = RGB( 15,  15,   0);

    typedef struct {
        int     fg;
        int     bg;
    } Color;
    static const Color colors[];
    static const int COLOR_DEFAULT          = 0;
    static const int COLOR_MEANS_HIDDEN     = 1;
    static const int COLOR_MEANS_SHOWN      = 2;
    static const int COLOR_MEANS_MIXED      = 3;

    // The rest of the window, text displayed in response to typed commands;
    // some of this might do something if you click on it.

    static const int NOT_A_LINK = 0;

    BYTE    text[MAX_ROWS][MAX_COLS];
    typedef void LinkFunction(int link, DWORD v);
    struct {
        int             color;
        int             link;
        DWORD           data;
        LinkFunction   *f;
    }       meta[MAX_ROWS][MAX_COLS];

    int rows;
   
    void Init(void);
    void Printf(char *fmt, ...);
    void ClearScreen(void);
    
    void Show(void);

    // State for the screen that we are showing in the text window.
    static const int SCREEN_GROUP_LIST      = 0;
    static const int SCREEN_REQUEST_LIST    = 1;
    typedef struct {
        int     screen;
        hGroup  group;
    } ShownState;
    static const int HISTORY_LEN = 16;
    ShownState showns[HISTORY_LEN];
    int shownIndex;
    int history;
    ShownState *shown;

    void ShowHeader(void);
    // These are self-contained screens, that show some information about
    // the sketch.
    void ShowGroupList(void);
    void ShowRequestList(void);
    void OneScreenForward(void);
    static void ScreenSelectGroup(int link, DWORD v);
    static void ScreenNavigaton(int link, DWORD v);
};

class GraphicsWindow {
public:
    // This table describes the top-level menus in the graphics winodw.
    typedef void MenuHandler(int id);
    typedef struct {
        int         level;          // 0 == on menu bar, 1 == one level down
        char       *label;          // or NULL for a separator
        int         id;             // unique ID
        MenuHandler *fn;
    } MenuEntry;
    static const MenuEntry menu[];

    void Init(void);

    // The width and height (in pixels) of the window.
    double width, height;
    // These parameters define the map from 2d screen coordinates to the
    // coordinates of the 3d sketch points. We will use an axonometric
    // projection.
    Vector  offset;
    Vector  projRight;
    Vector  projDown;
    double  scale;
    struct {
        Vector  offset;
        Vector  projRight;
        Vector  projDown;
        Point2d mouse;
    }       orig;

    void NormalizeProjectionVectors(void);
    Point2d ProjectPoint(Vector p);
    
    // The current selection.
    class Selection {
    public:
        hPoint      point;
        hEntity     entity;

        void Draw(void);

        void Clear(void);
        bool IsEmpty(void);
        bool Equals(Selection *b);
    };
    Selection hover;
    static const int MAX_SELECTED = 32;
    Selection selection[MAX_SELECTED];
    void HitTestMakeSelection(Point2d mp, Selection *dest);

    // This sets what gets displayed.
    bool    show2dCsyss;
    bool    showAxes;
    bool    showPoints;
    bool    showAllGroups;
    bool    showConstraints;
    static void ToggleBool(int link, DWORD v);
    static void ToggleAnyDatumShown(int link, DWORD v);

    // These are called by the platform-specific code.
    void Paint(int w, int h);
    void MouseMoved(double x, double y, bool leftDown, bool middleDown,
                                bool rightDown, bool shiftDown, bool ctrlDown);
    void MouseLeftDown(double x, double y);
    void MouseLeftDoubleClick(double x, double y);
    void MouseMiddleDown(double x, double y);
    void MouseScroll(double x, double y, int delta);
};


#endif
