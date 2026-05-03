#include "controller/RmtOutputAdapter.h"

#include <string.h>

namespace iheaterlink
{

    namespace
    {
        constexpr uint32_t RMT_FIELD_MAX_US = 32767;
    }

    // Инициализировать RMT-драйвер ESP32 и запустить FreeRTOS-таск периодической отправки.
    void RmtOutputAdapter::begin()
    {
        lastPulseCode_ = encode(lastCommand_);

        if (config_.outputPin == 0xFF)
        {
            return;
        }

        rmt_config_t cfg = {};
        cfg.rmt_mode = RMT_MODE_TX;
        cfg.channel = config_.channel;
        cfg.gpio_num = static_cast<gpio_num_t>(config_.outputPin);
        cfg.clk_div = 80; // 80 МГц / 80 = 1 МГц, 1 тик = 1 мкс
        cfg.mem_block_num = config_.memBlockNum;
        cfg.tx_config.loop_en = false;
        cfg.tx_config.carrier_en = false;
        cfg.tx_config.idle_output_en = true;
        cfg.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;

        if (rmt_config(&cfg) != ESP_OK)
        {
            return;
        }
        if (rmt_driver_install(config_.channel, 0, 0) != ESP_OK)
        {
            return;
        }

        rmt_set_tx_thr_intr_en(config_.channel, false, 0);

        initialized_ = true;
        rebuildFrame(lastPulseCode_);

        // Выделенный FreeRTOS-таск: период RMT не зависит от блокировок main loop
        // (WiFi/MQTT/HA могут подвешивать loopTask на секунды — это убивало тайминг).
        xTaskCreate(&RmtOutputAdapter::rmtTaskEntry,
                    "rmt_tx",
                    3072,
                    this,
                    tskIDLE_PRIORITY + 2,
                    &rmtTask_);
    }

    // Принять новую команду и обновить код пульса для следующей отправки.
    void RmtOutputAdapter::apply(const ControllerOutputCommand &command)
    {
        lastCommand_ = command;
        lastPulseCode_ = encode(command);
    }

    ControllerOutputCommand RmtOutputAdapter::getLastCommand() const
    {
        return lastCommand_;
    }

    // Перевести команду в код пульса: offCode для Off, иначе температура квантованная в min–max.
    uint8_t RmtOutputAdapter::encode(const ControllerOutputCommand &command) const
    {
        if (command.mode == ControllerOutputMode::Off)
        {
            return config_.offCode;
        }

        int target = static_cast<int>(lroundf(command.targetTempC));
        if (target < config_.minTempC)
        {
            target = config_.minTempC;
        }
        if (target > config_.maxTempC)
        {
            target = config_.maxTempC;
        }

        const int base = config_.minTempC;
        const int step = config_.stepTempC > 0 ? config_.stepTempC : 5;
        const int quantized = base + (((target - base) + (step / 2)) / step) * step;
        return static_cast<uint8_t>(quantized);
    }

    // Собрать массив RMT items: sync-LOW + N импульсных пар + end marker.
    void RmtOutputAdapter::rebuildFrame(uint8_t pulseCode)
    {
        size_t idx = 0;

        // SYNC LOW: поля 15-bit, max ~32767 мкс; разбиваем syncLowUs по парам полей.
        uint32_t remaining = config_.syncLowUs;
        while (remaining > 0 && idx < MAX_ITEMS - 1)
        {
            const uint32_t d0 = remaining > RMT_FIELD_MAX_US ? RMT_FIELD_MAX_US : remaining;
            remaining -= d0;
            const uint32_t d1 = remaining > RMT_FIELD_MAX_US ? RMT_FIELD_MAX_US : remaining;
            remaining -= d1;

            items_[idx].level0 = 0;
            items_[idx].duration0 = d0;
            items_[idx].level1 = 0;
            items_[idx].duration1 = d1;
            ++idx;
        }

        // N пар {HIGH, LOW}
        for (uint8_t i = 0; i < pulseCode && idx < MAX_ITEMS - 1; ++i)
        {
            items_[idx].level0 = 1;
            items_[idx].duration0 = config_.pulseHighUs;
            items_[idx].level1 = 0;
            items_[idx].duration1 = config_.pulseLowUs;
            ++idx;
        }

        // End marker — нулевой item завершает передачу, idle удержит LOW.
        items_[idx].val = 0;
        ++idx;

        itemCount_ = idx;
        builtForCode_ = pulseCode;
    }

    // Принудительно отправить фрейм немедленно, не дожидаясь следующего периода.
    void RmtOutputAdapter::forceFrame()
    {
        if (!isEnabled() || !rmtTask_)
            return;
        xTaskNotifyGive(rmtTask_);
    }

    // Точка входа FreeRTOS-таска (static) — делегирует в rmtTaskLoop.
    void RmtOutputAdapter::rmtTaskEntry(void *arg)
    {
        static_cast<RmtOutputAdapter *>(arg)->rmtTaskLoop();
    }

    // Основной цикл таска: отправлять фрейм каждые framePeriodMs или по уведомлению.
    void RmtOutputAdapter::rmtTaskLoop()
    {
        const TickType_t period = pdMS_TO_TICKS(config_.framePeriodMs);
        for (;;)
        {
            sendFrame();
            ulTaskNotifyTake(pdTRUE, period);
        }
    }

    // Перестроить фрейм если код изменился, затем отправить через RMT.
    void RmtOutputAdapter::sendFrame()
    {
        if (builtForCode_ != lastPulseCode_)
        {
            rebuildFrame(lastPulseCode_);
        }

        const uint32_t now = millis();
        const uint32_t dt = lastSentMs_ ? (now - lastSentMs_) : 0;
        lastSentMs_ = now;
        ++seqNum_;
        Serial.printf("[RMT] #%lu t=%lu dt=%lu ms code=%u items=%u\n",
                      (unsigned long)seqNum_,
                      (unsigned long)now, (unsigned long)dt,
                      (unsigned)lastPulseCode_, (unsigned)itemCount_);

        rmt_write_items(config_.channel, items_, static_cast<int>(itemCount_), false);
    }

} // namespace iheaterlink
