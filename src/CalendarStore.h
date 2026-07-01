#pragma once

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstring>
#include <string>
#include <vector>

struct CalendarEvent {
  std::string startTime;
  std::string endTime;
  std::string title;
  std::string location;
  bool allDay = false;
};

class CalendarStore {
 private:
  static CalendarStore instance;
  std::vector<CalendarEvent> events;
  std::string date;
  bool loaded = false;

  static constexpr size_t MAX_EVENTS = 20;
  static constexpr char CALENDAR_FILE[] = "/.crosspoint/calendar.json";

  CalendarStore() = default;

  void truncateString(std::string& s, size_t maxLen) const {
    if (s.length() > maxLen) {
      s = s.substr(0, maxLen);
    }
  }

 public:
  CalendarStore(const CalendarStore&) = delete;
  CalendarStore& operator=(const CalendarStore&) = delete;

  static CalendarStore& getInstance() { return instance; }

  bool loadFromFile();
  // Returns empty string on success, or an error description on failure.
  String saveToFile() const;
  void clear();

  bool parseFromJson(const char* json);

  const std::vector<CalendarEvent>& getEvents() const { return events; }
  const std::string& getDate() const { return date; }
  bool hasData() const { return !events.empty() || !date.empty(); }
  bool isLoaded() const { return loaded; }

  void ensureLoaded() {
    if (!loaded) {
      loadFromFile();
    }
  }
};

#define CALENDAR_STORE CalendarStore::getInstance()
