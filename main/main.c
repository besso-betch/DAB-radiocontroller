#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"
#include "time.h"
#include "sys/time.h"
#include "driver/i2c.h"

#define SPP_TAG "TinyAudio"
#define SPP_SERVER_NAME "SPP_SERVER"
#define DEVICE_NAME "HorchVx"
#define SPP_DATA_LEN 20
#define TINY_DATA_LENGTH_SHORT 4
#define TINY_DATA_LENGTH_LONG 12
#define I2C_SLAVE_SCL_IO 26
#define I2C_SLAVE_SDA_IO 25
#define I2C_SLAVE_TX_BUF_LEN 256
#define I2C_SLAVE_RX_BUF_LEN 256
#define I2C_SLAVE_ADDR 0x42
#define I2C_SLAVE_NUM 0 /*!< I2C port number for slave dev */

static const esp_spp_mode_t esp_spp_mode = ESP_SPP_MODE_CB;
static const esp_spp_sec_t sec_mask = ESP_SPP_SEC_AUTHENTICATE;
static const esp_spp_role_t role_slave = ESP_SPP_ROLE_SLAVE;

static uint8_t spp_data[SPP_DATA_LEN];

//#define taste_shortPress 50
//#define taste_longPress 3000

TaskHandle_t taskHandleTinyAudio = NULL;

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    switch (event)
    {
    case ESP_SPP_INIT_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_INIT_EVT");
        esp_bt_dev_set_device_name(DEVICE_NAME);
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        esp_spp_start_srv(sec_mask, role_slave, 0, SPP_SERVER_NAME);
        break;
    case ESP_SPP_DISCOVERY_COMP_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_DISCOVERY_COMP_EVT");
        break;
    case ESP_SPP_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_OPEN_EVT");
        break;
    case ESP_SPP_CLOSE_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CLOSE_EVT");
        break;
    case ESP_SPP_START_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_START_EVT");
        break;
    case ESP_SPP_CL_INIT_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CL_INIT_EVT");
        break;
    case ESP_SPP_DATA_IND_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_DATA_IND_EVT len=%d handle=%d", param->data_ind.len, param->data_ind.handle);
        //esp_log_buffer_hex("", param->data_ind.data, param->data_ind.len);
        //esp_spp_write(param->srv_open.handle, SPP_DATA_LEN, spp_data);
        if (param->data_ind.len == 1)
        {
            xTaskNotify(taskHandleTinyAudio, param->data_ind.data[0], eSetValueWithoutOverwrite);
        }
        break;
    case ESP_SPP_CONG_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CONG_EVT");
        break;
    case ESP_SPP_WRITE_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_WRITE_EVT");
        break;
    case ESP_SPP_SRV_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_OPEN_EVT");
        break;
    default:
        break;
    }
}

void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
    {
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGI(SPP_TAG, "authentication success: %s", param->auth_cmpl.device_name);
            esp_log_buffer_hex(SPP_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
        }
        else
        {
            ESP_LOGE(SPP_TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
        }
        break;
    }
    case ESP_BT_GAP_PIN_REQ_EVT:
    {
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_PIN_REQ_EVT min_16_digit:%d", param->pin_req.min_16_digit);
        if (param->pin_req.min_16_digit)
        {
            ESP_LOGI(SPP_TAG, "Input pin code: 0000 0000 0000 0000");
            esp_bt_pin_code_t pin_code = {0};
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
        }
        else
        {
            ESP_LOGI(SPP_TAG, "Input pin code: 1697");
            esp_bt_pin_code_t pin_code;
            pin_code[0] = '1';
            pin_code[1] = '6';
            pin_code[2] = '9';
            pin_code[3] = '7';
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
        }
        break;
    }
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey:%d", param->key_notif.passkey);
        break;
    case ESP_BT_GAP_KEY_REQ_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
        break;
    default:
    {
        ESP_LOGI(SPP_TAG, "event: %d", event);
        break;
    }
    }
    return;
}

