#pragma once

#include <gtk/gtk.h>
#include <vte/vte.h>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class insert_action : size_t {
  zoom_in,
  zoom_out,
  zoom_reset,
  new_window,
  selection_mode,
  url_hints,
  copy,
  paste,
  reload_config,
  reset_and_clear,
  completion,
  scroll_line_up,
  scroll_line_down,
  scroll_page_up,
  scroll_page_down,
  count
};

enum class selection_action : size_t {
  exit_selection,
  visual_block_mode,
  word_backward_blank,
  word_forward_blank,
  scroll_half_page_up,
  scroll_half_page_down,
  scroll_full_page_up,
  scroll_full_page_down,
  word_backward,
  word_forward,
  move_left,
  move_down,
  move_up,
  move_right,
  word_forward_end,
  word_forward_end_blank,
  first_row,
  last_row,
  top_of_screen,
  middle_of_screen,
  bottom_of_screen,
  visual_mode,
  visual_line_mode,
  copy,
  search_forward,
  search_backward,
  find_next,
  find_previous,
  url_search_forward,
  url_search_backward,
  open_url,
  url_hints,
  count
};

struct keybinding {
  guint keyval{0};
  GdkModifierType modifiers{static_cast<GdkModifierType>(0)};

  bool operator==(const GdkEventKey& event) const {
    return keyval == gdk_keyval_to_lower(event.keyval) &&
           modifiers == (event.state & gtk_accelerator_get_default_mod_mask());
  }
};

struct hint_info {
  PangoFontDescription* font{nullptr};
  cairo_pattern_t* fg{nullptr};
  cairo_pattern_t* bg{nullptr};
  cairo_pattern_t* af{nullptr};
  cairo_pattern_t* ab{nullptr};
  cairo_pattern_t* border{nullptr};
  double padding{0.0};
  double border_width{0.0};
  double roundness{0.0};

  hint_info() = default;
  ~hint_info() { reset(); }

  hint_info(const hint_info&) = delete;
  hint_info& operator=(const hint_info&) = delete;

  hint_info(hint_info&& other) noexcept;
  hint_info& operator=(hint_info&& other) noexcept;

  void reset() noexcept;
};

using insert_keybinding_table =
    std::array<std::optional<keybinding>,
               static_cast<size_t>(insert_action::count)>;
using selection_keybinding_table =
    std::array<std::optional<keybinding>,
               static_cast<size_t>(selection_action::count)>;

struct config_info {
  char* config_file{nullptr};
  std::string browser{};
  hint_info hints{};
  gboolean dynamic_title{FALSE};
  gboolean urgent_on_bell{FALSE};
  gboolean clickable_url{FALSE};
  gboolean clickable_url_ctrl{FALSE};
  gboolean size_hints{FALSE};
  gboolean filter_unmatched_urls{TRUE};
  gboolean modify_other_keys{FALSE};
  gboolean fullscreen{FALSE};
  gboolean smart_copy{FALSE};
  gboolean dpi_aware{FALSE};
  gboolean copy_mouse_selection{FALSE};
  std::vector<int> url_tags{};
  gdouble font_scale{0.0};
  int font_size{0};
  int completion_limit{5000};
  insert_keybinding_table keybindings{};
  selection_keybinding_table selection_keybindings{};
};

std::optional<insert_action> parse_insert_action(std::string_view name);
std::optional<selection_action> parse_selection_action(std::string_view name);

bool load_config(config_info& config, VteTerminal* vte, GtkWindow* window,
                 GtkWidget* scrollbar = nullptr, GtkWidget* hbox = nullptr,
                 char** icon = nullptr, bool* show_scrollbar_ptr = nullptr);
