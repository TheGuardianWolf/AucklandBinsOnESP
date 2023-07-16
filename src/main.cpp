#define EZTIME_EZT_NAMESPACE

#include <Arduino.h>
#include <WiFi.h>
#include <string>
#include <memory>
#include <optional>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <AsyncJson.h>
#include <time.h>
#include <ezTime.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const int JSON_BUFFER_SIZE = 50;
const IPAddress AP_IP(10, 100, 1, 1);
const std::string AP_SSID_BASE = "Bin Monitor ";
const std::string AP_PWD = "setup8888";
const unsigned short NTP_SYNC_TIMEOUT = 10000;
const std::string RECYCLING_PAGE_BASE_URL = "https://www.aucklandcouncil.govt.nz/rubbish-recycling/rubbish-recycling-collections/Pages/collection-day-detail.aspx?an=";
const std::string RECYCLING_ADDRESS_BASE_URL = "https://www.aucklandcouncil.govt.nz/_vti_bin/ACWeb/ACservices.svc/GetMatchingPropertyAddresses";

class ISystem
{
public:
    virtual unsigned long getUniqueId() const = 0;
    virtual bool startAccessPoint(const std::string &ssid, const std::string &password) const = 0;
    virtual std::string getLocalIP() const = 0;
    virtual bool connectAccessPoint(const std::string &ssid, const std::string &password) const = 0;
};

class System : public ISystem
{
    DNSServer dnsServer;

public:
    unsigned long getUniqueId() const override
    {
        return ESP.getEfuseMac();
    }

    bool startAccessPoint(const std::string &ssid, const std::string &password) const override
    {
        return WiFi.softAP(ssid.data(), password.data());
    }

    std::string getLocalIP() const override
    {
        return std::string(WiFi.localIP().toString().c_str());
    }

    bool connectAccessPoint(const std::string &ssid, const std::string &password) const override
    {
        return WiFi.begin(ssid.data(), password.data()) == WL_CONNECTED;
    }
};

class IKeyValueStorage
{
public:
    virtual void set(const std::string &key, const std::string &value) = 0;
    virtual std::string get(const std::string &key) = 0;
    virtual void remove(const std::string &key) = 0;
    virtual bool hasKey(const std::string &key) = 0;
};

class EEPROMStorage : public IKeyValueStorage
{
    Preferences m_pref;

public:
    EEPROMStorage(const std::string &name)
    {
        m_pref.begin(name.data());
    }

    ~EEPROMStorage()
    {
        m_pref.end();
    }

    void set(const std::string &key, const std::string &value) override
    {
        m_pref.putString(key.data(), value.data());
    }

    std::string get(const std::string &key) override
    {
        auto val = m_pref.getString(key.data());
        return std::string(val.c_str());
    }

    void remove(const std::string &key) override
    {
        m_pref.remove(key.data());
    }

    bool hasKey(const std::string &key) override
    {
        return m_pref.isKey(key.data());
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
        Serial.println(line.data());
    }
};

class Logger
{
private:
    std::shared_ptr<ILoggerOutput> m_output;

public:
    Logger(std::shared_ptr<ILoggerOutput> output) : m_output(output) {}

    void logDebug(const std::string &info) const
    {
        m_output->WriteLine("DEBUG: " + info);
    }

    void logError(const std::string &info) const
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

struct ConnectionData
{
    std::string ssid;
    std::string password;
    std::string ip;

    ConnectionData(const std::string &ssid, const std::string &password, const std::string &ip)
    {
        this->ssid = ssid;
        this->password = password;
        this->ip = ip;
    }
};

class WifiService
{
private:
    DNSServer dnsServer;
    std::shared_ptr<ISystem> m_pSystem;
    std::shared_ptr<IKeyValueStorage> m_pStorage;

public:
    WifiService(std::shared_ptr<ISystem> pSystem, std::shared_ptr<IKeyValueStorage> pStorage)
        : m_pSystem(pSystem), m_pStorage(pStorage)
    {
    }

