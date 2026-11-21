#include <glib.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "config.hh"
#include "util.hh"

namespace {

int g_failures = 0;

void expect_true(bool cond, const char* expr, int line) {
  if (!cond) {
    std::fprintf(stderr, "FAIL line %d: %s\n", line, expr);
    ++g_failures;
  }
}

#define EXPECT(expr) expect_true((expr), #expr, __LINE__)

bool is_non_space(gunichar c) { return !g_unichar_isspace(c); }

void test_backward_word_no_underflow() {
  // Sentinel ' ' at buf[0]; word "foo" starts at buf[1].
  // Passing &buf[1] exposes any codepoints[-1] read.
  const std::array<gunichar, 4> buf = {' ', 'f', 'o', 'o'};
  EXPECT(find_backward_word_col(buf.data() + 1, 3, 1, is_word_char) == 0);

  // Sentinel 'x' (a word char) at buf[0]: buggy code reads buf[0] at i==0
  // and keeps in_word=true instead of treating index 0 as the line boundary.
  const std::array<gunichar, 5> buf_word_sentinel = {'x', 'a', ' ', 'b', 'c'};
  EXPECT(find_backward_word_col(buf_word_sentinel.data() + 1, 4, 2,
                                is_word_char) == 0);

  // Empty line must safely return 0 without reading codepoints[-1].
  const std::array<gunichar, 2> empty_buf = {'x', 0};
  EXPECT(find_backward_word_col(empty_buf.data() + 1, 0, 1, is_word_char) == 0);

  // Standard multi-word backward movement.
  const std::array<gunichar, 8> line = {'f', 'o', 'o', ' ', 'b', 'a', 'r', ' '};
  EXPECT(find_backward_word_col(line.data(), 8, 1, is_word_char) == 4);
  EXPECT(find_backward_word_col(line.data(), 8, 2, is_word_char) == 0);
}

void test_forward_word_no_oob() {
  // Sentinel '\n' at buf[0] with length == 0: buggy code reads codepoints[-1]
  // and decrements length to -1.
  const std::array<gunichar, 2> empty_buf = {'\n', 'a'};
  EXPECT(find_forward_word_delta(empty_buf.data() + 1, 0, 1, false,
                                 is_word_char) == 0);
  EXPECT(find_forward_word_delta(empty_buf.data() + 1, 0, 1, true,
                                 is_word_char) == 0);

  // Word end at the very end of the buffer without a trailing NUL/newline:
  // sentinel 'x' placed immediately past the slice exposes codepoints[length].
  const std::array<gunichar, 5> no_nul = {'f', 'o', 'o', 'x', 'y'};
  EXPECT(find_forward_word_delta(no_nul.data(), 3, 1, true, is_word_char) == 2);

  // Normal forward word & word-end with trailing newline.
  const std::array<gunichar, 8> line = {'f', 'o', 'o', ' ',
                                        'b', 'a', 'r', '\n'};
  EXPECT(find_forward_word_delta(line.data(), 8, 1, false, is_word_char) == 4);
  EXPECT(find_forward_word_delta(line.data(), 8, 1, true, is_word_char) == 2);
  EXPECT(find_forward_word_delta(line.data(), 8, 2, true, is_word_char) == 6);
}

void test_first_match_col_with_nonzero_start() {
  const std::array<gunichar, 5> suffix = {' ', ' ', 'a', 'b', 'c'};
  // Range started at column 10, first non-space is at offset 2 -> column 12.
  EXPECT(find_first_match_col(10, suffix.data(), 5, is_non_space) == 12);
  EXPECT(find_first_match_col(0, suffix.data(), 5, is_non_space) == 2);

  const std::array<gunichar, 2> spaces = {' ', ' '};
  EXPECT(find_first_match_col(4, spaces.data(), 2, is_non_space) == -1);
}

void test_url_hint_launch_logic() {
  // Zero URLs must never trigger launch (avoids log10(0) UB).
  EXPECT(!should_launch_url_hint(0, "1"));
  EXPECT(!should_launch_url_hint(0, "0"));

  // Input "0" or non-digits must never trigger launch (avoids log10(0) UB).
  EXPECT(!should_launch_url_hint(5, "0"));
  EXPECT(!should_launch_url_hint(15, "0"));
  EXPECT(!should_launch_url_hint(5, ""));
  EXPECT(!should_launch_url_hint(5, "abc"));

  // 9 URLs: any single digit 1..9 is immediately unambiguous.
  EXPECT(should_launch_url_hint(9, "1"));
  EXPECT(should_launch_url_hint(9, "9"));

  // 10 URLs: "1" is ambiguous (1 vs 10); "2" and "10" are unambiguous.
  EXPECT(!should_launch_url_hint(10, "1"));
  EXPECT(should_launch_url_hint(10, "2"));
  EXPECT(should_launch_url_hint(10, "10"));

  // 100 URLs: "1" and "10" are ambiguous (10 vs 100); "11" and "100" launch.
  EXPECT(!should_launch_url_hint(100, "1"));
  EXPECT(!should_launch_url_hint(100, "10"));
  EXPECT(should_launch_url_hint(100, "11"));
  EXPECT(should_launch_url_hint(100, "100"));
}

void test_utf8_column_offset() {
  // Pure ASCII: byte offset equals column.
  EXPECT(utf8_byte_offset_to_column("abc https://x.com", 4) == 4);

  // 2-byte UTF-8 ('é' = 2 bytes, 1 display cell): "café " is 6 bytes, 5 cols.
  const std::string_view accented = "caf\xc3\xa9 https://x.com";
  const size_t accented_pos = accented.find("https://");
  EXPECT(accented_pos == 6);
  EXPECT(utf8_byte_offset_to_column(accented, accented_pos) == 5);

  // Wide CJK ('中' = 3 bytes, 2 cells; '文' = 3 bytes, 2 cells): "中文 " = 7
  // bytes, 5 display columns.
  const std::string_view cjk = "\xe4\xb8\xad\xe6\x96\x87 https://x.com";
  const size_t cjk_pos = cjk.find("https://");
  EXPECT(cjk_pos == 7);
  EXPECT(utf8_byte_offset_to_column(cjk, cjk_pos) == 5);
}

void test_normalize_dropped_text() {
  // Must handle nullptr safely without crashing.
  EXPECT(normalize_dropped_text(nullptr).empty());

  // Must strip file://, unescape %20, and strip both \r and \n.
  EXPECT(normalize_dropped_text("file:///tmp/my%20file.txt\r\n") ==
         "/tmp/my file.txt");

  // Plain text preserves spaces and strips \r\n.
  EXPECT(normalize_dropped_text("hello world\r\n") == "hello world");

  // Non-file URI is preserved without unescaping.
  EXPECT(normalize_dropped_text("https://example.com/a%20b\n") ==
         "https://example.com/a%20b");
}

void test_extract_completion_tokens() {
  std::string buf = "beta alpha\tgamma\nalpha  delta\n";
  auto tokens = extract_completion_tokens(buf.data());
  EXPECT(tokens.size() == 4);
  if (tokens.size() == 4) {
    // Returned tokens must be NUL-terminated and sorted uniquely.
    EXPECT(std::strcmp(tokens[0], "alpha") == 0);
    EXPECT(std::strcmp(tokens[1], "beta") == 0);
    EXPECT(std::strcmp(tokens[2], "delta") == 0);
    EXPECT(std::strcmp(tokens[3], "gamma") == 0);
  }
}

void test_headless_version_flag() {
  const char* bin = std::getenv("TERMITE_BIN");
  if (!bin) {
    return;
  }

  gchar* stdout_buf = nullptr;
  gchar* stderr_buf = nullptr;
  gint wait_status = 0;
  GError* error = nullptr;

  char** env = g_get_environ();
  env = g_environ_unsetenv(env, "DISPLAY");
  env = g_environ_unsetenv(env, "WAYLAND_DISPLAY");

  char* argv[] = {const_cast<char*>(bin), const_cast<char*>("--version"),
                  nullptr};
  const gboolean spawned =
      g_spawn_sync(nullptr, argv, env, G_SPAWN_DEFAULT, nullptr, nullptr,
                   &stdout_buf, &stderr_buf, &wait_status, &error);

  EXPECT(spawned == TRUE);
  EXPECT(error == nullptr);
  EXPECT(g_spawn_check_wait_status(wait_status, nullptr) == TRUE);
  EXPECT(stdout_buf != nullptr &&
         std::strstr(stdout_buf, "termite v") != nullptr);

  g_clear_error(&error);
  g_free(stdout_buf);
  g_free(stderr_buf);
  g_strfreev(env);
}

void test_action_parsing_and_hint_raii() {
  EXPECT(parse_insert_action("zoom-reset") == insert_action::zoom_reset);
  EXPECT(parse_insert_action("scroll-page-down") ==
         insert_action::scroll_page_down);
  EXPECT(!parse_insert_action("not-a-real-action").has_value());

  EXPECT(parse_selection_action("exit-selection") ==
         selection_action::exit_selection);
  EXPECT(parse_selection_action("url-hints") == selection_action::url_hints);
  EXPECT(!parse_selection_action("unknown-action").has_value());

  // Verify hint_info RAII safely releases shared border/fg references on reset
  // and destruction without double-freeing.
  hint_info hints;
  hints.font = pango_font_description_from_string("Monospace 10");
  hints.fg = cairo_pattern_create_rgb(1, 1, 1);
  hints.bg = cairo_pattern_create_rgb(0, 0, 0);
  hints.af = cairo_pattern_create_rgb(0.9, 0.5, 0.5);
  hints.ab = cairo_pattern_create_rgb(0, 0, 0);
  hints.border = cairo_pattern_reference(hints.fg);
  EXPECT(cairo_pattern_get_reference_count(hints.fg) == 2);

  hints.reset();
  EXPECT(hints.font == nullptr);
  EXPECT(hints.fg == nullptr);
  EXPECT(hints.border == nullptr);
}

}  // namespace

int main() {
  test_backward_word_no_underflow();
  test_forward_word_no_oob();
  test_first_match_col_with_nonzero_start();
  test_url_hint_launch_logic();
  test_utf8_column_offset();
  test_normalize_dropped_text();
  test_extract_completion_tokens();
  test_headless_version_flag();
  test_action_parsing_and_hint_raii();

  if (g_failures > 0) {
    std::fprintf(stderr, "%d test assertion(s) failed.\n", g_failures);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
