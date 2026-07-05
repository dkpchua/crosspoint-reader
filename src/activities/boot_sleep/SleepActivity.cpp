#include "SleepActivity.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Txt.h>
#include <Xtc.h>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "CalendarStore.h"
#include "activities/reader/ReaderUtils.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "images/Logo120.h"
#include "images/MoonIcon.h"

void SleepActivity::onEnter() {
  Activity::onEnter();

  const bool renderQuickResume =
      SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::QUICK_RESUME ||
      (fromTimeout &&
       SETTINGS.quickResumeSleepScreen == CrossPointSettings::QUICK_RESUME_SLEEP_SCREEN::QUICK_RESUME_AFTER_TIMEOUT);

  if (renderQuickResume) {
    return renderLastScreenSleepScreen();
  }

  // Show popup with reader orientation only when going to sleep from reader
  if (APP_STATE.lastSleepFromReader) {
    ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);
    GUI.drawPopup(renderer, tr(STR_ENTERING_SLEEP));
    renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  } else {
    GUI.drawPopup(renderer, tr(STR_ENTERING_SLEEP));
  }

  switch (SETTINGS.sleepScreen) {
    case (CrossPointSettings::SLEEP_SCREEN_MODE::BLANK):
      return renderBlankSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::CALENDAR):
      return renderCalendarSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::CUSTOM):
      return renderCustomSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::COVER):
      return renderCoverSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::COVER_CUSTOM):
      if (APP_STATE.lastSleepFromReader) {
        return renderCoverSleepScreen();
      } else {
        return renderCustomSleepScreen();
      }
    default:
      return renderDefaultSleepScreen();
  }
}

void SleepActivity::renderCustomSleepScreen() const {
  // Check if we have a /.sleep (preferred) or /sleep directory
  const char* sleepDir = nullptr;
  auto dir = Storage.open("/.sleep");

  // Look for sleep.bmp on the root of the sd card to determine if we should
  // render a custom sleep screen instead of the default.
  // This takes priority over the /sleep folder.
  HalFile file;
  if (Storage.openFileForRead("SLP", "/sleep.bmp", file)) {
    Bitmap bitmap(file, true);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      LOG_DBG("SLP", "Loading: /sleep.bmp");
      renderBitmapSleepScreen(bitmap);
      file.close();
      if (dir) dir.close();
      return;
    }
    file.close();
  }

  if (dir && dir.isDirectory()) {
    sleepDir = "/.sleep";
  } else {
    dir = Storage.open("/sleep");
    if (dir && dir.isDirectory()) {
      sleepDir = "/sleep";
    }
  }

  if (sleepDir) {
    std::vector<std::string> files;
    char name[500];
    // collect all valid BMP files
    for (auto dirFile = dir.openNextFile(); dirFile; dirFile = dir.openNextFile()) {
      if (dirFile.isDirectory()) {
        dirFile.close();
        continue;
      }
      dirFile.getName(name, sizeof(name));
      auto filename = std::string(name);
      if (filename[0] == '.') {
        dirFile.close();
        continue;
      }

      if (!FsHelpers::hasBmpExtension(filename)) {
        LOG_DBG("SLP", "Skipping non-.bmp file name: %s", name);
        dirFile.close();
        continue;
      }
      Bitmap bitmap(dirFile);
      if (bitmap.parseHeaders() != BmpReaderError::Ok) {
        LOG_DBG("SLP", "Skipping invalid BMP file: %s", name);
        dirFile.close();
        continue;
      }
      files.emplace_back(filename);
      dirFile.close();
    }
    const auto numFiles = files.size();
    if (numFiles > 0) {
      // Pick a random wallpaper, excluding recently shown ones.
      // Window: up to SLEEP_RECENT_COUNT entries, capped at numFiles-1.
      const uint16_t fileCount = static_cast<uint16_t>(std::min(numFiles, static_cast<size_t>(UINT16_MAX)));
      const uint8_t window =
          static_cast<uint8_t>(std::min(static_cast<size_t>(APP_STATE.recentSleepFill), numFiles - 1));
      auto randomFileIndex = static_cast<uint16_t>(random(fileCount));
      for (uint8_t attempt = 0; attempt < 20 && APP_STATE.isRecentSleep(randomFileIndex, window); attempt++) {
        randomFileIndex = static_cast<uint16_t>(random(fileCount));
      }
      APP_STATE.pushRecentSleep(randomFileIndex);
      APP_STATE.saveToFile();
      const auto filename = std::string(sleepDir) + "/" + files[randomFileIndex];
      HalFile randFile;
      if (Storage.openFileForRead("SLP", filename, randFile)) {
        LOG_DBG("SLP", "Randomly loading: %s/%s", sleepDir, files[randomFileIndex].c_str());
        delay(100);
        Bitmap bitmap(randFile, true);
        if (bitmap.parseHeaders() == BmpReaderError::Ok) {
          renderBitmapSleepScreen(bitmap);
          randFile.close();
          dir.close();
          return;
        }
        randFile.close();
      }
    }
  }
  if (dir) dir.close();

  renderDefaultSleepScreen();
}

