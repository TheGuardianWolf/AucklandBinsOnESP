#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <string>
#include <memory>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <AsyncJson.h>

const std::string JSON_TYPE = "application/json";
const std::string AP_SSID_BASE = "Bin Monitor ";
const std::string AP_PWD = "setup8888";

class ISystem
{
public:
  virtual std::string GetUniqueId() const = 0;
};

class System : public ISystem
{
public:
  std::string GetUniqueId() const override
  {
    return std::to_string(ESP.getEfuseMac());
  }
};

class IKeyValueStorage
{
public:
  virtual void Add(const std::string &key, const std::string &value) = 0;
  virtual std::string Get(const std::string &key) = 0;
  virtual void Remove(const std::string &key) = 0;
  virtual bool HasKey(const std::string &key) = 0;
};

class EEPROMStorage : public IKeyValueStorage
{
  Preferences m_pref;

public:
  EEPROMStorage(const std::string &name)
  {
    m_pref.begin(name.c_str());
  }

  ~EEPROMStorage()
  {
    m_pref.end();
  }

  void Add(const std::string &key, const std::string &value) override
  {
    m_pref.putString(key.c_str(), value.c_str());
  }

  std::string Get(const std::string &key) override
  {
    auto val = m_pref.getString(key.c_str());
    return std::string(val.c_str());
  }

  void Remove(const std::string &key) override
  {
    m_pref.remove(key.c_str());
  }

  bool HasKey(const std::string &key) override
  {
    return m_pref.isKey(key.c_str());
  }
};

class ILoggerOutput
{
public:
  virtual void WriteLine(const std::string &line) const = 0;
};

class SerialOutput : public ILoggerOutput
{
public:
  SerialOutput()
  {
    Serial.begin(115200);
    Serial.println();
  }

  ~SerialOutput()
  {
    Serial.end();
  }

  void WriteLine(const std::string &line) const override
  {
    Serial.println(line.c_str());
  }
};

class Logger
{
private:
  std::shared_ptr<ILoggerOutput> m_output;

public:
  Logger(std::shared_ptr<ILoggerOutput> output) : m_output(output) {}
  void LogDebug(const std::string &info) const
  {
    m_output->WriteLine("DEBUG: " + info);
  }
  void LogError(const std::string &info) const
  {
    m_output->WriteLine("ERROR: " + info);
  }
};

namespace MainStorage
{
  const std::string STORAGE_NAME = "Main";
  const std::string WIFI_SSID = "WifiSSID";
  const std::string WIFI_PASSWORD = "WifiPassword";
}

class WifiService
{
private:
  std::shared_ptr<ISystem> mSystem;

public:
  WifiService(std::shared_ptr<ISystem> system)
  {
    mSystem = system;
  }

  void startAccessPoint()
  {
  }

  void connect()
  {
  }

  void setCredentials()
  {
  }

  void haveCredentials()
  {
  }
};

class DateTimeService
{
private:
  std::shared_ptr<ISystem> mSystem;

public:
  DateTimeService(std::shared_ptr<ISystem> system)
  {
    mSystem = system;
  }

  void getDateTime()
  {
  }

  void setDateTime()
  {
  }

  void syncDateTime()
  {
  }
};

class CouncilWebService
{
private:
public:
  CouncilWebService()
  {
  }

  void getRateAccountKey()
  {
  }

  void getCollectionDates()
  {
  }
};

// Provides updated bin information
class BinService
{
private:
public:
  BinService()
  {
  }

  void getLocation()
  {
  }

  void setLocation()
  {
  }

  void getBinStatus()
  {
  }
};

Logger logger(std::static_pointer_cast<ILoggerOutput>(std::make_shared<SerialOutput>()));
System os;
DNSServer dnsServer;
AsyncWebServer server(80);
EEPROMStorage storage(MainStorage::STORAGE_NAME);

class CaptiveRequestHandler : public AsyncWebHandler
{
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request)
  {
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request)
  {
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->print("<!DOCTYPE html><html><head><title>Captive Portal</title></head><body>");
    response->print("<p>This is out captive portal front page.</p>");
    response->printf("<p>You were trying to reach: http://%s%s</p>", request->host().c_str(), request->url().c_str());
    response->printf("<p>Try opening <a href='http://%s'>this link</a> instead</p>", WiFi.softAPIP().toString().c_str());
    response->print("</body></html>");
    request->send(response);
  }
};

void setup()
{
  logger.LogDebug("Starting system...");

  if (!storage.HasKey(MainStorage::WIFI_SSID))
  {
    logger.LogDebug("No Wifi credentials set, going into AP mode for setup...");
    auto deviceSSID = AP_SSID_BASE + os.GetUniqueId();
    WiFi.softAP(deviceSSID.c_str(), AP_PWD.c_str());
    dnsServer.start(53, "*", WiFi.softAPIP());
    logger.LogDebug("AP is up, SSID " + deviceSSID + " with password " + AP_PWD);

    // Use captive portal to request wifi details
    server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER); // only when requested from AP
  }
  else
  {
    auto ssid = storage.Get(MainStorage::WIFI_SSID);
    auto password = storage.Get(MainStorage::WIFI_PASSWORD);
    logger.LogDebug("Wifi credentials found, connecting to SSID " + ssid + " with password " + password);
  }

  // Set WiFi credentials
  auto wifiCredentialsHandler = std::make_unique<AsyncCallbackJsonWebHandler>("/api/wifi-credentials", [](AsyncWebServerRequest *request, JsonVariant &json) {

  });
  wifiCredentialsHandler->setMethod(HTTP_POST);
  server.addHandler(wifiCredentialsHandler.get());

  // Get DateTime
  server.on("/api/datetime", HTTP_GET, [](AsyncWebServerRequest *request) {

  });

  // Set DateTime
  auto dateTimeHandler = std::make_unique<AsyncCallbackJsonWebHandler>("/api/datetime", [](AsyncWebServerRequest *request, JsonVariant &json) {

  });
  dateTimeHandler->setMethod(HTTP_POST);
  server.addHandler(dateTimeHandler.get());

  // Get Location
  server.on("/api/location", HTTP_GET, [](AsyncWebServerRequest *request) {

  });

  // Set Location
  auto locationHandler = std::make_unique<AsyncCallbackJsonWebHandler>("/api/location", [](AsyncWebServerRequest *request, JsonVariant &json) {

  });
  locationHandler->setMethod(HTTP_POST);
  server.addHandler(locationHandler.get());

  // Get Collection Dates
  server.on("/api/collection-dates", HTTP_GET, [](AsyncWebServerRequest *request) {

  });
  server.onNotFound([](AsyncWebServerRequest *request)
                    { request->send(404, JSON_TYPE.c_str(), "{\"message\":\"Not found\"}"); });

  server.begin();
}

void loop()
{
  // put your main code here, to run repeatedly:
}
