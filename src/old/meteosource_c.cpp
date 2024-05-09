#include "meteosource_c.h"
#include "Meteosource.h"
#include <iostream>
#include <jsoncpp/json/json.h>
#include <memory>

#ifdef __cplusplus
extern "C"
{
#endif

        Meteosource *meteosource_init(const char *api_key, const char *tier)
        {
                return new Meteosource(api_key, tier, "https://www.meteosource.com/api");
        }

        void meteosource_destroy(Meteosource *meteosource)
        {
                delete meteosource;
        }

        MinMaxTemperature get_min_max_temparature_forecast(Meteosource *meteosource, const char *place_id, const char *sections, const char *timezone, const char *language, const char *units)
        {
                MinMaxTemperature result;
                result.isError = true;

                auto res = meteosource->get_point_forecast(place_id, sections, timezone, language, units);
                if (!res)
                {
                        return result;
                }

                if (res->current)
                {
                        std::cout << "Current weather: " << res->current->summary << std::endl
                                  << std::endl;
                }


                if (res->hourly.size() > 0)
                {
                        result.isError = false;
                        result.min = res->hourly[0]->temperature;
                        result.max = res->hourly[0]->temperature;

                        std::cout << "Weather for next 5 hours:" << std::endl;
                        for (int i = 0; i < 5; ++i) 
                        {
                                std::cout << "  " << res->hourly[i]->date << ": temperature " << res->hourly[i]->temperature << ", wind speed: " << res->hourly[i]->wind_speed << std::endl;
                                if (res->hourly[i]->temperature > result.max) {
                                        result.max = res->hourly[i]->temperature;
                                }
                                if (res->hourly[i]->temperature < result.min) {
                                        result.min = res->hourly[i]->temperature;
                                }
                        }
                        std::cout << std::endl;
                }
                return result;
        }

#ifdef __cplusplus
}
#endif