void SleepActivity::renderDefaultSleepScreen() const {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  renderer.drawImage(Logo120, (pageWidth - 120) / 2, (pageHeight - 120) / 2, 120, 120);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 70, tr(STR_CROSSPOINT), true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 95, tr(STR_SLEEPING));

  // Make sleep screen dark unless light is selected in settings
  if (SETTINGS.sleepScreen != CrossPointSettings::SLEEP_SCREEN_MODE::LIGHT) {
    renderer.invertScreen();
  }

  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

void SleepActivity::renderBitmapSleepScreen(const Bitmap& bitmap) const {
  int x, y;
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  float cropX = 0, cropY = 0;

  LOG_DBG("SLP", "bitmap %d x %d, screen %d x %d", bitmap.getWidth(), bitmap.getHeight(), pageWidth, pageHeight);
  if (bitmap.getWidth() > pageWidth || bitmap.getHeight() > pageHeight) {
    // image will scale, make sure placement is right
    float ratio = static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
    const float screenRatio = static_cast<float>(pageWidth) / static_cast<float>(pageHeight);

    LOG_DBG("SLP", "bitmap ratio: %f, screen ratio: %f", ratio, screenRatio);
    if (ratio > screenRatio) {
      // image wider than viewport ratio, scaled down image needs to be centered vertically
      if (SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP) {
        cropX = 1.0f - (screenRatio / ratio);
        LOG_DBG("SLP", "Cropping bitmap x: %f", cropX);
        ratio = (1.0f - cropX) * static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
      }
      x = 0;
      y = std::round((static_cast<float>(pageHeight) - static_cast<float>(pageWidth) / ratio) / 2);
      LOG_DBG("SLP", "Centering with ratio %f to y=%d", ratio, y);
    } else {
      // image taller than viewport ratio, scaled down image needs to be centered horizontally
      if (SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP) {
        cropY = 1.0f - (ratio / screenRatio);
        LOG_DBG("SLP", "Cropping bitmap y: %f", cropY);
        ratio = static_cast<float>(bitmap.getWidth()) / ((1.0f - cropY) * static_cast<float>(bitmap.getHeight()));
      }
      x = std::round((static_cast<float>(pageWidth) - static_cast<float>(pageHeight) * ratio) / 2);
      y = 0;
      LOG_DBG("SLP", "Centering with ratio %f to x=%d", ratio, x);
    }
  } else {
    // center the image
    x = (pageWidth - bitmap.getWidth()) / 2;
    y = (pageHeight - bitmap.getHeight()) / 2;
  }

  LOG_DBG("SLP", "drawing to %d x %d", x, y);
  renderer.clearScreen();

  const bool hasGreyscale = bitmap.hasGreyscale() &&
                            SETTINGS.sleepScreenCoverFilter == CrossPointSettings::SLEEP_SCREEN_COVER_FILTER::NO_FILTER;

  renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);

  if (SETTINGS.sleepScreenCoverFilter == CrossPointSettings::SLEEP_SCREEN_COVER_FILTER::INVERTED_BLACK_AND_WHITE) {
    renderer.invertScreen();
  }

  if (hasGreyscale) {
    // OEM grayscale pipeline base: on X3 this displays the frame with the
    // dedicated "AA-pre-BW(mid)" differential waveform, leaving every pixel
    // in the calibrated state the gray nudge refresh expects; on X4 it is a
    // plain HALF refresh (previous behavior).
    renderer.displayGrayscaleBase(HalDisplay::HALF_REFRESH);
  } else {
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  }

  if (hasGreyscale) {
    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
    renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);
    renderer.copyGrayscaleLsbBuffers();

    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
    renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);
    renderer.copyGrayscaleMsbBuffers();

    renderer.displayGrayBuffer();
    renderer.setRenderMode(GfxRenderer::BW);
  }
}

