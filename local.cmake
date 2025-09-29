set(MATRIX_DISPLAY_BOARD    aslak3_pico_w_ice40up)
#set(MATRIX_DISPLAY_BOARD    adafuit_matrixportal_s3)
#set(MATRIX_DISPLAY_BOARD    pimoroni_interstate_75w)

# Used by MQTT topics and Home Assistant entities
set(DEVICE_NAME             matrix_display)
# The Home Assistant device name
set(DEVICE_PRETTY_NAME      "Matrix Display")

# See https://raw.githubusercontent.com/nayarsystems/posix_tz_db/refs/heads/master/zones.csv
set(TZ                      "GMT0BST,M3.5.0/1,M10.5.0")

set(WIFI_SSID               wifi-ssid)
set(WIFI_PASSWORD           wifi-password)
set(MQTT_BROKER_IP          192.168.0.100)
set(MQTT_BROKER_USERNAME    mqtt-username)
set(MQTT_BROKER_PASSWORD    mqtt-password)
