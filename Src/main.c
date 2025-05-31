/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body - POPRAWIONY
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "SSD1331.h"
#include "aht30.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define HISTORY_SIZE 100
#define HISTORY_PAUSE_TIME 30000  // 30 sekund pauzy podczas przeglądania historii
#define HISTORY_DISPLAY_TIME 800  // 800ms na wyświetlenie jednej pozycji historii

typedef enum {
    DISPLAY_TIME = 0,
    DISPLAY_TEMP,
    DISPLAY_HUM,
    DISPLAY_HIST_TEMP,
    DISPLAY_HIST_HUM,
    DISPLAY_AVG
} DisplayMode_t;

typedef struct{
	float temperatura;
	float humidity;
	uint32_t timestamp;
} Measurement;

typedef struct{
	Measurement data[HISTORY_SIZE];
	uint16_t head;
	uint16_t count;
} RingBuffer;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi3;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
float temp_threshold = 27.0;
float hum_threshold = 60.0;
#define ADC_BUF_LEN 2
uint16_t adc_buf[ADC_BUF_LEN]; // DMA wypełnia ten bufor
volatile uint8_t screenMode = 1;
RingBuffer history = {0};
float last_temp = 0, last_hum = 0;
float current_temp = 0, current_hum = 0; // Aktualne wartości dla wyświetlania
uint32_t last_measurement_tick = 0;
volatile uint32_t screenTimer = 0;
uint32_t last_avg_tick = 0;
int16_t history_index = 0;
DisplayMode_t current_display_mode = DISPLAY_TIME;
DisplayMode_t last_display_mode = DISPLAY_AVG; // Różny od aktualnego żeby wymusić odświeżenie

// Zmienne dla płynnej aktualizacji czasu
char last_time_str[16] = "";
char last_date_str[16] = "";
uint32_t last_time_update = 0;

// Nowe zmienne dla lepszej obsługi historii
uint32_t history_navigation_start = 0;  // Kiedy zaczęto przeglądać historię
uint32_t history_display_start = 0;     // Kiedy zaczęto wyświetlać aktualną pozycję
uint8_t history_browsing_active = 0;    // Czy obecnie przeglądamy historię
uint8_t history_position_changed = 0;   // Czy pozycja w historii się zmieniła
uint32_t last_joystick_move = 0;        // Ostatni ruch joystickiem

// **NOWE ZMIENNE DLA AUTOMATYCZNEJ ŚREDNIEJ**
uint8_t auto_avg_active = 0;            // Czy automatyczne wyświetlanie średniej jest aktywne
uint32_t auto_avg_start_time = 0;       // Kiedy rozpoczęto automatyczne wyświetlanie średniej
uint8_t screen_was_off_for_avg = 0;     // Czy ekran był wyłączony przed pokazaniem średniej
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI3_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);
static void MX_RTC_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */
void reset_screen_timer(uint32_t now) {
    screenTimer = now;
}

void add_measurement(RingBuffer *buf, float temp, float hum, uint32_t timestamp){
	buf->data[buf->head].temperatura = temp;
	buf->data[buf->head].humidity = hum;
	buf->data[buf->head].timestamp = timestamp;
	buf->head = (buf->head + 1) % HISTORY_SIZE;
	if (buf->count < HISTORY_SIZE) buf->count++;
}

Measurement get_measurement(const RingBuffer *buf, uint16_t index) {
    if (buf->count == 0) return (Measurement){0};
    if (index >= buf->count) index = buf->count - 1;
    uint16_t pos = (buf->head + HISTORY_SIZE - buf->count + index) % HISTORY_SIZE;
    return buf->data[pos];
}

void start_history_browsing(uint32_t now) {
    if (!history_browsing_active) {
        history_browsing_active = 1;
        history_navigation_start = now;
        history_display_start = now;
        history_position_changed = 1;
    }
}

void stop_history_browsing(void) {
    history_browsing_active = 0;
    history_navigation_start = 0;
    history_display_start = 0;
    history_position_changed = 0;
}