void SleepActivity::renderCoverSleepScreen() const {
  void (SleepActivity::*renderNoCoverSleepScreen)() const;
  switch (SETTINGS.sleepScreen) {
    case (CrossPointSettings::SLEEP_SCREEN_MODE::COVER_CUSTOM):
      renderNoCoverSleepScreen = &SleepActivity::renderCustomSleepScreen;
      break;
    default:
      renderNoCoverSleepScreen = &SleepActivity::renderDefaultSleepScreen;
      break;
  }

  if (APP_STATE.openEpubPath.empty()) {
    return (this->*renderNoCoverSleepScreen)();
  }

  std::string coverBmpPath;
  bool cropped = SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP;

  // Check if the current book is XTC, TXT, or EPUB
  if (FsHelpers::hasXtcExtension(APP_STATE.openEpubPath)) {
    // Handle XTC file
    Xtc lastXtc(APP_STATE.openEpubPath, "/.crosspoint");
    if (!lastXtc.load()) {
      LOG_ERR("SLP", "Failed to load last XTC");
      return (this->*renderNoCoverSleepScreen)();
    }

    if (!lastXtc.generateCoverBmp()) {
      LOG_ERR("SLP", "Failed to generate XTC cover bmp");
      return (this->*renderNoCoverSleepScreen)();
    }

    coverBmpPath = lastXtc.getCoverBmpPath();
  } else if (FsHelpers::hasTxtExtension(APP_STATE.openEpubPath)) {
    // Handle TXT file - looks for cover image in the same folder
    Txt lastTxt(APP_STATE.openEpubPath, "/.crosspoint");
    if (!lastTxt.load()) {
      LOG_ERR("SLP", "Failed to load last TXT");
      return (this->*renderNoCoverSleepScreen)();
    }

    if (!lastTxt.generateCoverBmp()) {
      LOG_ERR("SLP", "No cover image found for TXT file");
      return (this->*renderNoCoverSleepScreen)();
    }

    coverBmpPath = lastTxt.getCoverBmpPath();
  } else if (FsHelpers::hasEpubExtension(APP_STATE.openEpubPath)) {
    // Handle EPUB file
    Epub lastEpub(APP_STATE.openEpubPath, "/.crosspoint");
    // Skip loading css since we only need metadata here
    if (!lastEpub.load(true, true)) {
      LOG_ERR("SLP", "Failed to load last epub");
      return (this->*renderNoCoverSleepScreen)();
    }

    if (!lastEpub.generateCoverBmp(cropped)) {
      LOG_ERR("SLP", "Failed to generate cover bmp");
      return (this->*renderNoCoverSleepScreen)();
    }

    coverBmpPath = lastEpub.getCoverBmpPath(cropped);
  } else {
    return (this->*renderNoCoverSleepScreen)();
  }

  HalFile file;
  if (Storage.openFileForRead("SLP", coverBmpPath, file)) {
    Bitmap bitmap(file);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      LOG_DBG("SLP", "Rendering sleep cover: %s", coverBmpPath.c_str());
      renderBitmapSleepScreen(bitmap);
      return;
    }
  }

  return (this->*renderNoCoverSleepScreen)();
}