    std::optional<ConnectionData> startAccessPoint()
    {
        auto id = m_pSystem->getUniqueId();
        unsigned char compressedId = 0;
        for (auto i = 0; i < sizeof(unsigned long) / sizeof(unsigned char); i++)
        {
            compressedId ^= (id >> (i * sizeof(unsigned char))) & 0xFF;
        }

        auto deviceSSID = AP_SSID_BASE + std::to_string(compressedId);
        if (!m_pSystem->startAccessPoint(deviceSSID.data(), AP_PWD.data()))
        {
            return std::make_optional<ConnectionData>();
        }

        dnsServer.start(53, "*", AP_IP);
        auto ip = std::string(WiFi.softAPIP().toString().c_str());

        return std::make_optional<ConnectionData>(deviceSSID, AP_PWD, ip);
    }

    std::optional<ConnectionData> connect() const
    {
        auto ssid = m_pStorage->get(MainStorage::WIFI_SSID);
        auto password = m_pStorage->get(MainStorage::WIFI_PASSWORD);

        if (!m_pSystem->connectAccessPoint(ssid.data(), password.data()))
        {
            return std::make_optional<ConnectionData>();
        }

        auto ip = std::string(m_pSystem->getLocalIP());

        return std::make_optional<ConnectionData>(ssid, password, ip);
    }

    void setCredentials(const std::string &ssid, const std::string &password) const
    {
        m_pStorage->set(MainStorage::WIFI_SSID, ssid);
        m_pStorage->set(MainStorage::WIFI_PASSWORD, password);
    }

    bool haveCredentials() const
    {
        return m_pStorage->hasKey(MainStorage::WIFI_SSID) && m_pStorage->hasKey(MainStorage::WIFI_PASSWORD);
    }
};

struct DateTime
{
    unsigned char second;
    unsigned char minute;
    unsigned char hour;
    unsigned char day;
    unsigned char month;
    unsigned short year;

    DateTime(unsigned short year, unsigned char month, unsigned char day, unsigned char hour, unsigned char minute, unsigned char second)
        : year(year), month(month), day(day), hour(hour), minute(minute), second(second)
    {
    }
};

struct DateTimeUTC : public DateTime
{
    bool isUTC = true;

    DateTimeUTC(unsigned short year, unsigned char month, unsigned char day, unsigned char hour, unsigned char minute, unsigned char second)
        : DateTime(year, month, day, hour, minute, second)
    {
    }
};

class DateTimeService
{
private:
    std::shared_ptr<ISystem> m_pSystem;

public:
    DateTimeService(std::shared_ptr<ISystem> pSystem)
        : m_pSystem(pSystem)
    {
    }

    bool syncDateTime(unsigned short timeout = 0)
    {
        return ezt::waitForSync(timeout);
    }

    bool isSynced()
    {
        return ezt::lastNtpUpdateTime() > 0;
    }
};

struct RestResponse
{
    std::optional<int> responseCode;
    std::optional<int> errorCode;
    int jsonPayload;
    std::optional<std::string> stringPayload;

    RestResponse(std::optional<int> responseCode, std::optional<int> errorCode, int payload)
        : responseCode(responseCode), errorCode(errorCode), jsonPayload(payload)
    {
    }

    RestResponse(std::optional<int> responseCode, std::optional<int> errorCode, std::optional<std::string> payload)
        : responseCode(responseCode), errorCode(errorCode), stringPayload(payload)
    {
    }
};

class IRestClient
{
public:
    virtual RestResponse get(const std::string &url) const = 0;
    virtual RestResponse post(const std::string &url, const std::string &data) const = 0;
};

class RestClient : public IRestClient
{
private:
    RestResponse request(const std::string &method, const std::string &url, std::string payload = std::string()) const
    {
        HTTPClient client;
        client.useHTTP10();
        client.begin(url.data());
        auto responseCode = client.sendRequest(method.data(), payload.data());
        if (responseCode == -1)
        {
            client.end();
            return RestResponse(std::make_optional<int>(), std::make_optional<int>(-1), 1);
        }

        const char *headers = {"Content-Type"};
        client.collectHeaders(&headers, 1);
        auto contentType = client.header(headers[0]);

        if (contentType == JSON_MIMETYPE)
        {
            auto doc = std::make_unique<DynamicJsonDocument>(JSON_BUFFER_SIZE);
            auto errorCode = deserializeJson(*doc, client.getStream());
            doc->shrinkToFit();
            if (errorCode)
            {
                client.end();
                return RestResponse(std::make_optional<int>(responseCode), std::make_optional<int>(errorCode), 1);
            }

            client.end();
            return RestResponse(std::make_optional<int>(responseCode), std::optional<int>(), 1);
        }
        else
        {
            return RestResponse(std::make_optional<int>(responseCode), std::optional<int>(), std::make_optional<std::string>(client.getString().c_str()));
        }
    }

public:
    RestResponse get(const std::string &url) const
    {
        return request("GET", url);
    }