uint8_t should_pause_measurements(uint32_t now) {
    if (history_browsing_active) {
        return (now - history_navigation_start < HISTORY_PAUSE_TIME);
    }
    return 0;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_0 ) { // Debounce 200 ms
        if(screenMode == 0) { // Jeśli ekran był wyłączony
            screenMode = 1;
            SSD1331_DisplayON();
            screenTimer = HAL_GetTick(); // Resetuj timer przy KAŻDYM włączeniu
            last_display_mode = DISPLAY_AVG; // Wymuś odświeżenie

            // **ZATRZYMAJ AUTOMATYCZNĄ ŚREDNIĄ PRZY RĘCZNYM WŁĄCZENIU**
            if(auto_avg_active) {
                auto_avg_active = 0;
                current_display_mode = DISPLAY_TIME;
            }
        } else {
            screenMode = 0;
            SSD1331_DisplayOFF();
            // **ZATRZYMAJ AUTOMATYCZNĄ ŚREDNIĄ PRZY WYŁĄCZENIU**
            auto_avg_active = 0;
        }
    }
}

void check_power_save(uint32_t now){
    // **NIE WYŁĄCZAJ EKRANU PODCZAS AUTOMATYCZNEGO WYŚWIETLANIA ŚREDNIEJ**
    if(screenMode && !auto_avg_active && (now - screenTimer > 30000)){
        screenMode = 0;
        SSD1331_DisplayOFF();
    }
}

void get_time_date(char* time, char* date){
	RTC_DateTypeDef gDate;
	RTC_TimeTypeDef gTime;

	HAL_RTC_GetDate(&hrtc, &gDate, RTC_FORMAT_BIN);
	HAL_RTC_GetTime(&hrtc, &gTime, RTC_FORMAT_BIN);

	if(time)
		sprintf(time,"%02d:%02d:%02d", gTime.Hours, gTime.Minutes, gTime.Seconds);
	if(date)
		sprintf(date,"%02d-%02d-%2d", gDate.Date, gDate.Month, 2000+gDate.Year );
}

void display_time_date_smooth(void) {
    char time_str[16];
    char date_str[16];
    get_time_date(time_str, date_str);

    // Sprawdź czy czas się zmienił
    if (strcmp(time_str, last_time_str) != 0 || strcmp(date_str, last_date_str) != 0) {
        // Wyczyść tylko obszar gdzie jest tekst, nie cały ekran
        SSD1331_drawFrame(RGB_OLED_WIDTH/2 - 48, RGB_OLED_HEIGHT/2 - 10,
                         RGB_OLED_WIDTH/2 + 48, RGB_OLED_HEIGHT/2 + 25,
                         COLOR_BLACK, COLOR_BLACK);
        HAL_Delay(20); // Zmniejszone opóźnienie

        // Wyświetl nowy czas
        SSD1331_SetXY(RGB_OLED_WIDTH/2 - 48, RGB_OLED_HEIGHT/2 - 10);
        SSD1331_FStr(FONT_2X, (unsigned char*)time_str, COLOR_WHITE, COLOR_BLACK);
        SSD1331_SetXY(RGB_OLED_WIDTH/2 - 38, RGB_OLED_HEIGHT/2 +15);
        SSD1331_FStr(FONT_1X,  (unsigned char*)date_str , COLOR_WHITE, COLOR_BLACK);

        // Zapisz aktualny czas
        strcpy(last_time_str, time_str);
        strcpy(last_date_str, date_str);
    }
}

void display_temp(float temp) {
    char temp_str[16];
    SSD1331_drawFrame(0, 0, RGB_OLED_WIDTH - 1, RGB_OLED_HEIGHT - 1, COLOR_BLACK, COLOR_BLACK);
    HAL_Delay(10);
    SSD1331_SetXY(RGB_OLED_WIDTH/2 - 40, RGB_OLED_HEIGHT/2 - 10);
    SSD1331_FStr(FONT_1X, (unsigned char*)"Temperatura :", COLOR_RED, COLOR_BLACK);
    sprintf(temp_str, "%.2f st. C", temp);
    SSD1331_SetXY(RGB_OLED_WIDTH/2 - 30, RGB_OLED_HEIGHT/2 +6);
    SSD1331_FStr(FONT_1X, (unsigned char*)temp_str, COLOR_RED, COLOR_BLACK);
}