void SleepActivity::renderLastScreenSleepScreen() const {
  const auto pageHeight = renderer.getScreenHeight();
  renderer.drawImage(MoonIcon, 0, pageHeight - MOONICON_HEIGHT, MOONICON_WIDTH, MOONICON_HEIGHT);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

void SleepActivity::renderBlankSleepScreen() const {
  renderer.clearScreen();
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

void SleepActivity::renderCalendarSleepScreen() const {
  CALENDAR_STORE.ensureLoaded();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  if (!CALENDAR_STORE.hasData()) {
    renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2 - 10, tr(STR_CALENDAR_DATA_MISSING), true,
                              EpdFontFamily::BOLD);
    renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 20, tr(STR_CALENDAR));
    renderer.invertScreen();
    renderer.displayBuffer(HalDisplay::FULL_REFRESH);
    return;
  }

  const auto& events = CALENDAR_STORE.getEvents();
  const auto& dateStr = CALENDAR_STORE.getDate();

  const int margin = 20;
  const int timeColW = 60;
  const int lineH = 20;
  const int locH = 15;
  const int sectionGap = 10;

  // --- Parse date string "YYYY-MM-DD" to "WEEKDAY, D MONTH YYYY" ---
  static const char* dayNames[] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
  static const char* monthNames[] = {"JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
                                     "JULY",   "AUGUST",   "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"};
  char dateHeader[48] = {0};
  int year = 0, month = 0, day = 0;
  if (sscanf(dateStr.c_str(), "%d-%d-%d", &year, &month, &day) == 3 && year >= 1900 && month >= 1 && month <= 12 &&
      day >= 1 && day <= 31) {
    // Zeller's congruence for day of week (0=Saturday)
    int m = month;
    int y = year;
    if (m < 3) { m += 12; y -= 1; }
    int k = y % 100;
    int j = y / 100;
    int dow = (day + 13 * (m + 1) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    // Convert: 0=Sat -> 6, 1=Sun -> 0, 2=Mon -> 1, etc.
    dow = (dow + 6) % 7;
    snprintf(dateHeader, sizeof(dateHeader), "%s, %d %s %d", dayNames[dow], day, monthNames[month - 1], year);
  } else {
    snprintf(dateHeader, sizeof(dateHeader), "%s", dateStr.c_str());
  }

  // --- Separate all-day and timed events ---
  std::vector<const CalendarEvent*> allDayEvents;
  std::vector<const CalendarEvent*> timedEvents;
  for (const auto& e : events) {
    if (e.allDay) {
      allDayEvents.push_back(&e);
    } else {
      timedEvents.push_back(&e);
    }
  }

  int y = margin;

  // --- Date header ---
  renderer.drawText(UI_12_FONT_ID, margin, y, dateHeader, true, EpdFontFamily::BOLD);
  y += renderer.getTextHeight(UI_12_FONT_ID) + 6;
  renderer.drawLine(margin, y, pageWidth - margin, y, true);
  y += sectionGap + 4;

  // --- All-day events section ---
  if (!allDayEvents.empty()) {
    renderer.drawText(SMALL_FONT_ID, margin, y, "ALL-DAY", true, EpdFontFamily::BOLD);
    y += locH + 4;

    const int boxX = margin;
    const int boxW = pageWidth - 2 * margin;

    for (const auto* e : allDayEvents) {
      if (y > pageHeight - margin - lineH) break;

      const int itemH = lineH + (e->location.empty() ? 0 : locH) + 12;
      renderer.drawRect(boxX, y, boxW, itemH, true);
      renderer.drawText(UI_10_FONT_ID, boxX + 8, y + 5, e->title.c_str(), true);
      if (!e->location.empty()) {
        renderer.drawText(SMALL_FONT_ID, boxX + 8, y + 5 + lineH, e->location.c_str(), false);
      }
      y += itemH + 4;
    }
    y += sectionGap;
  }

  // --- Timeline section ---
  if (!timedEvents.empty()) {
    if (y + locH + 4 <= pageHeight - margin) {
      renderer.drawText(SMALL_FONT_ID, margin, y, "TIMELINE", true, EpdFontFamily::BOLD);
      y += locH + 8;
      renderer.drawLine(margin, y, pageWidth - margin, y, true);
      y += 8;
    }

    const int timelineX = margin + timeColW;
    const int timelineEndX = pageWidth - margin;

    for (size_t i = 0; i < timedEvents.size(); i++) {
      const auto* e = timedEvents[i];
      if (y > pageHeight - margin - lineH) break;

      const int eventH = lineH + (e->location.empty() ? 0 : locH) + 12;
      const int textTopOffset = (eventH - lineH) / 2;

      // Time label on left
      renderer.drawText(SMALL_FONT_ID, margin, y + textTopOffset, e->startTime.c_str(), true);

      // Timeline connector line
      renderer.drawLine(timelineX, y + eventH / 2, timelineX + 12, y + eventH / 2, true);
      renderer.drawLine(timelineX, y, timelineX, y + eventH - 4, true);

      // Event block
      renderer.drawRect(timelineX + 14, y, timelineEndX - timelineX - 14, eventH, true);
      renderer.drawText(UI_10_FONT_ID, timelineX + 22, y + textTopOffset, e->title.c_str(), true);
      if (!e->location.empty()) {
        renderer.drawText(SMALL_FONT_ID, timelineX + 22, y + textTopOffset + lineH, e->location.c_str(), false);
      }

      y += eventH + 4;
    }
  }

  if (events.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_NO_EVENTS_TODAY));
  }

  renderer.invertScreen();
  renderer.displayBuffer(HalDisplay::FULL_REFRESH);
}
