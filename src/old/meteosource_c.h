#ifndef meteosource_c_h_
#define meteosource_c_h_

#ifdef  __cplusplus
extern "C" {
#endif

typedef struct Meteosource Meteosource;

Meteosource* meteosource_init(const char* api_key, const char* tier);
void meteosource_destroy(Meteosource* meteosource);

typedef struct {
    bool isError;
	double min;
	double max;
} MinMaxTemperature;

MinMaxTemperature get_min_max_temparature_forecast(Meteosource* meteosource, const char* place_id, const char* sections, const char* timezone, const char* language, const char* units);

#ifdef  __cplusplus
}
#endif

#endif /* meteosource_c_h_ */