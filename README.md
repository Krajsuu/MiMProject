Interaktywna Stacja Temperaturowa Pomieszczenia
Projekt zrealizowany na mikrokontrolerze STM32 Nucleo F302R8, napisany w języku C z wykorzystaniem środowiska STM32CubeIDE.

Opis projektu
Aplikacja służy do pomiaru warunków środowiskowych w pomieszczeniu. Wykorzystuje czujnik AHT30 do odczytu temperatury i wilgotności, 
a wyniki prezentowane są na kolorowym wyświetlaczu SSD1331. Obsługa interfejsu odbywa się za pomocą 3-osiowego joysticka analogowego.

Funkcjonalności
Czujnik AHT30
Pomiar temperatury i wilgotności z otoczenia,
Przesyłanie danych do mikrokontrolera za pomocą magistrali I2C,
Wyniki prezentowane na ekranie OLED.

Dioda LED (wbudowana w płytkę)
Sygnalizuje stan temperatury:
Dioda zgaszona – temperatura jest pokojowa,
Dioda świeci – temperatura powyżej pokojowej,
Dioda miga – temperatura poniżej pokojowej.

Wyświetlacz SSD1331
Wyświetlanie bieżących wartości temperatury i wilgotności,
Prezentacja historii pomiarów,
Wyświetlanie średniej z ostatnich 100 pomiarów co 50 sekund,
Automatyczne wyłączanie ekranu po 30 sekundach braku aktywności.

Obsługa joysticka
Przesunięcie w górę – widok temperatury,
Przesunięcie w dół – widok wilgotności,
Przesunięcie w lewo – historia temperatury,
Przesunięcie w prawo – historia wilgotności,
Wciśnięcie joysticka – włączenie/wyłączenie ekranu (obsługa przerwania).

Technologie
Język programowania: C
Środowisko: STM32CubeIDE
Mikrokontroler: STM32F302R8 (Nucleo)

Wykorzystywane komponenty:
Czujnik AHT30 (I2C),
Wyświetlacz SSD1331 (SPI),
Joystick analogowy (ADC),
Dioda LED (GPIO).

Struktura projektu
/Core
├── Inc/
│   ├── SSD1331.h  
│   ├── aht30.h   
│   ├── main.h
│   ├── stm32f3xx_hal_conf.h
│   └── stm32f3xx_it.h
├── Src/
│   ├── SSD1331.c  - implementuje obsługe ekranu SDD1331
│   ├── aht30.c   - implementuje obsługe czujnika AHT30
│   ├── main.c
│   ├── stm32f3xx_hal_msp.c
│   ├── stm32f3xx_it.c
│   ├── syscalls.c
│   ├── sysmem.c
│   └── system_stm32f3xx.c
