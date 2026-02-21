#include "config.hh"

#include <array>
#include <functional>
#include <string>

#include "url_regex.hh"
#include "util.hh"

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

static const char* const url_regex_patterns[] = {
    REGEX_URL_AS_IS, REGEX_URL_HTTP, REGEX_URL_FILE,
    REGEX_URL_VOIP,  REGEX_EMAIL,    REGEX_NEWS_MAN,
};

using namespace std::placeholders;

template <typename T>
std::optional<T> get_config(T (*get)(GKeyFile*, const char*, const char*,
                                     GError**),
                            GKeyFile* config, const char* group,
                            const char* key) {
  GError* error = nullptr;
  std::optional<T> value = get(config, group, key, &error);
  if (error) {
    g_error_free(error);
    return {};
  }
  return value;
}

static auto get_config_integer(GKeyFile* config, const char* group,
                               const char* key) {
  return get_config<int>(g_key_file_get_integer, config, group, key);
}

static auto get_config_string(GKeyFile* config, const char* group,
                              const char* key) {
  return get_config<char*>(g_key_file_get_string, config, group, key);
}

static auto get_config_double(GKeyFile* config, const char* group,
                              const char* key) {
  return get_config<double>(g_key_file_get_double, config, group, key);
}

static std::optional<GdkRGBA> get_config_color(GKeyFile* config,
                                               const char* section,
                                               const char* key) {
  if (auto s = get_config_string(config, section, key)) {
    GdkRGBA color;
    if (gdk_rgba_parse(&color, *s)) {
      g_free(*s);
      return color;
    }
    g_printerr("invalid color string: %s\n", *s);
    g_free(*s);
  }
  return {};
}

static std::optional<cairo_pattern_t*> get_config_cairo_color(GKeyFile* config,
                                                              const char* group,
                                                              const char* key) {
  if (auto color = get_config_color(config, group, key)) {
    return cairo_pattern_create_rgba(color->red, color->green, color->blue,
                                     color->alpha);
  }
  return {};
}

static void load_theme(GtkWindow* window, VteTerminal* vte, GKeyFile* config,
                       hint_info& hints) {
  std::array<GdkRGBA, 256> palette;
  char color_key[] = "color000";

  for (unsigned i = 0; i < palette.size(); i++) {
    snprintf(color_key, sizeof(color_key), "color%u", i);
    if (auto color = get_config_color(config, "colors", color_key)) {
      palette[i] = *color;
    } else if (i < 16) {
      palette[i].blue =
          (((i & 4) ? 0xc000 : 0) + (i > 7 ? 0x3fff : 0)) / 65535.0;
      palette[i].green =
          (((i & 2) ? 0xc000 : 0) + (i > 7 ? 0x3fff : 0)) / 65535.0;
      palette[i].red =
          (((i & 1) ? 0xc000 : 0) + (i > 7 ? 0x3fff : 0)) / 65535.0;
      palette[i].alpha = 0;
    } else if (i < 232) {
      const unsigned j = i - 16;
      const unsigned r = j / 36, g = (j / 6) % 6, b = j % 6;
      const unsigned red = (r == 0) ? 0 : r * 40 + 55;
      const unsigned green = (g == 0) ? 0 : g * 40 + 55;
      const unsigned blue = (b == 0) ? 0 : b * 40 + 55;
      palette[i].red = (red | red << 8) / 65535.0;
      palette[i].green = (green | green << 8) / 65535.0;
      palette[i].blue = (blue | blue << 8) / 65535.0;
      palette[i].alpha = 0;
    } else if (i < 256) {
      const unsigned shade = 8 + (i - 232) * 10;
      palette[i].red = palette[i].green = palette[i].blue =
          (shade | shade << 8) / 65535.0;
      palette[i].alpha = 0;
    }
  }

  vte_terminal_set_colors(vte, nullptr, nullptr, palette.data(),
                          palette.size());
  if (auto color = get_config_color(config, "colors", "foreground")) {
    vte_terminal_set_color_foreground(vte, &*color);
    vte_terminal_set_color_bold(vte, &*color);
  }
  if (auto color = get_config_color(config, "colors", "foreground_bold")) {
    vte_terminal_set_color_bold(vte, &*color);
  }
  if (auto color = get_config_color(config, "colors", "background")) {
    vte_terminal_set_color_background(vte, &*color);
    override_background_color(GTK_WIDGET(window), &*color);
  }
  if (auto color = get_config_color(config, "colors", "cursor")) {
    vte_terminal_set_color_cursor(vte, &*color);
  }
  if (auto color = get_config_color(config, "colors", "cursor_foreground")) {
    vte_terminal_set_color_cursor_foreground(vte, &*color);
  }
  if (auto color = get_config_color(config, "colors", "highlight")) {
    vte_terminal_set_color_highlight(vte, &*color);
  }

  if (auto s = get_config_string(config, "hints", "font")) {
    hints.font = pango_font_description_from_string(*s);
    g_free(*s);
  }

  hints.fg = get_config_cairo_color(config, "hints", "foreground")
                 .value_or(cairo_pattern_create_rgb(1, 1, 1));
  hints.bg = get_config_cairo_color(config, "hints", "background")
                 .value_or(cairo_pattern_create_rgb(0, 0, 0));
  hints.af = get_config_cairo_color(config, "hints", "active_foreground")
                 .value_or(cairo_pattern_create_rgb(0.9, 0.5, 0.5));
  hints.ab = get_config_cairo_color(config, "hints", "active_background")
                 .value_or(cairo_pattern_create_rgb(0, 0, 0));
  hints.border =
      get_config_cairo_color(config, "hints", "border").value_or(hints.fg);
  hints.padding = get_config_double(config, "hints", "padding").value_or(2.0);
  hints.border_width =
      get_config_double(config, "hints", "border_width").value_or(1.0);
  hints.roundness =
      get_config_double(config, "hints", "roundness").value_or(1.5);
}