void display_hum(float hum) {
    char hum_str[16];
    SSD1331_drawFrame(0, 0, RGB_OLED_WIDTH - 1, RGB_OLED_HEIGHT - 1, COLOR_BLACK, COLOR_BLACK);
    HAL_Delay(10);
    SSD1331_SetXY(RGB_OLED_WIDTH/2 - 32, RGB_OLED_HEIGHT/2 - 10);
    SSD1331_FStr(FONT_1X, (unsigned char*)"Wilgotnosc :", COLOR_BLUE, COLOR_BLACK);
    sprintf(hum_str, "%.2f %%", hum);
    SSD1331_SetXY(RGB_OLED_WIDTH/2 - 35, RGB_OLED_HEIGHT/2 +6);
    SSD1331_FStr(FONT_1X, (unsigned char*)hum_str, COLOR_BLUE, COLOR_BLACK);
}

void display_avg(const RingBuffer *buf) {
    if(buf->count == 0) return;
    float avg_temp = 0, avg_hum = 0;
    for(uint16_t i=0; i<buf->count; i++) {
        Measurement m = get_measurement(buf, i);
        avg_temp += m.temperatura;
        avg_hum += m.humidity;
    }
    avg_temp /= buf->count;
    avg_hum /= buf->count;
    char avg_str[80];

    SSD1331_drawFrame(0, 0, RGB_OLED_WIDTH - 1, RGB_OLED_HEIGHT - 1, COLOR_BLACK, COLOR_BLACK);
    HAL_Delay(10);

    // **DODAJ NAGŁÓWEK POKAZUJĄCY ŻE TO AUTOMATYCZNA ŚREDNIA**
    if(auto_avg_active) {
        SSD1331_SetXY(5, 5);
        SSD1331_FStr(FONT_1X, (unsigned char*)"AUTO SREDNIA", COLOR_GREEN, COLOR_BLACK);
    }

    sprintf(avg_str, "Srednia T: %.2fC", avg_temp);
    SSD1331_SetXY(0, auto_avg_active ? 20 : 15);
    SSD1331_FStr(FONT_1X, (unsigned char*)avg_str, COLOR_YELLOW, COLOR_BLACK);
    sprintf(avg_str, "Srednia W: %.2f%%", avg_hum);
    SSD1331_SetXY(0, auto_avg_active ? 35 : 30);
    SSD1331_FStr(FONT_1X, (unsigned char*)avg_str, COLOR_YELLOW, COLOR_BLACK);

    // **DODAJ INFORMACJĘ O CZASIE POZOSTAŁYM**
    if(auto_avg_active) {
        uint32_t remaining = 6000 - (HAL_GetTick() - auto_avg_start_time);
        if(remaining > 6000) remaining = 0; // Zabezpieczenie przed overflow
        sprintf(avg_str, "Pozostalo: %lus", remaining/1000 + 1);
        SSD1331_SetXY(5, 50);
        SSD1331_FStr(FONT_1X, (unsigned char*)avg_str, COLOR_WHITE, COLOR_BLACK);
    }
    HAL_Delay(800);
}

void display_history_temp(const RingBuffer *buf, int16_t idx) {
    if(buf->count == 0) return;

    // Upewnij się, że indeks jest w poprawnym zakresie
    if(idx >= buf->count) idx = buf->count - 1;
    if(idx < 0) idx = 0;

    Measurement m = get_measurement(buf, idx);
    char hist_str[60];
    char line1[30], line2[30], line3[30];

    // Wyświetl informację o pozycji: najnowszy = 1, najstarszy = count
    int display_pos = buf->count - idx; // Odwróć dla intuicyjnego wyświetlania

    sprintf(line1, "HISTORIA TEMP [%d/%d]", display_pos, buf->count);
    sprintf(line2, "Temp: %.2f C", m.temperatura);
    sprintf(line3, "Czas: %lu s", m.timestamp);

    SSD1331_drawFrame(0, 0, RGB_OLED_WIDTH - 1, RGB_OLED_HEIGHT - 1, COLOR_BLACK, COLOR_BLACK);
    HAL_Delay(10);

    SSD1331_SetXY(2, 5);
    SSD1331_FStr(FONT_1X, (unsigned char*)line1, COLOR_GREEN, COLOR_BLACK);
    SSD1331_SetXY(2, 20);
    SSD1331_FStr(FONT_1X, (unsigned char*)line2, COLOR_GREEN, COLOR_BLACK);
    SSD1331_SetXY(2, 35);
    SSD1331_FStr(FONT_1X, (unsigned char*)line3, COLOR_GREEN, COLOR_BLACK);
    HAL_Delay(800);
}

