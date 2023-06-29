#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <string>
#include <memory>
#include <Preferences.h>

const std::string AP_SSID_BASE = "Bin Monitor ";
const std::string AP_PWD = "setup8888";

class System
{
public:
  std::string GetUniqueId() const
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

Logger logger(std::static_pointer_cast<ILoggerOutput>(std::make_shared<SerialOutput>()));
System os;
WiFiServer server(80);
EEPROMStorage storage(MainStorage::STORAGE_NAME);

void setup()
{
  logger.LogDebug("Starting system...");

  if (!storage.HasKey(MainStorage::WIFI_SSID))
  {
    logger.LogDebug("No Wifi credentials set, going into AP mode for setup...");
    auto deviceSSID = AP_SSID_BASE + os.GetUniqueId();
    WiFi.softAP(deviceSSID.c_str(), AP_PWD.c_str());
    logger.LogDebug("AP is up, SSID " + deviceSSID + " with password " + AP_PWD);
  }
  else
  {
    auto ssid = storage.Get(MainStorage::WIFI_SSID);
    auto password = storage.Get(MainStorage::WIFI_PASSWORD);
    logger.LogDebug("Wifi credentials found, connecting to SSID " + ssid + " with password " + password);
  }
}

void loop()
{
  // put your main code here, to run repeatedly:
}
