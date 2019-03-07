#include <AZ3166WiFi.h>
#include "Sensor.h"

static bool hasWifi = false;

static DevI2C *ext_i2c;
static HTS221Sensor *ht_sensor;
static int dataSent;

WiFiServer server(502);

static void InitWifi()
{
    if (WiFi.begin() == WL_CONNECTED)
    {
        IPAddress ip = WiFi.localIP();
        Screen.print(2, ip.get_address());
        hasWifi = true;
    }
    else
    {
        hasWifi = false;
    }
}

static void showSent(void)
{
    char tmp[32];
    sprintf(tmp, "Sent:%d", dataSent);
    Screen.print(3, tmp);
}

void setup()
{
    dataSent = 0;
    
    Screen.init();
    Screen.print(0, "IoT DevKit");
    Screen.print(1, "--- MODBUS ---");
    Screen.print(2, "Initializing...");
    InitWifi();
    if (hasWifi)
    {
        server.begin();
    }
    showSent();

    ext_i2c = new DevI2C(D14, D15);
    ht_sensor = new HTS221Sensor(*ext_i2c);
    ht_sensor->init(NULL);
}

// Wait utill continuous zeros of size num
void waitZero(WiFiClient* client, int num)
{
    int cnt = 0;
    while (client->connected())
    {
        char c = client->read();
        if (c == 0)
        {
            cnt++;
        }
        else
        {
            cnt = 0;
        }
        if (cnt == num)
        {
            return;
        }
    }
}

// Read a short(2 bytes) from client
int readShort(WiFiClient* client)
{
    int cnt = 0, ret = 0;
    while (client->connected()) {
        if (cnt == 2)
        {
            return ret;
        }
        char c = client->read();
        if (cnt == 0)
        {
            ret += (int)c * 256;
        }
        else if (cnt == 1)
        {
            ret += c;
        }
        cnt ++;
    }
}

void writeShort(WiFiClient* client, int x)
{
    client->write((char)(x >> 8));
    client->write((char)(x & 0xFF));
}

// Fake register, address 0 for temperature and 1 for humidity
int getRegister(int registerAddress) {
    if (registerAddress < 0 || registerAddress > 1)
    {
        return 0;
    }
    else
    {
        ht_sensor -> reset();
        float value = 0;
        if (registerAddress == 0)
        {
            ht_sensor->getTemperature(&value);
        }
        if (registerAddress == 1)
        {
            ht_sensor->getHumidity(&value);
        }
        return value * 100;
    }
}

void loop() {
    if (!hasWifi)
    {
        return;
    }
    Serial.println("list for incoming clients");
    WiFiClient client = server.available();
    Serial.println("availabled");
    if (client) 
    {
        Serial.println("new client");
        while (client.connected())
        {
            Serial.println("new command");
            waitZero(&client, 4);
            int commandLen = readShort(&client);
            int command = readShort(&client);
            int startRegister = readShort(&client);
            int registerSize = readShort(&client);
            writeShort(&client, 0);
            writeShort(&client, 0);
            writeShort(&client, registerSize * 2 + 3);
            writeShort(&client, command);
            client.write((char)(registerSize * 2));
            for (int i = 0; i < registerSize; ++i) {
                writeShort(&client, getRegister(startRegister + i));
            }
        }
        // give the web browser time to receive the data
        delay(1);

        // close the connection:
        client.stop();
        Serial.println("client disonnected");

        dataSent++;
        showSent();
    }
}