void display_history_hum(const RingBuffer *buf, int16_t idx) {
    if(buf->count == 0) return;

    // Upewnij się, że indeks jest w poprawnym zakresie
    if(idx >= buf->count) idx = buf->count - 1;
    if(idx < 0) idx = 0;

    Measurement m = get_measurement(buf, idx);
    char line1[30], line2[30], line3[30];

    // Wyświetl informację o pozycji: najnowszy = 1, najstarszy = count
    int display_pos = buf->count - idx; // Odwróć dla intuicyjnego wyświetlania

    sprintf(line1, "HISTORIA WILG [%d/%d]", display_pos, buf->count);
    sprintf(line2, "Wilg: %.2f %%", m.humidity);
    sprintf(line3, "Czas: %lu s", m.timestamp);

    SSD1331_drawFrame(0, 0, RGB_OLED_WIDTH - 1, RGB_OLED_HEIGHT - 1, COLOR_BLACK, COLOR_BLACK);
    HAL_Delay(10);

    SSD1331_SetXY(2, 5);
    SSD1331_FStr(FONT_1X, (unsigned char*)line1, COLOR_CYAN, COLOR_BLACK);
    SSD1331_SetXY(2, 20);
    SSD1331_FStr(FONT_1X, (unsigned char*)line2, COLOR_CYAN, COLOR_BLACK);
    SSD1331_SetXY(2, 35);
    SSD1331_FStr(FONT_1X, (unsigned char*)line3, COLOR_CYAN, COLOR_BLACK);
    HAL_Delay(800);
}

void update_current_readings(void) {
    // Aktualizuj aktualne odczyty dla wyświetlania (nie zapisuj do historii)
    if(AHT30_Read(&current_temp, &current_hum) != HAL_OK) {
        // Jeśli nie udało się odczytać, użyj ostatnich wartości
        current_temp = last_temp;
        current_hum = last_hum;
    }
}

uint8_t is_joystick_in_neutral_position(uint16_t x, uint16_t y) {
    return (x >= 1500 && x <= 2500 && y >= 1500 && y <= 2500);
}

// **NOWA FUNKCJA - URUCHOM AUTOMATYCZNE WYŚWIETLANIE ŚREDNIEJ**
void start_auto_avg_display(uint32_t now) {
    auto_avg_active = 1;
    auto_avg_start_time = now;

    // Sprawdź czy ekran był wyłączony
    screen_was_off_for_avg = (screenMode == 0);

    // Włącz ekran jeśli był wyłączony
    if(screenMode == 0) {
        screenMode = 1;
        SSD1331_DisplayON();
    }

    // Zatrzymaj przeglądanie historii
    stop_history_browsing();

    // Przełącz na tryb średniej
    current_display_mode = DISPLAY_AVG;
    last_display_mode = DISPLAY_TIME; // Wymuś odświeżenie

    // Resetuj timer ekranu żeby nie wyłączył się podczas wyświetlania
    screenTimer = now;
}

