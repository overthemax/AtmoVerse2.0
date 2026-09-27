/**
 * @file WeatherIconMap.cpp
 * @brief Weather condition -> icon (see WeatherIconMap.h)
 */
#include "WeatherIconMap.h"

WeatherIcon weatherIconFor(int weatherID, bool isNight, float windSpeed) {
  /*
   * Codici meteo principali OpenWeatherMap:
   * Gruppo 2xx: Temporale (200-232)
   * Gruppo 3xx: Pioggerella (300-321)
   * Gruppo 5xx: Pioggia (500-531)
   * Gruppo 6xx: Neve (600-622)
   * Gruppo 7xx: Atmosfera (701-781: nebbia, foschia, polvere, sabbia, cenere, tornado)
   * Gruppo 800: Cielo sereno
   * Gruppo 80x: Nuvolosità (801-804)
   * Gruppo 90x: Eventi estremi (900-906: tornado, uragano, freddo/caldo estremo, vento forte)
   */
  
  // Temporale (200-232)
  if (weatherID >= 200 && weatherID < 300) {
    // Temporale con pioggia leggera/moderata (200-202, 230-232)
    if (weatherID <= 202 || weatherID >= 230) {
      return isNight ? ICON_NIGHT_THUNDERSTORM : ICON_DAY_THUNDERSTORM;
    }
    // Temporale intenso (210-221)
    else {
      return isNight ? ICON_NIGHT_STORM_SHOWERS : ICON_DAY_STORM_SHOWERS;
    }
  }
  
  // Pioggerella (300-321)
  else if (weatherID >= 300 && weatherID < 400) {
    return isNight ? ICON_NIGHT_SPRINKLE : ICON_DAY_SPRINKLE;
  }
  
  // Pioggia (500-531)
  else if (weatherID >= 500 && weatherID < 600) {
    // Pioggia leggera (500-501, 520)
    if (weatherID <= 501 || weatherID == 520) {
      return isNight ? ICON_NIGHT_SPRINKLE : ICON_DAY_SPRINKLE;
    }
    // Pioggia moderata/forte (502-504, 521-522, 531)
    else if (weatherID <= 504 || weatherID == 521 || weatherID == 522 || weatherID == 531) {
      return isNight ? ICON_NIGHT_RAIN : ICON_DAY_RAIN;
    }
    // Pioggia molto forte (511: freezing rain)
    else if (weatherID == 511) {
      return isNight ? ICON_NIGHT_SLEET : ICON_DAY_SLEET;
    }
    // Acquazzoni (520-531)
    else {
      return isNight ? ICON_NIGHT_SHOWERS : ICON_DAY_SHOWERS;
    }
  }
  
  // Neve (600-622)
  else if (weatherID >= 600 && weatherID < 700) {
    // Neve leggera (600)
    if (weatherID == 600) {
      return isNight ? ICON_NIGHT_SNOW : ICON_DAY_SNOW;
    }
    // Neve (601)
    else if (weatherID == 601) {
      return isNight ? ICON_NIGHT_SNOW : ICON_DAY_SNOW;
    }
    // Neve intensa (602)
    else if (weatherID == 602) {
      return ICON_SNOWFLAKE_COLD; // Icona neutra per neve molto intensa
    }
    // Sleet/Nevischio (611-615)
    else if (weatherID >= 611 && weatherID <= 615) {
      return isNight ? ICON_NIGHT_SLEET : ICON_DAY_SLEET;
    }
    // Sleet con temporale (616)
    else if (weatherID == 616) {
      return isNight ? ICON_NIGHT_SLEET_STORM : ICON_DAY_SLEET_STORM;
    }
    // Neve con temporale (620-621)
    else if (weatherID == 620 || weatherID == 621) {
      return isNight ? ICON_NIGHT_SNOW_THUNDERSTORM : ICON_DAY_SNOW_THUNDERSTORM;
    }
    // Neve intensa con temporale (622)
    else if (weatherID == 622) {
      return isNight ? ICON_NIGHT_SNOW_THUNDERSTORM : ICON_DAY_SNOW_THUNDERSTORM;
    }
    else {
      return isNight ? ICON_NIGHT_SNOW : ICON_DAY_SNOW;
    }
  }
  
  // Atmosfera (700-781)
  else if (weatherID >= 700 && weatherID < 800) {
    // Nebbia/Foschia (701, 741)
    if (weatherID == 701 || weatherID == 741) {
      return isNight ? ICON_NIGHT_FOG : ICON_DAY_FOG;
    }
    // Fumo (711)
    else if (weatherID == 711) {
      return ICON_SMOKE;
    }
    // Foschia/Caligine (721)
    else if (weatherID == 721) {
      return ICON_HAZE;
    }
    // Polvere (731, 761)
    else if (weatherID == 731 || weatherID == 761) {
      return ICON_DUST;
    }
    // Sabbia (751)
    else if (weatherID == 751) {
      return ICON_SANDSTORM;
    }
    // Cenere vulcanica (762)
    else if (weatherID == 762) {
      return ICON_VOLCANO;
    }
    // Tornado (781)
    else if (weatherID == 781) {
      return ICON_TORNADO;
    }
    // Altri fenomeni atmosferici
    else {
      return ICON_FOG; // Icona neutra nebbia
    }
  }
  
  // Cielo sereno (800)
  else if (weatherID == 800) {
    return isNight ? ICON_NIGHT_CLEAR : ICON_DAY_SUNNY;
  }
  
  // Nuvolosità (801-804)
  else if (weatherID > 800 && weatherID < 900) {
    // Controlla se c'è vento forte (> 30 km/h) per icone varianti
    bool strongWind = windSpeed > 30.0f;
    bool veryStrongWind = windSpeed > 50.0f;
    
    // Poche nuvole (801-802)
    if (weatherID <= 802) {
      if (veryStrongWind) {
        return isNight ? ICON_NIGHT_CLOUDY_GUSTS : ICON_DAY_CLOUDY_GUSTS;
      } else if (strongWind) {
        return isNight ? ICON_NIGHT_CLOUDY_WINDY : ICON_DAY_CLOUDY_WINDY;
      }
      return isNight ? ICON_NIGHT_CLOUDY : ICON_DAY_CLOUDY;
    }
    // Nubi sparse/coperto (803-804)
    else {
      if (strongWind) {
        return ICON_CLOUDY_WINDY;
      }
      return ICON_CLOUDY; // Icona neutra nuvoloso
    }
  }
  
  // Condizioni estreme (900-906) - Estensioni API OpenWeatherMap
  else if (weatherID >= 900 && weatherID < 910) {
    // Tornado (900)
    if (weatherID == 900) {
      return ICON_TORNADO;
    }
    // Tempesta tropicale (901)
    else if (weatherID == 901) {
      return ICON_HURRICANE;
    }
    // Uragano (902)
    else if (weatherID == 902) {
      return ICON_HURRICANE;
    }
    // Freddo estremo (903)
    else if (weatherID == 903) {
      return ICON_SNOWFLAKE_COLD;
    }
    // Caldo estremo (904)
    else if (weatherID == 904) {
      return ICON_HOT;
    }
    // Ventoso (905-906)
    else if (weatherID >= 905 && weatherID <= 906) {
      return ICON_WINDY;
    }
  }
  
  // Valore di default
  return isNight ? ICON_NIGHT_CLEAR : ICON_DAY_SUNNY;
}

