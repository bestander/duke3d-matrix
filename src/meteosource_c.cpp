#include "meteosource_c.h"
#include "Meteosource.h"
#include <iostream>
#include <jsoncpp/json/json.h>
#include <memory>

#ifdef  __cplusplus
extern "C" {
#endif

void meteosource_init(void) {
        printf("INITING METSOURCE/n");
        const std::string api_key = "";
        const std::string tier = "free";
        Meteosource m = Meteosource(api_key, tier);

        const std::string place_id = "nocatee-7315235";
        const std::string sections = "all";
        const std::string timezone = "UTC";
        const std::string language = "en";
        const std::string units = "metric";
        // auto res = m.get_point_forecast(place_id, sections, timezone, language, units);
        // if (!res)
        // {
        //         return;
        // }

        // if (res->current)
        // {
        //         std::cout << "Current weather: " << res->current->summary << std::endl << std::endl;
        // }

        // if (res->hourly.size() > 0)
        // {
        //         std::cout << "Weather for next 5 hours:" << std::endl;
        //         for (int i = 0; i < 5; ++i)
        //         std::cout << "  " << res->hourly[i]->date << ": temperature " << res->hourly[i]->temperature << ", wind speed: " << res->hourly[i]->wind_speed << std::endl;
        //         std::cout << std::endl;
        // }
}
#ifdef  __cplusplus
}
#endif
