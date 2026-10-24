#pragma once

#include <gtk/gtk.h>
#include <vte/vte.h>

#include <optional>
#include <string>
#include <unordered_map>

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
};

struct config_info {
  char* config_file;
  char* browser{nullptr};
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
  int tag{-1};
  gdouble font_scale{0.0};
  int font_size{0};
  int completion_limit{5000};
  std::unordered_map<std::string, keybinding> keybindings{};
  std::unordered_map<std::string, keybinding> selection_keybindings{};
};

bool load_config(config_info& config, VteTerminal* vte, GtkWindow* window,
                 GtkWidget* scrollbar = nullptr, GtkWidget* hbox = nullptr,
                 char** icon = nullptr, bool* show_scrollbar_ptr = nullptr);
