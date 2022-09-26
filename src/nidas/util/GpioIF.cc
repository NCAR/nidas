
#include "GpioIF.h"

#include <string>
#include <map>

#define GPIONAME(GPIO) { GPIO, "GPIO_" #GPIO }


std::string
nidas::util::gpio2Str(GPIO_PORT_DEFS gpio)
{
    static std::map<GPIO_PORT_DEFS, const char*> names =
    {
        GPIONAME(SER_PORT0),
        GPIONAME(SER_PORT1),
        GPIONAME(SER_PORT2),
        GPIONAME(SER_PORT3),
        GPIONAME(SER_PORT4),
        GPIONAME(SER_PORT5),
        GPIONAME(SER_PORT6),
        GPIONAME(SER_PORT7),
        GPIONAME(PWR_28V),
        GPIONAME(PWR_AUX),
        GPIONAME(PWR_BANK1),
        GPIONAME(PWR_BANK2),
        GPIONAME(PWR_BTCON),
        // these last two had different strings than their enum name, so maybe
        // someday the enum should be changed to match.
        { DEFAULT_SW, "GPIO_DEFAULT_SW1" },
        { WIFI_SW, "GPIO_WIFI_SW2" }
    };

    auto iter = names.find(gpio);
    if (iter != names.end())
        return iter->second;
    return "GPIO_ILLEGAL";
}