    RestResponse post(const std::string &url, const std::string &payload) const
    {
        return request("POST", url, payload);
    }
};

struct Property
{
    std::string address;
    std::string rateAccountKey;

    Property(const std::string &address, const std::string &rateAccountKey)
        : address(address), rateAccountKey(rateAccountKey)
    {
    }
};

struct CollectionDates
{
};

class CouncilWebService
{
private:
    std::shared_ptr<IRestClient> m_pRestClient;

public:
    CouncilWebService(std::shared_ptr<IRestClient> pRestClient)
        : m_pRestClient(pRestClient)
    {
    }

    std::optional<Property> getProperty(std::string search)
    {
        auto response = m_pRestClient->post(RECYCLING_ADDRESS_BASE_URL, ("{"
                                                                         "\"RateKeyRequired\": false,"
                                                                         "\"ResultCount\": \"1\","
                                                                         "\"SearchText\": \"" +
                                                                         search + "\""
                                                                                  "}"));

        // if (!response.responseCode || response.errorCode || response.jsonPayload == nullptr || response.responseCode < 200 || response.responseCode >= 300)
        // {
        //     return std::optional<Property>();
        // }

        // JsonDocument *doc = response.jsonPayload.get();
        // if (!doc->containsKey(0))
        // {
        //     return std::optional<Property>();
        // }

        // auto p = std::make_optional<Property>(
        //     doc[0]["Address"]
        //         .as<std::string>(),
        //     doc[0]["ACRateAccountKey"].as<std::string>());

        // return p;
        return std::optional<Property>();
    }

    std::optional<CollectionDates> getCollectionDates(const std::string &rateAccountKey)
    {
        // auto response = m_pRestClient->get(RECYCLING_PAGE_BASE_URL);

        // if (!response.responseCode || response.errorCode || !response.jsonPayload || response.jsonPayload == nullptr || response.responseCode < 200 || response.responseCode >= 300)
        // {
        //     return std::optional<CollectionDates>();
        // }
        return std::optional<CollectionDates>();
    }
};

// Provides updated bin information
class BinService
{
private:
    std::shared_ptr<IKeyValueStorage> m_pStorage;

public:
    BinService(std::shared_ptr<IKeyValueStorage> pStorage)
        : m_pStorage(pStorage)
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
auto pOS = std::make_shared<System>();
AsyncWebServer server(80);

auto pStorage = std::make_shared<EEPROMStorage>(MainStorage::STORAGE_NAME);
auto pWifiService = std::make_shared<WifiService>(pOS, pStorage);
auto pDateTimeService = std::make_shared<DateTimeService>(pOS);

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
    logger.logDebug("Starting system...");

    if (!pWifiService->haveCredentials())
    {
        logger.logDebug("No Wifi credentials set, going into AP mode for setup...");
        auto optConnection = pWifiService->startAccessPoint();

        auto connection = optConnection.value();
        logger.logDebug("AP is up, SSID " + connection.ssid + " with password " + connection.password + " on ip: " + connection.ip);

        // Use captive portal to request wifi details
        server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER); // only when requested from AP
    }
    else
    {
        auto optConnection = pWifiService->connect();

        auto connection = optConnection.value();
        logger.logDebug("Wifi credentials found, connecting to SSID " + connection.ssid + " with password " + connection.password + " on ip " + connection.ip);

        pDateTimeService->syncDateTime(NTP_SYNC_TIMEOUT);
    }

    /////////////////
    // Web Handlers
    /////////////////

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
                      { request->send(404, JSON_MIMETYPE, "{\"message\":\"Not found\"}"); });

    server.begin();
}

void loop()
{
    // put your main code here, to run repeatedly:
}
