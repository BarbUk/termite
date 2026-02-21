#pragma once

#include <gtk/gtk.h>
#include <vte/vte.h>

#include <memory>

template <typename T, typename Deleter>
std::unique_ptr<T, Deleter> make_unique_ptr(T* p, Deleter d) {
  return std::unique_ptr<T, Deleter>(p, d);
}

void adjust_font_size(VteTerminal* vte, GdkWindow* window, int font_size);
void set_size_hints(GtkWindow* window, VteTerminal* vte);
void get_vte_padding(VteTerminal* vte, int* left, int* top, int* right,
                     int* bottom);
void override_background_color(GtkWidget* widget, const GdkRGBA* color);
char* check_match(VteTerminal* vte, GdkEventButton* event);
std::unique_ptr<char, decltype(&g_free)> get_text_range(VteTerminal* vte,
                                                        long start_row,
                                                        long start_col,
                                                        long end_row,
                                                        long end_col);
