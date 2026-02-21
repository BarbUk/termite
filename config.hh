#pragma once

#include <gtk/gtk.h>
#include <vte/vte.h>

#include <optional>

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
};

bool load_config(config_info& config, VteTerminal* vte, GtkWindow* window,
                 GtkWidget* scrollbar = nullptr, GtkWidget* hbox = nullptr,
                 char** icon = nullptr, bool* show_scrollbar_ptr = nullptr);
