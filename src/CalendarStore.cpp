#include "CalendarStore.h"

CalendarStore CalendarStore::instance;

bool CalendarStore::loadFromFile() {
  loaded = true;
  events.clear();
  date.clear();

  if (!Storage.exists(CALENDAR_FILE)) {
    return false;
  }

  String json = Storage.readFile(CALENDAR_FILE);
  if (json.isEmpty()) {
    LOG_DBG("CAL", "Calendar file is empty");
    return false;
  }

  return parseFromJson(json.c_str());
}

String CalendarStore::saveToFile() const {
  if (!Storage.ready()) {
    LOG_ERR("CAL", "SD card not ready");
    return "SD card not ready";
  }

  Storage.mkdir("/.crosspoint");

  JsonDocument doc;
  doc["date"] = date;

  JsonArray eventsArr = doc["events"].to<JsonArray>();
  for (const auto& e : events) {
    JsonObject obj = eventsArr.add<JsonObject>();
    obj["start"] = e.startTime;
    if (!e.endTime.empty()) {
      obj["end"] = e.endTime;
    }
    obj["title"] = e.title;
    if (!e.location.empty()) {
      obj["location"] = e.location;
    }
    if (e.allDay) {
      obj["all_day"] = true;
    }
  }

  String output;
  serializeJson(doc, output);

  LOG_DBG("CAL", "Writing %zu bytes to %s (%zu events)", output.length(), CALENDAR_FILE, events.size());

  if (!Storage.writeFile(CALENDAR_FILE, output)) {
    LOG_ERR("CAL", "Failed to write calendar file (%zu bytes) to %s", output.length(), CALENDAR_FILE);
    return "SD write failed (path=" + String(CALENDAR_FILE) + ", bytes=" + String(output.length()) + ")";
  }

  LOG_DBG("CAL", "Calendar saved: %zu events for %s", events.size(), date.c_str());
  return "";
}

void CalendarStore::clear() {
  events.clear();
  date.clear();
  Storage.remove(CALENDAR_FILE);
  LOG_DBG("CAL", "Calendar data cleared");
}

bool CalendarStore::parseFromJson(const char* json) {
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, json);
  if (err) {
    LOG_ERR("CAL", "Failed to parse calendar JSON: %s", err.c_str());
    return false;
  }

  events.clear();
  date.clear();

  const char* d = doc["date"];
  if (d) {
    date = d;
  }

  JsonArray eventsArr = doc["events"].as<JsonArray>();
  if (eventsArr.isNull()) {
    return true;
  }

  for (JsonObject obj : eventsArr) {
    if (events.size() >= MAX_EVENTS) {
      LOG_DBG("CAL", "Max events (%zu) reached, ignoring rest", MAX_EVENTS);
      break;
    }

    CalendarEvent e;
    const char* start = obj["start"];
    e.startTime = start ? start : "";
    const char* end = obj["end"];
    e.endTime = end ? end : "";
    const char* title = obj["title"];
    e.title = title ? title : "";
    const char* loc = obj["location"];
    e.location = loc ? loc : "";
    e.allDay = obj["all_day"] | false;

    truncateString(e.title, 30);
    truncateString(e.location, 20);

    events.push_back(std::move(e));
  }

  LOG_DBG("CAL", "Parsed %zu events for %s", events.size(), date.c_str());
  return true;
}
