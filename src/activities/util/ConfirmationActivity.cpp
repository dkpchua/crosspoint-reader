#include "ConfirmationActivity.h"

#include <I18n.h>

#include "HalDisplay.h"
#include "components/TouchRegistry.h"
#include "components/UITheme.h"

ConfirmationActivity::ConfirmationActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                           const std::string& heading, const std::string& body)
    : Activity("Confirmation", renderer, mappedInput), heading(heading), body(body) {}

void ConfirmationActivity::onEnter() {
  Activity::onEnter();

  lineHeight = renderer.getLineHeight(fontId);
  const int maxWidth = renderer.getScreenWidth() - (margin * 2);

  if (!heading.empty()) {
    safeHeading = renderer.truncatedText(fontId, heading.c_str(), maxWidth, EpdFontFamily::BOLD);
  }
  if (!body.empty()) {
    safeBody = renderer.truncatedText(fontId, body.c_str(), maxWidth, EpdFontFamily::REGULAR);
  }

  int totalHeight = 0;
  if (!safeHeading.empty()) totalHeight += lineHeight;
  if (!safeBody.empty()) totalHeight += lineHeight;
  if (!safeHeading.empty() && !safeBody.empty()) totalHeight += spacing;

  startY = (renderer.getScreenHeight() - totalHeight) / 2;

  requestUpdate(true);
}

void ConfirmationActivity::render(RenderLock&& lock) {
  renderer.clearScreen();

  int currentY = startY;
  LOG_DBG("CONF", "currentY: %d", currentY);
  // Draw Heading
  if (!safeHeading.empty()) {
    renderer.drawCenteredText(fontId, currentY, safeHeading.c_str(), true, EpdFontFamily::BOLD);
    currentY += lineHeight + spacing;
  }

  // Draw Body
  if (!safeBody.empty()) {
    renderer.drawCenteredText(fontId, currentY, safeBody.c_str(), true, EpdFontFamily::REGULAR);
    currentY += lineHeight;
  }

  // On-screen Cancel / Confirm buttons (also tappable). The footer hints below map
  // the same actions to Left/Right for button devices, but on touch-only devices
  // the hints are hidden, so these are the only affordance.
  const int btnH = lineHeight + 20;
  const int totalW = renderer.getScreenWidth() - margin * 2;
  const int btnW = (totalW - spacing) / 2;
  const int btnY = currentY + spacing * 3;
  const Rect cancelRect{margin, btnY, btnW, btnH};
  const Rect confirmRect{margin + btnW + spacing, btnY, btnW, btnH};
  renderer.drawRect(cancelRect.x, cancelRect.y, cancelRect.width, cancelRect.height);
  renderer.drawRect(confirmRect.x, confirmRect.y, confirmRect.width, confirmRect.height);
  const int btnTextY = btnY + (btnH - lineHeight) / 2;
  UITheme::drawCenteredText(renderer, cancelRect, fontId, btnTextY, I18N.get(StrId::STR_CANCEL));
  UITheme::drawCenteredText(renderer, confirmRect, fontId, btnTextY, I18N.get(StrId::STR_CONFIRM));
  TouchRegistry::getInstance().add(cancelRect, 0, TouchRegistry::Item);
  TouchRegistry::getInstance().add(confirmRect, 1, TouchRegistry::Item);

  // Draw UI Elements
  const auto labels = mappedInput.mapLabels("", "", I18N.get(StrId::STR_CANCEL), I18N.get(StrId::STR_CONFIRM));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer(HalDisplay::RefreshMode::FAST_REFRESH);
}

void ConfirmationActivity::loop() {
  // Tap the on-screen buttons: id 1 = Confirm, id 0 = Cancel.
  int tappedId = -1;
  if (mappedInput.wasItemTapped(tappedId)) {
    ActivityResult res;
    res.isCancelled = (tappedId != 1);
    setResult(std::move(res));
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    ActivityResult res;
    res.isCancelled = false;
    setResult(std::move(res));
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    ActivityResult res;
    res.isCancelled = true;
    setResult(std::move(res));
    finish();
    return;
  }
}