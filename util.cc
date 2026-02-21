#include "util.hh"

#include <cmath>

void adjust_font_size(VteTerminal* vte, GdkWindow* window, int font_size) {
  if (!font_size) return;

  if (!window) return;

  auto* screen = gtk_widget_get_screen(GTK_WIDGET(vte));
  auto* display = gdk_screen_get_display(screen);
  auto* monitor = gdk_display_get_monitor_at_window(display, window);

  GdkRectangle geometry;
  gdk_monitor_get_geometry(monitor, &geometry);

  static constexpr double mm_per_pt = 25.4 / 72.0;
  const auto height_mm = mm_per_pt * font_size;
  const auto height_px = (double)geometry.height * height_mm /
                         (double)gdk_monitor_get_height_mm(monitor);

  PangoFontDescription* font =
      pango_font_description_copy_static(vte_terminal_get_font(vte));
  pango_font_description_set_absolute_size(font, height_px * PANGO_SCALE);
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
  GtkCssProvider* provider = gtk_css_provider_new();

  gchar* colorstr = gdk_rgba_to_string(rgba);
  char* css = g_strdup_printf("* { background-color: %s; }", colorstr);
  gtk_css_provider_load_from_data(provider, css, -1, nullptr);
  g_free(colorstr);
  g_free(css);

  gtk_style_context_add_provider(gtk_widget_get_style_context(widget),
                                 GTK_STYLE_PROVIDER(provider),
                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);
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
