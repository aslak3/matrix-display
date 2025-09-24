#include <stdint.h>
#include <string.h>

class TemperatureSensor
{
	public:
		virtual float get_temperature() = 0;
};

class PressureSensor
{
	public:
		virtual float get_pressure() = 0;
};

class HumiditySensor
{
	public:
		virtual float get_humidity() = 0;
};

class IlluminanceSensor
{
	public:
		virtual uint16_t get_illuminance() = 0;
};

class Sensor
{
	public:
		virtual bool configure(void) { return false; }
		virtual bool request_run(void) { return false; }
		virtual bool receive_data(void) { return false; }
		virtual TemperatureSensor *temperature_sensor(void) { return NULL; }
		virtual HumiditySensor *humidity_sensor(void) { return NULL; }
		virtual PressureSensor *pressure_sensor(void) { return NULL; }
		virtual IlluminanceSensor *illuminance_sensor(void) { return NULL; }		
};

#define DS3231_TEMPERATURE_LEN 2

class DS3231Sensor: public Sensor, public TemperatureSensor
{
	public:
        static Sensor *create(void);
		bool receive_data(void) override;
		TemperatureSensor *temperature_sensor(void) override;
		float get_temperature(void) override;

    private:
        uint8_t buffer[DS3231_TEMPERATURE_LEN];
};

class BME680Sensor: public Sensor, public TemperatureSensor, public HumiditySensor, public PressureSensor
{
	public:
        static Sensor *create(void);
		bool configure(void) override;
		bool request_run(void) override;
		bool receive_data(void) override;
		TemperatureSensor *temperature_sensor(void) override;
		float get_temperature(void) override;
		HumiditySensor *humidity_sensor(void) override;
		float get_humidity(void) override;
		PressureSensor *pressure_sensor(void) override;
		float get_pressure(void) override;

    private:
		float t_fine = 0.0;
		uint8_t current_state[256];
		uint8_t calib[25 + 16];
};

#define BH1750_ILLUMINANCE_LEN 2

class BH1750Sensor: public Sensor, public IlluminanceSensor
{
	public:
        static Sensor *create(void);
		bool request_run(void) override;
		bool receive_data(void) override;
		IlluminanceSensor *illuminance_sensor(void) override;
		uint16_t get_illuminance(void) override;

    private:
        uint8_t buffer[BH1750_ILLUMINANCE_LEN];
};
