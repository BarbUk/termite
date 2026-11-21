#pragma once

#include <gtk/gtk.h>
#include <vte/vte.h>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

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

using char_predicate = bool (*)(gunichar);

bool is_word_char(gunichar c);
long find_backward_word_col(const gunichar* codepoints, long length, long count,
                            char_predicate is_word);
long find_forward_word_delta(const gunichar* codepoints, long length,
                             long count, bool goto_word_end,
                             char_predicate is_word);
long find_first_match_col(long start_col, const gunichar* codepoints,
                          long length, char_predicate is_match);
bool should_launch_url_hint(size_t url_count, const char* input);
long utf8_byte_offset_to_column(std::string_view line, size_t byte_offset);
std::string normalize_dropped_text(const char* raw);
std::vector<const char*> extract_completion_tokens(char* buffer);