static void taskTinyAudio(void *pvParameters)
{
    uint32_t receivedValue;
    BaseType_t xResult;

    uint8_t dataShort[TINY_DATA_LENGTH_SHORT];
    dataShort[0] = 0x55;
    dataShort[1] = 0xAA;

    uint8_t dataLong[TINY_DATA_LENGTH_LONG];
    dataLong[0] = 0x55;
    dataLong[1] = 0xAA;
    dataLong[4] = 0x55;
    dataLong[5] = 0xAA;
    dataLong[8] = 0x55;
    dataLong[9] = 0xAA;

    while (true)
    {
        xResult = xTaskNotifyWait(0, ULONG_MAX, &receivedValue, portMAX_DELAY);

        if (xResult == pdPASS)
        {
            switch (receivedValue)
            {
            case '1':
                dataShort[2] = 0x01;
                dataShort[3] = 0xFE;
                break;
            case '2':
                dataShort[2] = 0x02;
                dataShort[3] = 0xFD;
                break;
            case '3':
                dataShort[2] = 0x06;
                dataShort[3] = 0xF9;
                break;
            case '4':
                dataShort[2] = 0x05;
                dataShort[3] = 0xFA;
                break;
            case '5':
                dataShort[2] = 0x03;
                dataShort[3] = 0xFC;
                break;
            case '6':
                dataShort[2] = 0x04;
                dataShort[3] = 0xFB;
                break;
            default:
                dataShort[2] = 0x0F;
                dataShort[3] = 0xF0;
                break;
            }

            size_t d_size = i2c_slave_write_buffer(I2C_SLAVE_NUM, dataShort, TINY_DATA_LENGTH_SHORT, 1000 / portTICK_RATE_MS);

            if (d_size == 0)
            {
                ESP_LOGW(SPP_TAG, "i2c slave tx buffer full");
            }
            else
            {
                ESP_LOGI(SPP_TAG, "Written bytes: %d", d_size);
            }

            if (receivedValue == '7') //RDS mode
            {
                vTaskDelay(50 / portTICK_PERIOD_MS);
                dataLong[2] = 0x11;
                dataLong[3] = 0xEE;
                dataLong[6] = 0x21;
                dataLong[7] = 0xDE;
                dataLong[10] = 0x41;
                dataLong[11] = 0xBE;

                d_size = i2c_slave_write_buffer(I2C_SLAVE_NUM, dataLong, TINY_DATA_LENGTH_LONG, 1000 / portTICK_RATE_MS);

                if (d_size == 0)
                {
                    ESP_LOGW(SPP_TAG, "i2c slave tx buffer full");
                }
                else
                {
                    ESP_LOGI(SPP_TAG, "Written bytes: %d", d_size);
                }
            }
            else if (receivedValue == '8') //Autosøk
            {
                vTaskDelay(50 / portTICK_PERIOD_MS);
                dataLong[2] = 0x15;
                dataLong[3] = 0xEA;
                dataLong[6] = 0x25;
                dataLong[7] = 0xDA;
                dataLong[10] = 0x45;
                dataLong[11] = 0xBA;

                dataShort[2] = 0x01;
                dataShort[3] = 0xFE;

                d_size = i2c_slave_write_buffer(I2C_SLAVE_NUM, dataLong, TINY_DATA_LENGTH_LONG, 1000 / portTICK_RATE_MS);

                if (d_size == 0)
                {
                    ESP_LOGW(SPP_TAG, "i2c slave tx buffer full");
                }
                else
                {
                    ESP_LOGI(SPP_TAG, "Written bytes: %d", d_size);
                }

                vTaskDelay(50 / portTICK_PERIOD_MS);

                d_size = i2c_slave_write_buffer(I2C_SLAVE_NUM, dataShort, TINY_DATA_LENGTH_SHORT, 1000 / portTICK_RATE_MS);

                if (d_size == 0)
                {
                    ESP_LOGW(SPP_TAG, "i2c slave tx buffer full");
                }
                else
                {
                    ESP_LOGI(SPP_TAG, "Written bytes: %d", d_size);
                }
            }
            else if (receivedValue >= 65 && receivedValue <= 68)
            {
                dataLong[2] = 0x16;
                dataLong[3] = 0xE9;
                dataLong[6] = 0x26;
                dataLong[7] = 0xD9;
                dataLong[10] = 0x46;
                dataLong[11] = 0xBA;

                switch (receivedValue)
                {
                case 'A':
                    dataShort[2] = 0x01;
                    dataShort[3] = 0xFE;
                    break;
                case 'B':
                    dataShort[2] = 0x02;
                    dataShort[3] = 0xFD;
                    break;
                case 'C':
                    dataShort[2] = 0x06;
                    dataShort[3] = 0xF9;
                    break;
                case 'D':
                    dataShort[2] = 0x05;
                    dataShort[3] = 0xFA;
                    break;
                }

                vTaskDelay(50 / portTICK_PERIOD_MS);
                i2c_slave_write_buffer(I2C_SLAVE_NUM, dataLong, TINY_DATA_LENGTH_LONG, 1000 / portTICK_RATE_MS);
                vTaskDelay(50 / portTICK_PERIOD_MS);
                i2c_slave_write_buffer(I2C_SLAVE_NUM, dataShort, TINY_DATA_LENGTH_SHORT, 1000 / portTICK_RATE_MS);
            }
        }
    }
}
static esp_err_t i2c_slave_init()
{
    int i2c_slave_port = I2C_SLAVE_NUM;
    i2c_config_t conf_slave;
    conf_slave.sda_io_num = I2C_SLAVE_SDA_IO;
    conf_slave.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf_slave.scl_io_num = I2C_SLAVE_SCL_IO;
    conf_slave.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf_slave.mode = I2C_MODE_SLAVE;
    conf_slave.slave.addr_10bit_en = 0;
    conf_slave.slave.slave_addr = I2C_SLAVE_ADDR;
    i2c_param_config(i2c_slave_port, &conf_slave);
    return i2c_driver_install(i2c_slave_port, conf_slave.mode,
                              I2C_SLAVE_RX_BUF_LEN,
                              I2C_SLAVE_TX_BUF_LEN, 0);
}

void app_main()
{
    for (int i = 0; i < SPP_DATA_LEN; ++i)
    {
        spp_data[i] = i + 65;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_init()) != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "%s initialize bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_enable()) != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "%s enable bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_gap_register_callback(esp_bt_gap_cb)) != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "%s gap register failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_spp_register_callback(esp_spp_cb)) != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "%s spp register failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_spp_init(esp_spp_mode)) != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "%s spp init failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
    esp_bt_pin_code_t pin_code;
    esp_bt_gap_set_pin(pin_type, 0, pin_code);

    ESP_ERROR_CHECK(i2c_slave_init());

    xTaskCreate(taskTinyAudio, "taskTest", 2048, NULL, 1, &taskHandleTinyAudio);
}
