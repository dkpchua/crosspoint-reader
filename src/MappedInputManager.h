#pragma once

#include <HalGPIO.h>

class GfxRenderer;

class MappedInputManager {
 public:
  enum class Button { Back, Confirm, Left, Right, Up, Down, Power, PageBack, PageForward };

  struct Labels {
    const char* btn1;
    const char* btn2;
    const char* btn3;
    const char* btn4;
  };

  MappedInputManager(HalGPIO& gpio, GfxRenderer& renderer) : gpio(gpio), renderer(renderer) {}

  void update() const { gpio.update(); }
  bool wasPressed(Button button) const;
  bool wasReleased(Button button) const;
  bool isPressed(Button button) const;
  // Reusable touch "back" gesture: a tap released in the top-left corner, OR a tap
  // on the header back area registered by the theme. Folded into Back's press/
  // release edges, so every screen gets it with no per-activity code. False on
  // non-touch devices.
  bool wasBackGesture() const;
  // One-shot: if a tap this frame hit a registered interactive element (theme
  // draw methods register them via TouchRegistry), returns true and writes the
  // element's id. Activities treat the id as "select + activate". False on
  // non-touch devices or when the tap missed every target.
  bool wasItemTapped(int& id) const;
  // Press-edge analogue of wasItemTapped: fires on touch-DOWN over an item, so the
  // activity can move its selection to that item (showing the selected state) before
  // release. Release still activates via wasItemTapped. Mirrors button nav (move
  // selection, then confirm).
  bool wasItemTouchedDown(int& id) const;
  // Long-press variant of wasItemTapped: true on release of a touch over an item
  // held past the long-press threshold (a subset of wasItemTapped's releases, so
  // check this first). Lets a screen distinguish tap vs press-and-hold on touch.
  bool wasItemLongPressed(int& id) const;
  // Like wasItemTapped, but for tab-bar tabs (id = tab index) and cover/card
  // targets (id = item index). Distinct kinds so screens with both a list and a
  // tab bar / cover (Home, Settings) don't confuse them.
  bool wasTabTapped(int& id) const;
  bool wasCoverTapped(int& id) const;
  bool wasAnyPressed() const;
  bool wasAnyReleased() const;
  unsigned long getHeldTime() const;
  Labels mapLabels(const char* back, const char* confirm, const char* previous, const char* next) const;
  // Returns the raw front button index that was pressed this frame (or -1 if none).
  int getPressedFrontButton() const;

 private:
  HalGPIO& gpio;
  GfxRenderer& renderer;

  bool mapButton(Button button, bool (HalGPIO::*fn)(uint8_t) const) const;
};