static void set_config(GtkWindow* window, VteTerminal* vte,
                       GtkWidget* scrollbar, GtkWidget* hbox, config_info* info,
                       char** icon, bool* show_scrollbar_ptr,
                       GKeyFile* config) {
  auto cfg_bool = [config](const char* key, gboolean value) {
    return get_config<gboolean>(g_key_file_get_boolean, config, "options", key)
        .value_or(value);
  };

  vte_terminal_set_scroll_on_output(vte, cfg_bool("scroll_on_output", FALSE));
  vte_terminal_set_scroll_on_keystroke(vte,
                                       cfg_bool("scroll_on_keystroke", TRUE));
  vte_terminal_set_audible_bell(vte, cfg_bool("audible_bell", FALSE));
  vte_terminal_set_mouse_autohide(vte, cfg_bool("mouse_autohide", FALSE));
  g_object_set(vte, "allow-bold", cfg_bool("allow_bold", TRUE), nullptr);
  vte_terminal_search_set_wrap_around(vte, cfg_bool("search_wrap", TRUE));
  vte_terminal_set_allow_hyperlink(vte, cfg_bool("hyperlinks", FALSE));
  vte_terminal_set_bold_is_bright(vte, cfg_bool("bold_is_bright", TRUE));
  vte_terminal_set_cell_height_scale(
      vte,
      get_config_double(config, "options", "cell_height_scale").value_or(1.0));
  vte_terminal_set_cell_width_scale(
      vte,
      get_config_double(config, "options", "cell_width_scale").value_or(1.0));
  vte_terminal_set_enable_bidi(vte, cfg_bool("bidi", FALSE));
  vte_terminal_set_enable_shaping(vte, cfg_bool("arabic_shaping", FALSE));
  info->dynamic_title = cfg_bool("dynamic_title", TRUE);
  info->urgent_on_bell = cfg_bool("urgent_on_bell", TRUE);
  info->clickable_url = cfg_bool("clickable_url", TRUE);
  info->clickable_url_ctrl = cfg_bool("clickable_url_ctrl", FALSE);
  info->size_hints = cfg_bool("size_hints", FALSE);
  info->filter_unmatched_urls = cfg_bool("filter_unmatched_urls", TRUE);
  info->modify_other_keys = cfg_bool("modify_other_keys", FALSE);
  info->fullscreen = cfg_bool("fullscreen", TRUE);
  info->smart_copy = cfg_bool("smart_copy", FALSE);
  info->dpi_aware = cfg_bool("dpi_aware", FALSE);
  info->copy_mouse_selection = cfg_bool("copy_mouse_selection", FALSE);
  info->font_scale = vte_terminal_get_font_scale(vte);

  g_free(info->browser);
  info->browser = nullptr;

  if (auto s = get_config_string(config, "options", "browser")) {
    info->browser = *s;
  } else {
    info->browser = g_strdup(g_getenv("BROWSER"));
  }

  if (!info->browser) {
    info->browser = g_strdup("xdg-open");
  }

  if (info->clickable_url) {
    for (size_t i = 0; i < G_N_ELEMENTS(url_regex_patterns); ++i) {
      VteRegex* regex = vte_regex_new_for_match(
          url_regex_patterns[i], (gssize)strlen(url_regex_patterns[i]),
          PCRE2_UTF | PCRE2_NO_UTF_CHECK | PCRE2_UCP | PCRE2_MULTILINE,
          nullptr);
      vte_regex_jit(regex, PCRE2_JIT_COMPLETE, nullptr);
      vte_regex_jit(regex, PCRE2_JIT_PARTIAL_SOFT, nullptr);
      info->tag = vte_terminal_match_add_regex(vte, regex, 0);
      vte_regex_unref(regex);
    }
    vte_terminal_match_set_cursor_name(vte, info->tag, "hand");
  } else if (info->tag != -1) {
    vte_terminal_match_remove(vte, info->tag);
    info->tag = -1;
  }

  if (auto s = get_config_string(config, "options", "word_char_exceptions")) {
    vte_terminal_set_word_char_exceptions(vte, *s);
  }

  if (auto s = get_config_string(config, "options", "font")) {
    PangoFontDescription* font = pango_font_description_from_string(*s);
    info->font_size = pango_font_description_get_size_is_absolute(font)
                          ? 0
                          : pango_font_description_get_size(font);
    vte_terminal_set_font(vte, font);
    pango_font_description_free(font);
    if (info->dpi_aware)
      adjust_font_size(vte, gtk_widget_get_window(GTK_WIDGET(vte)),
                       info->font_size);
    g_free(*s);
  }

  if (auto i = get_config_integer(config, "options", "scrollback_lines")) {
    vte_terminal_set_scrollback_lines(vte, *i);
  }

  if (auto i = get_config_integer(config, "options", "completion_limit")) {
    info->completion_limit = *i;
  }

  if (auto s = get_config_string(config, "options", "cursor_blink")) {
    if (!g_ascii_strcasecmp(*s, "system")) {
      vte_terminal_set_cursor_blink_mode(vte, VTE_CURSOR_BLINK_SYSTEM);
    } else if (!g_ascii_strcasecmp(*s, "on")) {
      vte_terminal_set_cursor_blink_mode(vte, VTE_CURSOR_BLINK_ON);
    } else if (!g_ascii_strcasecmp(*s, "off")) {
      vte_terminal_set_cursor_blink_mode(vte, VTE_CURSOR_BLINK_OFF);
    }
    g_free(*s);
  }

  if (auto s = get_config_string(config, "options", "cursor_shape")) {
    if (!g_ascii_strcasecmp(*s, "block")) {
      vte_terminal_set_cursor_shape(vte, VTE_CURSOR_SHAPE_BLOCK);
    } else if (!g_ascii_strcasecmp(*s, "ibeam")) {
      vte_terminal_set_cursor_shape(vte, VTE_CURSOR_SHAPE_IBEAM);
    } else if (!g_ascii_strcasecmp(*s, "underline")) {
      vte_terminal_set_cursor_shape(vte, VTE_CURSOR_SHAPE_UNDERLINE);
    }
    g_free(*s);
  }

  if (icon) {
    if (auto s = get_config_string(config, "options", "icon_name")) {
      *icon = *s;
    }
  }

  if (info->size_hints) {
    set_size_hints(window, vte);
  }

  bool show_scrollbar = false;
  if (auto s = get_config_string(config, "options", "scrollbar")) {
    if (!g_ascii_strcasecmp(*s, "left")) {
      show_scrollbar = true;
      if (hbox && scrollbar) gtk_box_reorder_child(GTK_BOX(hbox), scrollbar, 0);
    } else if (!g_ascii_strcasecmp(*s, "right")) {
      show_scrollbar = true;
      if (hbox && scrollbar)
        gtk_box_reorder_child(GTK_BOX(hbox), scrollbar, -1);
    }
    g_free(*s);
  }
  if (scrollbar) {
    if (show_scrollbar) {
      gtk_widget_show(scrollbar);
    } else {
      gtk_widget_hide(scrollbar);
    }
  }
  if (show_scrollbar_ptr != nullptr) {
    *show_scrollbar_ptr = show_scrollbar;
  }

  g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme",
               cfg_bool("gtk_dark_theme", FALSE), nullptr);

  load_theme(window, vte, config, info->hints);
}

