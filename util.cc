#include "util.hh"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {

constexpr const char* kBgCssProviderKey = "termite-bg-css-provider";
constexpr std::string_view kFileUriPrefix = "file://";
constexpr const char* kWordCharAsciiPunct = "-,./?%&#_=+@~";
constexpr double kMmPerPt = 25.4 / 72.0;

}  // namespace

void adjust_font_size(VteTerminal* vte, GdkWindow* window, int font_size) {
  if (!font_size || !window) {
    return;
  }

  auto* screen = gtk_widget_get_screen(GTK_WIDGET(vte));
  auto* display = gdk_screen_get_display(screen);
  auto* monitor = gdk_display_get_monitor_at_window(display, window);
  if (!monitor) {
    return;
  }

  const int monitor_height_mm = gdk_monitor_get_height_mm(monitor);
  if (monitor_height_mm <= 0) {
    return;
  }

  GdkRectangle geometry;
  gdk_monitor_get_geometry(monitor, &geometry);

  const double height_mm = kMmPerPt * font_size;
  const double height_px = static_cast<double>(geometry.height) * height_mm /
                           static_cast<double>(monitor_height_mm);
  const int target_size =
      static_cast<int>(std::lround(height_px * PANGO_SCALE));

  // Skip redundant VTE font reloads when monitor DPI has not changed.
  const PangoFontDescription* current_font = vte_terminal_get_font(vte);
  if (current_font &&
      pango_font_description_get_size_is_absolute(current_font) &&
      pango_font_description_get_size(current_font) == target_size) {
    return;
  }

  PangoFontDescription* font = pango_font_description_copy_static(current_font);
  pango_font_description_set_absolute_size(font, target_size);
  vte_terminal_set_font(vte, font);
  pango_font_description_free(font);
}

void set_size_hints(GtkWindow* window, VteTerminal* vte) {
  static const GdkWindowHints wh =
      (GdkWindowHints)(GDK_HINT_RESIZE_INC | GDK_HINT_MIN_SIZE |
                       GDK_HINT_BASE_SIZE);
  const int char_width = (int)vte_terminal_get_char_width(vte);
  const int char_height = (int)vte_terminal_get_char_height(vte);
  int padding_left, padding_top, padding_right, padding_bottom;
  get_vte_padding(vte, &padding_left, &padding_top, &padding_right,
                  &padding_bottom);

  GdkGeometry hints;
  hints.base_width = char_width + padding_left + padding_right;
  hints.base_height = char_height + padding_top + padding_bottom;
  hints.min_width = hints.base_width;
  hints.min_height = hints.base_height;
  hints.width_inc = char_width;
  hints.height_inc = char_height;

  gtk_window_set_geometry_hints(window, NULL, &hints, wh);
}

void get_vte_padding(VteTerminal* vte, int* left, int* top, int* right,
                     int* bottom) {
  GtkBorder border;
  gtk_style_context_get_padding(gtk_widget_get_style_context(GTK_WIDGET(vte)),
                                gtk_widget_get_state_flags(GTK_WIDGET(vte)),
                                &border);
  *left = border.left;
  *right = border.right;
  *top = border.top;
  *bottom = border.bottom;
}

