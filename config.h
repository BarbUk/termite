#define CLICKABLE_URL

static const char *url_regex = "(ftp|http)s?://[-a-zA-Z0-9.?$%&/=_~#.,:;+]*";

#ifdef CLICKABLE_URL
#define URL_COMMAND(URL_MATCH) {"/usr/bin/firefox", URL_MATCH, NULL}
#endif

// 0.0: opaque, 1.0: transparent
//#define TRANSPARENCY 0.2

static const char *foreground_color = "#dcdccc";
static const char *background_color = "#3f3f3f";
static const char *cursor_color = "#dcdccc";

static const char *colors[16] = {
    "#3f3f3f", // black
    "#705050", // red
    "#60b48a", // green
    "#dfaf8f", // yellow
    "#506070", // blue
    "#dc8cc3", // magenta
    "#8cd0d3", // cyan
    "#dcdccc", // white
    "#709080", // bright black
    "#dca3a3", // bright red
    "#c3bf9f", // bright green
    "#f0dfaf", // bright yellow
    "#94bff3", // bright blue
    "#ec93d3", // bright magenta
    "#93e0e3", // bright cyan
    "#ffffff", // bright white
};

static const char *term = "vte-256color";

// keybindings
#define KEY_COPY    c
#define KEY_PASTE   v
#define KEY_PREV    p
#define KEY_NEXT    n
#define KEY_SEARCH  f
#define KEY_RSEARCH r
#define KEY_URL     j
#define KEY_RURL    k