bool load_config(config_info& info, VteTerminal* vte, GtkWindow* window,
                 GtkWidget* scrollbar, GtkWidget* hbox, char** icon,
                 bool* show_scrollbar_ptr) {
  const std::string default_path = "/termite/config";
  GKeyFile* config = g_key_file_new();
  GError* error = nullptr;

  gboolean loaded = FALSE;

  if (info.config_file) {
    loaded = g_key_file_load_from_file(config, info.config_file,
                                       G_KEY_FILE_NONE, &error);
    if (!loaded) {
      g_printerr("%s parsing failed: %s\n", info.config_file, error->message);
      g_clear_error(&error);
    }
  }

  if (!loaded) {
    loaded = g_key_file_load_from_file(
        config, (g_get_user_config_dir() + default_path).c_str(),
        G_KEY_FILE_NONE, &error);
    if (!loaded) {
      if (error) g_clear_error(&error);
    }
  }

  if (!loaded) {
    for (const char* const* dir = g_get_system_config_dirs(); !loaded && *dir;
         dir++) {
      loaded = g_key_file_load_from_file(config, (*dir + default_path).c_str(),
                                         G_KEY_FILE_NONE, &error);
      if (!loaded && error) g_clear_error(&error);
    }
  }

  if (loaded) {
    set_config(window, vte, scrollbar, hbox, &info, icon, show_scrollbar_ptr,
               config);
  }
  g_key_file_free(config);
  return loaded;
}