void override_background_color(GtkWidget* widget, const GdkRGBA* rgba) {
  auto* provider = static_cast<GtkCssProvider*>(
      g_object_get_data(G_OBJECT(widget), kBgCssProviderKey));

  // Reuse a single provider per widget so config reloads do not leak providers.
  if (!provider) {
    provider = gtk_css_provider_new();
    gtk_style_context_add_provider(gtk_widget_get_style_context(widget),
                                   GTK_STYLE_PROVIDER(provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_set_data_full(G_OBJECT(widget), kBgCssProviderKey, provider,
                           g_object_unref);
  }

  gchar* colorstr = gdk_rgba_to_string(rgba);
  char* css = g_strdup_printf("* { background-color: %s; }", colorstr);
  gtk_css_provider_load_from_data(provider, css, -1, nullptr);
  g_free(colorstr);
  g_free(css);
}

char* check_match(VteTerminal* vte, GdkEventButton* event) {
  int tag;
  return vte_terminal_match_check_event(vte, (GdkEvent*)event, &tag);
}

std::unique_ptr<char, decltype(&g_free)> get_text_range(VteTerminal* vte,
                                                        long start_row,
                                                        long start_col,
                                                        long end_row,
                                                        long end_col) {
  return {
      vte_terminal_get_text_range_format(vte, VTE_FORMAT_TEXT, start_row,
                                         start_col, end_row, end_col, nullptr),
      g_free};
}

bool is_word_char(gunichar c) {
  return g_unichar_isgraph(c) &&
         (g_unichar_isalnum(c) ||
          (g_unichar_ispunct(c) &&
           (c >= 0x80 ||
            strchr(kWordCharAsciiPunct, static_cast<int>(c)) != nullptr)));
}

long find_backward_word_col(const gunichar* codepoints, long length, long count,
                            char_predicate is_word) {
  if (!codepoints || length <= 0) {
    return 0;
  }

  bool in_word = false;
  long i = length - 1;
  for (; i > 0; --i) {
    if (!is_word(codepoints[i - 1])) {
      if (in_word) {
        if (--count == 0) {
          break;
        }
        in_word = false;
      }
    } else {
      in_word = true;
    }
  }
  return i;
}

long find_forward_word_delta(const gunichar* codepoints, long length,
                             long count, bool goto_word_end,
                             char_predicate is_word) {
  if (!codepoints || length <= 0) {
    return 0;
  }

  // Prevent going past the end (get_text_range appends a trailing '\n').
  if (codepoints[length - 1] == '\n') {
    --length;
  }

  long delta = 0;
  if (!goto_word_end) {
    bool end_of_word = false;
    for (long i = 1; i < length; ++i) {
      if (is_word(codepoints[i - 1])) {
        if (end_of_word) {
          if (--count == 0) {
            break;
          }
          end_of_word = false;
        }
      } else {
        end_of_word = true;
      }
      ++delta;
    }
    return delta;
  }

  for (long i = 2; i <= length; ++i) {
    ++delta;
    const bool at_word_end =
        is_word(codepoints[i - 1]) && (i == length || !is_word(codepoints[i]));
    if (at_word_end && --count == 0) {
      break;
    }
  }
  return delta;
}

long find_first_match_col(long start_col, const gunichar* codepoints,
                          long length, char_predicate is_match) {
  if (!codepoints || length <= 0) {
    return -1;
  }

  auto iter = std::find_if(codepoints, codepoints + length, is_match);
  if (iter != codepoints + length) {
    return start_col + (iter - codepoints);
  }
  return -1;
}

bool should_launch_url_hint(size_t url_count, const char* input) {
  if (url_count == 0 || !input || *input == '\0') {
    return false;
  }

  char* end = nullptr;
  errno = 0;
  const unsigned long value = std::strtoul(input, &end, 10);
  if (errno != 0 || end == input || *end != '\0' || value == 0) {
    return false;
  }

  // Launch immediately once no further digit can yield an index <= url_count.
  return value > url_count / 10;
}

long utf8_byte_offset_to_column(std::string_view line, size_t byte_offset) {
  const size_t limit = std::min(byte_offset, line.size());
  const char* ptr = line.data();
  const char* const end = ptr + limit;

  long col = 0;
  while (ptr < end) {
    const gunichar ch = g_utf8_get_char_validated(ptr, end - ptr);
    if (ch == static_cast<gunichar>(-1) || ch == static_cast<gunichar>(-2)) {
      ++ptr;
      ++col;
      continue;
    }

    if (g_unichar_iswide(ch)) {
      col += 2;
    } else if (!g_unichar_iszerowidth(ch)) {
      col += 1;
    }
    ptr = g_utf8_next_char(ptr);
  }
  return col;
}

std::string normalize_dropped_text(const char* raw) {
  if (!raw) {
    return {};
  }

  std::string selection(raw);
  selection.erase(std::remove_if(selection.begin(), selection.end(),
                                 [](char c) { return c == '\r' || c == '\n'; }),
                  selection.end());

  if (selection.starts_with(kFileUriPrefix)) {
    selection.erase(0, kFileUriPrefix.size());
    auto unescaped = make_unique_ptr(
        g_uri_unescape_string(selection.c_str(), nullptr), g_free);
    if (unescaped) {
      selection.assign(unescaped.get());
    }
  }
  return selection;
}

std::vector<const char*> extract_completion_tokens(char* buffer) {
  std::vector<const char*> tokens;
  if (!buffer) {
    return tokens;
  }

  // Split in-place on whitespace so every token is NUL-terminated with zero
  // per-token heap allocations.
  char* p = buffer;
  while (*p != '\0') {
    while (*p == ' ' || *p == '\n' || *p == '\t') {
      ++p;
    }
    if (*p == '\0') {
      break;
    }

    tokens.push_back(p);
    while (*p != '\0' && *p != ' ' && *p != '\n' && *p != '\t') {
      ++p;
    }
    if (*p != '\0') {
      *p = '\0';
      ++p;
    }
  }

  std::sort(tokens.begin(), tokens.end(),
            [](const char* a, const char* b) { return std::strcmp(a, b) < 0; });
  tokens.erase(std::unique(tokens.begin(), tokens.end(),
                           [](const char* a, const char* b) {
                             return std::strcmp(a, b) == 0;
                           }),
               tokens.end());
  return tokens;
}