const char* weatherIconPath(WeatherIcon icon) {
  // Converte il percorso SVG in BMP (usa la cartella /icons/ e .bmp)
  switch(icon) {
    // GIORNO - Condizioni Base
    case ICON_DAY_SUNNY: return "/icons/wi-day-sunny.bmp";
    case ICON_DAY_CLOUDY: return "/icons/wi-day-cloudy.bmp";
    case ICON_DAY_FOG: return "/icons/wi-day-fog.bmp";
    case ICON_DAY_WINDY: return "/icons/wi-day-windy.bmp";
    
    // GIORNO - Pioggia
    case ICON_DAY_RAIN: return "/icons/wi-day-rain.bmp";
    case ICON_DAY_RAIN_WIND: return "/icons/wi-day-rain-wind.bmp";
    case ICON_DAY_SPRINKLE: return "/icons/wi-day-sprinkle.bmp";
    case ICON_DAY_SHOWERS: return "/icons/wi-day-showers.bmp";
    
    // GIORNO - Temporali
    case ICON_DAY_THUNDERSTORM: return "/icons/wi-day-thunderstorm.bmp";
    case ICON_DAY_STORM_SHOWERS: return "/icons/wi-day-storm-showers.bmp";
    case ICON_DAY_LIGHTNING: return "/icons/wi-day-lightning.bmp";
    
    // GIORNO - Neve
    case ICON_DAY_SNOW: return "/icons/wi-day-snow.bmp";
    case ICON_DAY_SNOW_WIND: return "/icons/wi-day-snow-wind.bmp";
    case ICON_DAY_SLEET: return "/icons/wi-day-sleet.bmp";
    case ICON_DAY_RAIN_MIX: return "/icons/wi-day-rain-mix.bmp";
    case ICON_DAY_HAIL: return "/icons/wi-day-hail.bmp";
    
    // NOTTE - Condizioni Base
    case ICON_NIGHT_CLEAR: return "/icons/wi-night-clear.bmp";
    case ICON_NIGHT_CLOUDY: return "/icons/wi-night-alt-cloudy.bmp";
    case ICON_NIGHT_FOG: return "/icons/wi-night-fog.bmp";
    
    // NOTTE - Pioggia
    case ICON_NIGHT_RAIN: return "/icons/wi-night-alt-rain.bmp";
    case ICON_NIGHT_RAIN_WIND: return "/icons/wi-night-alt-rain-wind.bmp";
    case ICON_NIGHT_SPRINKLE: return "/icons/wi-night-alt-sprinkle.bmp";
    case ICON_NIGHT_SHOWERS: return "/icons/wi-night-alt-showers.bmp";
    
    // NOTTE - Temporali
    case ICON_NIGHT_THUNDERSTORM: return "/icons/wi-night-alt-thunderstorm.bmp";
    case ICON_NIGHT_STORM_SHOWERS: return "/icons/wi-night-alt-storm-showers.bmp";
    
    // NOTTE - Neve
    case ICON_NIGHT_SNOW: return "/icons/wi-night-alt-snow.bmp";
    case ICON_NIGHT_SNOW_WIND: return "/icons/wi-night-alt-snow-wind.bmp";
    case ICON_NIGHT_SLEET: return "/icons/wi-night-alt-sleet.bmp";
    case ICON_NIGHT_RAIN_MIX: return "/icons/wi-night-alt-rain-mix.bmp";
    case ICON_NIGHT_HAIL: return "/icons/wi-night-alt-hail.bmp";
    
    // NEUTRO
    case ICON_CLOUDY: return "/icons/wi-cloudy.bmp";
    case ICON_RAIN: return "/icons/wi-rain.bmp";
    case ICON_SNOW: return "/icons/wi-snow.bmp";
    case ICON_THUNDERSTORM: return "/icons/wi-thunderstorm.bmp";
    case ICON_FOG: return "/icons/wi-fog.bmp";
    case ICON_WINDY: return "/icons/wi-windy.bmp";
    case ICON_TORNADO: return "/icons/wi-tornado.bmp";
    case ICON_HURRICANE: return "/icons/wi-hurricane.bmp";
    case ICON_HOT: return "/icons/wi-hot.bmp";
    case ICON_SNOWFLAKE_COLD: return "/icons/wi-snowflake-cold.bmp";
    
    // Default
    default: return "/icons/wi-day-sunny.bmp";
  }
}
