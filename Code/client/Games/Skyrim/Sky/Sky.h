#pragma once

struct TESWeather;

struct Sky
{
    static Sky* Get() noexcept;

    // inline: definido no header, então precisa de uma única definição entre todas
    // as TUs que o incluem. Sem isso o link só passa com /FORCE:MULTIPLE, que não
    // se aplica a DLLs (STClientPayload.dll).
    inline static bool s_shouldUpdateWeather = true;

    virtual ~Sky();

    void SetWeather(TESWeather* apWeather) noexcept;
    void ForceWeather(TESWeather* apWeather) noexcept;
    void ReleaseWeatherOverride() noexcept;
    void ResetWeather() noexcept;

    TESWeather* GetWeather() const noexcept;

    uint8_t unk8[0x48 - 0x8];
    TESWeather* pCurrentWeather;
    uint8_t unk50[0x2C8 - 0x50];
};

static_assert(sizeof(Sky) == 0x2C8);
