#include "config.hh"

#include <array>
#include <cstring>
#include <string>
#include <utility>

#include "url_regex.hh"
#include "util.hh"

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

namespace {

constexpr const char* kDefaultBrowser = "xdg-open";
constexpr const char* kUrlCursorName = "hand";

constexpr std::array<const char*, 6> kUrlRegexPatterns = {
    REGEX_URL_AS_IS, REGEX_URL_HTTP, REGEX_URL_FILE,
    REGEX_URL_VOIP,  REGEX_EMAIL,    REGEX_NEWS_MAN,
};

constexpr std::array<std::pair<std::string_view, insert_action>,
                     static_cast<size_t>(insert_action::count)>
    kInsertActionNames = {{
        {"zoom-in", insert_action::zoom_in},
        {"zoom-out", insert_action::zoom_out},
        {"zoom-reset", insert_action::zoom_reset},
        {"new-window", insert_action::new_window},
        {"selection-mode", insert_action::selection_mode},
        {"url-hints", insert_action::url_hints},
        {"copy", insert_action::copy},
        {"paste", insert_action::paste},
        {"reload-config", insert_action::reload_config},
        {"reset-and-clear", insert_action::reset_and_clear},
        {"completion", insert_action::completion},
        {"scroll-line-up", insert_action::scroll_line_up},
        {"scroll-line-down", insert_action::scroll_line_down},
        {"scroll-page-up", insert_action::scroll_page_up},
        {"scroll-page-down", insert_action::scroll_page_down},
    }};

constexpr std::array<std::pair<std::string_view, selection_action>,
                     static_cast<size_t>(selection_action::count)>
    kSelectionActionNames = {{
        {"exit-selection", selection_action::exit_selection},
        {"visual-block-mode", selection_action::visual_block_mode},
        {"word-backward-blank", selection_action::word_backward_blank},
        {"word-forward-blank", selection_action::word_forward_blank},
        {"scroll-half-page-up", selection_action::scroll_half_page_up},
        {"scroll-half-page-down", selection_action::scroll_half_page_down},
        {"scroll-full-page-up", selection_action::scroll_full_page_up},
        {"scroll-full-page-down", selection_action::scroll_full_page_down},
        {"word-backward", selection_action::word_backward},
        {"word-forward", selection_action::word_forward},
        {"move-left", selection_action::move_left},
        {"move-down", selection_action::move_down},
        {"move-up", selection_action::move_up},
        {"move-right", selection_action::move_right},
        {"word-forward-end", selection_action::word_forward_end},
        {"word-forward-end-blank", selection_action::word_forward_end_blank},
        {"first-row", selection_action::first_row},
        {"last-row", selection_action::last_row},
        {"top-of-screen", selection_action::top_of_screen},
        {"middle-of-screen", selection_action::middle_of_screen},
        {"bottom-of-screen", selection_action::bottom_of_screen},
        {"visual-mode", selection_action::visual_mode},
        {"visual-line-mode", selection_action::visual_line_mode},
        {"copy", selection_action::copy},
        {"search-forward", selection_action::search_forward},
        {"search-backward", selection_action::search_backward},
        {"find-next", selection_action::find_next},
        {"find-previous", selection_action::find_previous},
        {"url-search-forward", selection_action::url_search_forward},
        {"url-search-backward", selection_action::url_search_backward},
        {"open-url", selection_action::open_url},
        {"url-hints", selection_action::url_hints},
    }};

template <typename T>
std::optional<T> get_config(T (*get)(GKeyFile*, const char*, const char*,
                                     GError**),
                            GKeyFile* config, const char* group,
                            const char* key) {
  GError* error = nullptr;
  T value = get(config, group, key, &error);
  if (error) {
    g_error_free(error);
    return {};
  }
  return value;
}

auto get_config_integer(GKeyFile* config, const char* group, const char* key) {
  return get_config<int>(g_key_file_get_integer, config, group, key);
}

std::optional<std::string> get_config_string(GKeyFile* config,
                                             const char* group,
                                             const char* key) {
  auto raw = get_config<char*>(g_key_file_get_string, config, group, key);
  if (!raw) {
    return {};
  }
  std::string result(*raw);
  g_free(*raw);
  return result;
}

auto get_config_double(GKeyFile* config, const char* group, const char* key) {
  return get_config<double>(g_key_file_get_double, config, group, key);
}

std::optional<GdkRGBA> get_config_color(GKeyFile* config, const char* section,
                                        const char* key) {
  auto s = get_config_string(config, section, key);
  if (!s) {
    return {};
  }

  GdkRGBA color;
  if (gdk_rgba_parse(&color, s->c_str())) {
    return color;
  }

  g_printerr("invalid color string: %s\n", s->c_str());
  return {};
}

// Lazily allocates a fallback RGB pattern only when the config key is absent.
cairo_pattern_t* load_cairo_color(GKeyFile* config, const char* group,
                                  const char* key, double fallback_r,
                                  double fallback_g, double fallback_b) {
  if (auto color = get_config_color(config, group, key)) {
    return cairo_pattern_create_rgba(color->red, color->green, color->blue,
                                     color->alpha);
  }
  return cairo_pattern_create_rgb(fallback_r, fallback_g, fallback_b);
}

// Compile and JIT the static URL match regexes once per process.
const std::array<VteRegex*, kUrlRegexPatterns.size()>& get_url_regexes() {
  static const auto regexes = [] {
    constexpr uint32_t kMatchFlags =
        PCRE2_UTF | PCRE2_NO_UTF_CHECK | PCRE2_UCP | PCRE2_MULTILINE;
    std::array<VteRegex*, kUrlRegexPatterns.size()> compiled{};
    for (size_t i = 0; i < compiled.size(); ++i) {
      VteRegex* regex = vte_regex_new_for_match(
          kUrlRegexPatterns[i],
          static_cast<gssize>(std::strlen(kUrlRegexPatterns[i])), kMatchFlags,
          nullptr);
      if (regex) {
        vte_regex_jit(regex, PCRE2_JIT_COMPLETE, nullptr);
        vte_regex_jit(regex, PCRE2_JIT_PARTIAL_SOFT, nullptr);
      }
      compiled[i] = regex;
    }
    return compiled;
  }();
  return regexes;
}

template <typename Table, typename Parser>
void parse_keybindings(GKeyFile* config, const char* section, Table& table,
                       Parser parse_action) {
  table.fill(std::nullopt);
  if (!g_key_file_has_group(config, section)) {
    return;
  }

  gsize len = 0;
  gchar** keys = g_key_file_get_keys(config, section, &len, nullptr);
  if (!keys) {
    return;
  }

  for (gsize i = 0; i < len; ++i) {
    auto action = parse_action(keys[i]);
    if (!action) {
      g_printerr("unknown keybinding action in [%s]: %s\n", section, keys[i]);
      continue;
    }

    auto val = get_config_string(config, section, keys[i]);
    if (!val) {
      continue;
    }

    guint keyval = 0;
    GdkModifierType mods = static_cast<GdkModifierType>(0);
    gtk_accelerator_parse(val->c_str(), &keyval, &mods);
    if (keyval != 0 || mods != 0) {
      table[static_cast<size_t>(*action)] = {gdk_keyval_to_lower(keyval), mods};
    } else {
      g_printerr("invalid keybinding for %s: %s\n", keys[i], val->c_str());
    }
  }
  g_strfreev(keys);
}

void load_theme(GtkWindow* window, VteTerminal* vte, GKeyFile* config,
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

  hints.reset();

  if (auto s = get_config_string(config, "hints", "font")) {
    hints.font = pango_font_description_from_string(s->c_str());
  }

  hints.fg = load_cairo_color(config, "hints", "foreground", 1, 1, 1);
  hints.bg = load_cairo_color(config, "hints", "background", 0, 0, 0);
  hints.af =
      load_cairo_color(config, "hints", "active_foreground", 0.9, 0.5, 0.5);
  hints.ab = load_cairo_color(config, "hints", "active_background", 0, 0, 0);

  if (auto color = get_config_color(config, "hints", "border")) {
    hints.border = cairo_pattern_create_rgba(color->red, color->green,
                                             color->blue, color->alpha);
  } else {
    hints.border = cairo_pattern_reference(hints.fg);
  }

  hints.padding = get_config_double(config, "hints", "padding").value_or(2.0);
  hints.border_width =
      get_config_double(config, "hints", "border_width").value_or(1.0);
  hints.roundness =
      get_config_double(config, "hints", "roundness").value_or(1.5);
}

void set_config(GtkWindow* window, VteTerminal* vte, GtkWidget* scrollbar,
                GtkWidget* hbox, config_info* info, char** icon,
                bool* show_scrollbar_ptr, GKeyFile* config) {
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

  if (auto s = get_config_string(config, "options", "browser")) {
    info->browser = std::move(*s);
  } else if (const char* env_browser = g_getenv("BROWSER");
             env_browser && *env_browser != '\0') {
    info->browser = env_browser;
  } else {
    info->browser = kDefaultBrowser;
  }

  // Register cached URL regexes once and attach the hand cursor to every tag.
  if (info->clickable_url) {
    if (info->url_tags.empty()) {
      const auto& regexes = get_url_regexes();
      info->url_tags.reserve(regexes.size());
      for (VteRegex* regex : regexes) {
        if (!regex) {
          continue;
        }
        const int tag = vte_terminal_match_add_regex(vte, regex, 0);
        vte_terminal_match_set_cursor_name(vte, tag, kUrlCursorName);
        info->url_tags.push_back(tag);
      }
    }
  } else if (!info->url_tags.empty()) {
    for (int tag : info->url_tags) {
      vte_terminal_match_remove(vte, tag);
    }
    info->url_tags.clear();
  }

  if (auto s = get_config_string(config, "options", "word_char_exceptions")) {
    vte_terminal_set_word_char_exceptions(vte, s->c_str());
  }

  if (auto s = get_config_string(config, "options", "font")) {
    PangoFontDescription* font = pango_font_description_from_string(s->c_str());
    info->font_size = pango_font_description_get_size_is_absolute(font)
                          ? 0
                          : pango_font_description_get_size(font);
    vte_terminal_set_font(vte, font);
    pango_font_description_free(font);
    if (info->dpi_aware) {
      adjust_font_size(vte, gtk_widget_get_window(GTK_WIDGET(vte)),
                       info->font_size);
    }
  }

  if (auto i = get_config_integer(config, "options", "scrollback_lines")) {
    vte_terminal_set_scrollback_lines(vte, *i);
  }

  if (auto i = get_config_integer(config, "options", "completion_limit")) {
    info->completion_limit = *i;
  }

  if (auto s = get_config_string(config, "options", "cursor_blink")) {
    if (!g_ascii_strcasecmp(s->c_str(), "system")) {
      vte_terminal_set_cursor_blink_mode(vte, VTE_CURSOR_BLINK_SYSTEM);
    } else if (!g_ascii_strcasecmp(s->c_str(), "on")) {
      vte_terminal_set_cursor_blink_mode(vte, VTE_CURSOR_BLINK_ON);
    } else if (!g_ascii_strcasecmp(s->c_str(), "off")) {
      vte_terminal_set_cursor_blink_mode(vte, VTE_CURSOR_BLINK_OFF);
    }
  }

  if (auto s = get_config_string(config, "options", "cursor_shape")) {
    if (!g_ascii_strcasecmp(s->c_str(), "block")) {
      vte_terminal_set_cursor_shape(vte, VTE_CURSOR_SHAPE_BLOCK);
    } else if (!g_ascii_strcasecmp(s->c_str(), "ibeam")) {
      vte_terminal_set_cursor_shape(vte, VTE_CURSOR_SHAPE_IBEAM);
    } else if (!g_ascii_strcasecmp(s->c_str(), "underline")) {
      vte_terminal_set_cursor_shape(vte, VTE_CURSOR_SHAPE_UNDERLINE);
    }
  }

  if (icon) {
    if (auto s = get_config_string(config, "options", "icon_name")) {
      *icon = g_strdup(s->c_str());
    }
  }

  if (info->size_hints) {
    set_size_hints(window, vte);
  }

  bool show_scrollbar = false;
  if (auto s = get_config_string(config, "options", "scrollbar")) {
    if (!g_ascii_strcasecmp(s->c_str(), "left")) {
      show_scrollbar = true;
      if (hbox && scrollbar) gtk_box_reorder_child(GTK_BOX(hbox), scrollbar, 0);
    } else if (!g_ascii_strcasecmp(s->c_str(), "right")) {
      show_scrollbar = true;
      if (hbox && scrollbar) {
        gtk_box_reorder_child(GTK_BOX(hbox), scrollbar, -1);
      }
    }
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

  parse_keybindings(config, "keybindings", info->keybindings,
                    parse_insert_action);
  parse_keybindings(config, "selection_keybindings",
                    info->selection_keybindings, parse_selection_action);

  load_theme(window, vte, config, info->hints);
}

}  // namespace

hint_info::hint_info(hint_info&& other) noexcept
    : font(std::exchange(other.font, nullptr)),
      fg(std::exchange(other.fg, nullptr)),
      bg(std::exchange(other.bg, nullptr)),
      af(std::exchange(other.af, nullptr)),
      ab(std::exchange(other.ab, nullptr)),
      border(std::exchange(other.border, nullptr)),
      padding(other.padding),
      border_width(other.border_width),
      roundness(other.roundness) {}

hint_info& hint_info::operator=(hint_info&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  reset();
  font = std::exchange(other.font, nullptr);
  fg = std::exchange(other.fg, nullptr);
  bg = std::exchange(other.bg, nullptr);
  af = std::exchange(other.af, nullptr);
  ab = std::exchange(other.ab, nullptr);
  border = std::exchange(other.border, nullptr);
  padding = other.padding;
  border_width = other.border_width;
  roundness = other.roundness;
  return *this;
}

void hint_info::reset() noexcept {
  if (font) {
    pango_font_description_free(font);
    font = nullptr;
  }
  if (fg) {
    cairo_pattern_destroy(fg);
    fg = nullptr;
  }
  if (bg) {
    cairo_pattern_destroy(bg);
    bg = nullptr;
  }
  if (af) {
    cairo_pattern_destroy(af);
    af = nullptr;
  }
  if (ab) {
    cairo_pattern_destroy(ab);
    ab = nullptr;
  }
  if (border) {
    cairo_pattern_destroy(border);
    border = nullptr;
  }
}

std::optional<insert_action> parse_insert_action(std::string_view name) {
  for (const auto& [action_name, action] : kInsertActionNames) {
    if (action_name == name) {
      return action;
    }
  }
  return {};
}

std::optional<selection_action> parse_selection_action(std::string_view name) {
  for (const auto& [action_name, action] : kSelectionActionNames) {
    if (action_name == name) {
      return action;
    }
  }
  return {};
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
    if (!loaded && error) {
      g_clear_error(&error);
    }
  }

  if (!loaded) {
    for (const char* const* dir = g_get_system_config_dirs(); !loaded && *dir;
         dir++) {
      loaded = g_key_file_load_from_file(config, (*dir + default_path).c_str(),
                                         G_KEY_FILE_NONE, &error);
      if (!loaded && error) {
        g_clear_error(&error);
      }
    }
  }

  if (loaded) {
    set_config(window, vte, scrollbar, hbox, &info, icon, show_scrollbar_ptr,
               config);
  }
  g_key_file_free(config);
  return loaded;
}