// **NOWA FUNKCJA - ZATRZYMAJ AUTOMATYCZNE WYŚWIETLANIE ŚREDNIEJ**
void stop_auto_avg_display(void) {
    auto_avg_active = 0;

    // Jeśli ekran był wyłączony przed pokazaniem średniej, wyłącz go ponownie
    if(screen_was_off_for_avg) {
        screenMode = 0;
        SSD1331_DisplayOFF();
    } else {
        // Jeśli ekran był włączony, wróć do zegara
        current_display_mode = DISPLAY_TIME;
        last_display_mode = DISPLAY_AVG; // Wymuś odświeżenie
        screenTimer = HAL_GetTick(); // Resetuj timer
    }

    screen_was_off_for_avg = 0;
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_SPI3_Init();
  MX_I2C1_Init();
  MX_ADC1_Init();
  MX_RTC_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buf, ADC_BUF_LEN);
  HAL_Delay(100);

  // Inicjalizacja AHT30
  AHT30_Init();
  HAL_Delay(100);

  //Inicjalizacja konfiguracji ekranu
  SSD1331_init();
  HAL_Delay(100);
  //Wypelnienie calego ekranu na czarno
  SSD1331_drawFrame(0, 0, RGB_OLED_WIDTH - 1, RGB_OLED_HEIGHT - 1, COLOR_BLACK, COLOR_BLACK);
  HAL_Delay(150);
  //Wyswietlenie wiadomosci powitalnej
  SSD1331_SetXY(RGB_OLED_WIDTH/2 - 40, RGB_OLED_HEIGHT/2 - 10);
  char *wiadomosc = "WELCOME";
  SSD1331_FStr(FONT_2X, (unsigned char*) wiadomosc, COLOR_WHITE, COLOR_BLACK);
  SSD1331_SetXY(RGB_OLED_WIDTH/2 - 38, RGB_OLED_HEIGHT/2 +15);
  char *wiadomosc2 = "Projekt KZ,KT";
  SSD1331_FStr(FONT_1X, (unsigned char*) wiadomosc2, COLOR_WHITE, COLOR_BLACK);
  HAL_Delay(3000);
  SSD1331_drawFrame(0, 0, RGB_OLED_WIDTH - 1, RGB_OLED_HEIGHT - 1, COLOR_BLACK, COLOR_BLACK);
  HAL_Delay(100);

  //Deklarowanie zmiennych odnosnie kontroli ekranu
  screenMode = 1;
  screenTimer = HAL_GetTick();

  // Początkowy odczyt temperatury i wilgotności
  if(AHT30_Read(&current_temp, &current_hum) == HAL_OK) {
      last_temp = current_temp;
      last_hum = current_hum;
  }

  // **USTAWIENIE POCZĄTKOWEGO CZASU DLA ŚREDNIEJ (dla testów - 30 sekund)**
  last_avg_tick = 0; // Odejmij 4 minuty, żeby pierwsza średnia pojawiła się za 30s

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  uint32_t now;
  uint32_t led_tick = 0;
  uint8_t led_temp_state = 0;
  uint8_t led_hum_state = 0;
  uint8_t time_display_flag = 1;

  while (1)
  {
      now = HAL_GetTick();

      check_power_save(now);

      // **SCHEDULER: AUTOMATYCZNA ŚREDNIA CO 4.5 MINUTY (270 sekund)**
      if(now - last_avg_tick >= 50000) {
          start_auto_avg_display(now);
          last_avg_tick = now;
      }

      // **SPRAWDŹ CZY ZAKOŃCZYĆ AUTOMATYCZNE WYŚWIETLANIE ŚREDNIEJ**
      if(auto_avg_active && (now - auto_avg_start_time >= 6000)) {
    	  SSD1331_drawFrame(0, 0, RGB_OLED_WIDTH - 1, RGB_OLED_HEIGHT - 1, COLOR_BLACK, COLOR_BLACK);
    	  HAL_Delay(20);
          stop_auto_avg_display();
      }

      // **JEŚLI AUTOMATYCZNA ŚREDNIA JEST AKTYWNA, POKAŻ JĄ I POMIŃ RESZTĘ**
      if(auto_avg_active) {
          if(current_display_mode != last_display_mode) {
              last_display_mode = current_display_mode;
          }
          display_avg(&history);
          HAL_Delay(50);
          continue; // Pomiń resztę logiki
      }

      if(!screenMode) continue; // ekran wyłączony

      // Scheduler: pomiar do historii co 15s (ale tylko jeśli nie przeglądamy historii)
      if((now - last_measurement_tick >= 15000) && !should_pause_measurements(now)) {
          float temp, hum;
          if(AHT30_Read(&temp, &hum) == HAL_OK) {
              last_temp = temp;
              last_hum = hum;
              add_measurement(&history, temp, hum, now/1000);
          }
          last_measurement_tick = now;
      }

      // Odczyt joysticka
      uint16_t x = adc_buf[0];
      uint16_t y = adc_buf[1];

      // Logika joysticka
      if(x < 1000) {
            time_display_flag = 0;
            if(current_display_mode != DISPLAY_TEMP || now - last_time_update > 1000) {
                update_current_readings();
                current_display_mode = DISPLAY_TEMP;
                stop_history_browsing(); // Zatrzymaj przeglądanie historii

                // Obsługa LED dla temperatury
                if (current_temp > temp_threshold) {
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
                } else if (current_temp < temp_threshold) {
                    led_temp_state = !led_temp_state;
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, led_temp_state);
                } else {
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
                }
            }
            reset_screen_timer(now);

        } else if(x > 3000) {
            time_display_flag = 0;
            if(current_display_mode != DISPLAY_HUM || now - last_time_update > 1000) {
                update_current_readings();
                current_display_mode = DISPLAY_HUM;
                stop_history_browsing(); // Zatrzymaj przeglądanie historii

                // Obsługa LED dla wilgotności
                if (current_hum > hum_threshold) {
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
                } else if (current_hum < hum_threshold) {
                    led_hum_state = !led_hum_state;
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, led_hum_state);
                } else {
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
                }
            }
            reset_screen_timer(now);

         } else if(y < 1000) {
                  // Przeglądanie historii wilgotności - W DÓŁ = od najnowszych do najstarszych
                  time_display_flag = 0;

                  // Reset indeksu jeśli wchodzimy do trybu historii po raz pierwszy lub po przerwie
                  if(current_display_mode != DISPLAY_HIST_HUM) {
                      history_index = 0; // Zacznij od najnowszego pomiaru
                      history_position_changed = 1;
                      history_display_start = now; // Ustaw czas wyświetlania
                      start_history_browsing(now);
                  } else {
                      // Tylko jeśli już jesteśmy w trybie historii, pozwól na nawigację
                      if(now - last_joystick_move > 500) { // Opóźnienie między ruchami
                          if(history_index < history.count-1) {
                              history_index++; // Przejdź do starszego pomiaru
                              history_position_changed = 1;
                              history_display_start = now;
                          }
                          last_joystick_move = now;
                      }
                  }
                  current_display_mode = DISPLAY_HIST_HUM;
                  reset_screen_timer(now);

              } else if(y > 3000) {
                  // Przeglądanie historii temperatury - W GÓRĘ = od najnowszych do najstarszych
                  time_display_flag = 0;

                  // Reset indeksu jeśli wchodzimy do trybu historii po raz pierwszy lub po przerwie
                  if(current_display_mode != DISPLAY_HIST_TEMP) {
                      history_index = 0; // Zacznij od najnowszego pomiaru
                      history_position_changed = 1;
                      history_display_start = now; // Ustaw czas wyświetlania
                      start_history_browsing(now);
                  } else {
                      // Tylko jeśli już jesteśmy w trybie historii, pozwól na nawigację
                      if(now - last_joystick_move > 500) { // Opóźnienie między ruchami
                          if(history_index < history.count-1) {
                              history_index++; // Przejdź do starszego pomiaru
                              history_position_changed = 1;
                              history_display_start = now;
                          }
                          last_joystick_move = now;
                      }
                  }
                  current_display_mode = DISPLAY_HIST_TEMP;
                  reset_screen_timer(now);
                    } else {
            // POZYCJA NEUTRALNA
            if(is_joystick_in_neutral_position(x, y)) {
                if(history_browsing_active) {
                    // Jeśli przeglądamy historię, zatrzymaj po powrocie do neutralnej pozycji
                    if(now - history_navigation_start > 1000) { // Daj czas na stabilizację
                        stop_history_browsing();
                        current_display_mode = DISPLAY_TIME;
                    }
                } else {
                    current_display_mode = DISPLAY_TIME;
                }
            }
        }

      // Sprawdź czy automatycznie zatrzymać przeglądanie historii
      if(history_browsing_active && (now - history_navigation_start > HISTORY_PAUSE_TIME)) {
          stop_history_browsing();
          if(current_display_mode == DISPLAY_HIST_TEMP || current_display_mode == DISPLAY_HIST_HUM) {
              current_display_mode = DISPLAY_TIME;
          }
      }

      // Wyświetlanie na podstawie aktualnego trybu
      if(current_display_mode != last_display_mode) {
                // Tryb się zmienił, wyczyść ekran (oprócz płynnego czasu)
                if(current_display_mode != DISPLAY_TIME) {
                    SSD1331_drawFrame(0, 0, RGB_OLED_WIDTH - 1, RGB_OLED_HEIGHT - 1, COLOR_BLACK, COLOR_BLACK);
                    HAL_Delay(20);
                } else {
                    // Reset dla płynnego czasu
                    strcpy(last_time_str, "");
                    strcpy(last_date_str, "");
                }
                last_display_mode = current_display_mode;
      }

      switch(current_display_mode) {
          case DISPLAY_TIME:
        	  if(time_display_flag == 0){
        		  SSD1331_drawFrame(0, 0, RGB_OLED_WIDTH - 1, RGB_OLED_HEIGHT - 1, COLOR_BLACK, COLOR_BLACK);
        		  HAL_Delay(20);
        		  time_display_flag = 1;
        	  }
              display_time_date_smooth();
              break;

          case DISPLAY_TEMP:
              if(now - last_time_update > 1000) { // Aktualizuj co sekundę
                  update_current_readings();
                  display_temp(current_temp);
                  last_time_update = now;
              }
              break;

          case DISPLAY_HUM:
              if(now - last_time_update > 1000) { // Aktualizuj co sekundę
                  update_current_readings();
                  display_hum(current_hum);
                  last_time_update = now;
              }
              break;

          case DISPLAY_HIST_TEMP:
                       // Wyświetl pozycję przez określony czas lub jeśli pozycja się zmieniła
                       if(history_position_changed || (now - history_display_start > HISTORY_DISPLAY_TIME)) {
                           display_history_temp(&history, history_index);
                           history_position_changed = 0;
                           if(now - history_display_start > HISTORY_DISPLAY_TIME) {
                               history_display_start = now;
                           }
                       }
                       break;

                   case DISPLAY_HIST_HUM:
                       // Wyświetl pozycję przez określony czas lub jeśli pozycja się zmieniła
                       if(history_position_changed || (now - history_display_start > HISTORY_DISPLAY_TIME)) {
                           display_history_hum(&history, history_index);
                           history_position_changed = 0;
                           if(now - history_display_start > HISTORY_DISPLAY_TIME) {
                               history_display_start = now;
                           }
                       }
                       break;

          case DISPLAY_AVG:
        	  time_display_flag = 0;
              display_avg(&history);
              break;
      }

      HAL_Delay(50); // Zmniejszone opóźnienie dla płynniejszego działania
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1|RCC_PERIPHCLK_RTC;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 2;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_7;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.SamplingTime = ADC_SAMPLETIME_181CYCLES_5;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_8;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00201D2B;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 0x1;
  sDate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SPI3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI3_Init(void)
{

  /* USER CODE BEGIN SPI3_Init 0 */

  /* USER CODE END SPI3_Init 0 */

  /* USER CODE BEGIN SPI3_Init 1 */

  /* USER CODE END SPI3_Init 1 */
  /* SPI3 parameter configuration*/
  hspi3.Instance = SPI3;
  hspi3.Init.Mode = SPI_MODE_MASTER;
  hspi3.Init.Direction = SPI_DIRECTION_2LINES;
  hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi3.Init.NSS = SPI_NSS_SOFT;
  hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi3.Init.CRCPolynomial = 7;
  hspi3.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi3.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI3_Init 2 */

  /* USER CODE END SPI3_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 38400;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, CS_Pin|RESET_Pin|DC_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PC0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : CS_Pin RESET_Pin DC_Pin */
  GPIO_InitStruct.Pin = CS_Pin|RESET_Pin|DC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
