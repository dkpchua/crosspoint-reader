#pragma once

#include <EpdFontFamily.h>

#include <functional>
#include <memory>

#include "CrossPointSettings.h"
#include "components/themes/BaseTheme.h"

class UITheme {
  // Static instance
  static UITheme instance;

 public:
  UITheme();
  static UITheme& getInstance() { return instance; }

  const ThemeMetrics& getMetrics() const { return *currentMetrics; }
  const BaseTheme& getTheme() const { return *currentTheme; }
  Rect getScreenSafeArea(const GfxRenderer& renderer, bool hasFrontButtonHints = false,
                         bool hasSideButtonHints = false);
  static void drawCenteredText(const GfxRenderer& renderer, Rect screen, int fontId, int y, const char* text,
                               bool black = true, EpdFontFamily::Style style = EpdFontFamily::REGULAR);
  void reload();
  void setTheme(CrossPointSettings::UI_THEME type);
  static int getNumberOfItemsPerPage(const GfxRenderer& renderer, bool hasHeader, bool hasTabBar, bool hasButtonHints,
                                     bool hasSubtitle, int extraReservedHeight = 0);
  static std::string getCoverThumbPath(std::string coverBmpPath, int coverHeight);
  static UIIcon getFileIcon(const std::string& filename);
  static int getStatusBarHeight();
  static int getProgressBarHeight();

  // Per-board UI scale (BoardConfig::ACTIVE.uiScale): 1.0 on button devices, >1 on
  // touch devices so chrome is finger-sized. Applied to the metrics below.
  static float uiScale();

 private:
  const ThemeMetrics* currentMetrics;
  // Scaled copy of the active theme's constexpr metrics (scaled by uiScale() in
  // setTheme()). currentMetrics points here so getMetrics() returns scaled values.
  ThemeMetrics scaledMetrics{};
  std::unique_ptr<BaseTheme> currentTheme;
};

// Scale a theme's pixel-dimension metrics by `scale` (counts, percents, ratios,
// and bools are left untouched). At scale 1.0 it is an exact copy (no drift).
// Shared by UITheme (active-theme metrics) and each theme's own draw code.
ThemeMetrics scaleThemeMetrics(const ThemeMetrics& base, float scale);

// Helper macro to access current theme
#define GUI UITheme::getInstance().getTheme()
