# Notable Changes

This file documents notable user-facing changes introduced in each release.

## v16.15

### Added

- Configurable keybindings under `[keybindings]` (Insert mode) and
  `[selection_keybindings]` (Selection mode) using GTK accelerator syntax,
  including configurable scroll keys (`scroll-line-up`, `scroll-line-down`,
  `scroll-page-up`, `scroll-page-down`).
- Headless unit test suite (`termite-unit`) executed by `meson test`.

### Changed

- Converted `README.rst` to Markdown (`README.md`) and updated manual pages
  and default config comments.
- Optimized keypress dispatch with O(1) enum-indexed keybinding tables,
  in-place zero-allocation completion tokenization, cached compiled URL
  regexes across reloads, `://` pre-filtering in URL hints, and no-op guards
  in `dpi_aware` font scaling.

### Fixed

- Fixed out-of-bounds buffer reads in backward and forward word movement, and
  offset calculation in `move_first`.
- Fixed undefined behavior (`log10(0)`) when typing `0` or triggering URL hints
  with no matches, and fixed horizontal hint badge alignment on lines with
  multibyte UTF-8 or wide characters.
- Fixed PTY output unpausing prematurely when launching URL hints from Insert
  mode (`ctrl-shift-x`).
- Fixed null dereference, memory leak, `\r` handling, and percent-decoding on
  drag-and-drop `file://` URIs.
- Fixed memory leaks on configuration reload (Cairo hint patterns, Pango hint
  font, `word_char_exceptions`, GTK CSS providers, and duplicate VTE URL regex
  matchers) and attached the hand cursor to all 6 URL patterns.
- Fixed live-reload handling for `filter_unmatched_urls` and `fullscreen`.
- Fixed `termite --version` failing when no display server is available, and
  fixed `ctrl-=` default binding for `zoom-reset`.

## v16.14

### Changed

- Updated VTE subproject to version 0.84.1.

## v16.13

### Added

- Added automated GitHub release workflow packaging self-contained `.tar.zst`
  source tarballs with bundled subprojects.

### Changed

- Updated VTE subproject to version 0.84.0.
- Updated CI toolchain to GCC 14 and added `ccache` support.

## v16.12

### Changed

- Modularized configuration and utility logic into `config.{cc,hh}` and
  `util.{cc,hh}`.
- Switched build standard to C++23 (`gnu++23`).

## v16.11

### Added

- Added `completion_limit` configuration option (default: `5000` lines) to
  bound scrollback scanning during word completion.

### Changed

- Enabled Link-Time Optimization (`b_lto=true`) by default.
- Switched `SIGUSR1` config reload to `g_unix_signal_add()` on the GLib main
  loop.
- Reused a single `PangoLayout` across all URL hint markers per draw pass.
- Replaced deprecated `vte_terminal_get_window_title()` and
  `vte_terminal_get_current_directory_uri()` with VTE termprop APIs.

## v16.10

### Changed

- Updated VTE subproject to version 0.82.3.
- Enabled PCRE2 JIT compilation (`vte_regex_jit`) for URL matching and
  scrollback search regexes, caching the compiled search regex across repeated
  searches.
- Added pre-commit hooks (`clang-format`, `yamllint`, `typos`, `actionlint`,
  `rumdl`).

## v16.9

### Fixed

- Fix vertical locations of URL hint labels in presence of empty lines
  in the terminal widget.

## v16.8

### Fixed

- Fix URL hints mode not working in version 16.7 due to behaviour change
  introduced in VTE in its 0.76.x update.

### Changed

- Avoid usage of deprecated VTE functionality.
- Make VTE widget centered inside the Termite window.

## v16.7

### Added

- New `copy_mouse_selection` configuration option, which will automatically
  send selections done with the mouse in insert mode to the `CLIPBOARD`
  buffer as well as `PRIMARY` one. When disabled, selections are still
  sent to `PRIMARY`, which was the existing behaviour.
- **Experimental**. New `dpi_aware` configuration option, which will adjust
  the font size to match the expected physical size in points for the
  monitor where the window is being shown. The adjustment is updated when
  the window changes monitors, but this is somewhat unreliable under some
  Wayland compositors, therefore the option is disabled by default.

### Changed

- Update VTE subproject to version 0.76.1, which brings in a number of
  performance improvements.
- Meson 1.0.0 is now the minimum usable for building.
- Replace uses of `std::not1()`, which has been deprecated in C++17,
  with the generic `std::not_fn()` helper.

## v16.6

### Changed

- Improved URL regex matching.
- Update VTE subproject to version 0.72.2.
- Meson 0.60.0 or newer is now required for building.

## v16.5

### Added

- New `bidi` configuration option, which enables rendering of
  bidirectional text.
- New `arabic_shaping` configuration option, which enables shaping of
  Arabic text.

### Changed

- Update VTE subproject to version 0.70.0.

## v16.4

### Changed

- Update VTE subproject to version 0.68.0.

## v16.3

### Changed

- Update VTE subproject to a prerelease of version 0.66.3, which fixes
  a crash triggered by visually selecting text when certain fonts and
  sizes are configured (for example Cascadia Mono at 12pt).

## v16.2

### Changed

- Update VTE subproject to version 0.66.2

## v16.1

### Added

- Make `Ctrl-+`, `Ctrl--`, and `Ctrl-=` are not usable in selection mode.

### Fixed

- Make `Ctrl-+` and `Ctrl--` work when the plus and minus keys from the
  numeric keypad are used.

### Changed

- Update VTE subproject to version 0.66.0.

## v16.0

### Added

- New `smart_copy` configuration option, which enables using `Ctrl-C` and
  `Ctrl-V` for copy-paste (instead of the usual `Ctrl-Shift-C` and
  `Ctrl-Shift-V`).
- New `clickable_url_ctrl` configuration option, which changes behaviour
  to require `Ctrl` to be pressed to act on detected URLs.
- Support dragging text and files onto Termite windows. Files are converted
  into their file system path.

### Changed

- Update VTE subproject to a prerelease of version 0.64.3.

## v15.3

### Added

- Make `PageDown` and `PageUp` work in command mode, moving by one screenful
  at a time (sans one line for context).

### Changed

- Update VTE subproject to a prerelease of version 0.64.3.

## v15.2

### Changed

- Update VTE subproject to version 0.64.1.
- Allow choosing values for the Meson `b_lto` and `b_ndebug` options.
- Depend on Meson 0.58, which allows installation with `--skip-subprojects`.
- Avoid using the deprecated `vte_terminal_spawn_sync()` function.

## Fixed

- Install configuration file correctly in `sysconfdir`.

## v15.1

### Added

- Movement and search commands now support an optional numeric count. For
  example `5j` will move the cursor five lines down.
- New `gtk_dark_theme` and `word_char_exceptions` configuration options.
- Running `termite --version` prints an additional line with the VTE version
  and the list of features it has been built with.

### Changed

- The build system will build and link statically a suitable version of
  the VTE library during compilation.

### Fixed

- Corrected off-by-one error in backward word movement after manually moving
  with `hjkl` past the end of the line's input.

### Removed

- The `TERM` environment variable is no longer set to `xterm-terminfo`, and
  the custom _terminfo_ entry is not supplied anymore. Instead, the values
  provided by VTE are used.
