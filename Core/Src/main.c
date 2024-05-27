/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdbool.h>
#include "ILI9341_STM32_Driver.h"

#include "ILI9341_GFX.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim14;
TIM_HandleTypeDef htim16;
TIM_HandleTypeDef htim17;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

enum MainModes {WATCH, GAME};
enum ClockModes {NORMAL, SET_TIME, SET_ALARM};
enum Timeset {SET_MINUTES, SET_HOURS};
enum GameModes {START, GAMING, GAME_OVER, GAME_WIN};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_ADC_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM14_Init(void);
static void MX_TIM16_Init(void);
static void MX_TIM17_Init(void);
/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Set time defines
#define LOOP_LIMIT 4

// Boder defines
#define LEFT_BORDER 10
#define RIGHT_BORDER 230
#define TOP_BORDER 30
#define TOP_BORDER_HEIGHT 10

// Platform defines
#define PLATFORM_LINE 270
#define DEFAULT_PLATFORM_DIRECTION 7
#define PLATFORM_LINE_THICKNESS 1

// Block defines
#define TOP_ROW    60
#define BOTTOM_ROW 140
#define LEFT_COLUMN LEFT_BORDER
#define RIGHT_COLUMN RIGHT_BORDER

#define ROW_HEIGHT 20
#define COLUMN_WIDTH 44

#define N_ROWS ((BOTTOM_ROW - TOP_ROW) / ROW_HEIGHT)
#define N_COLS ((RIGHT_COLUMN - LEFT_COLUMN) / COLUMN_WIDTH)

#define BLOCK_LINE_THICKNESS 1
#define N_LEVELS 3

// Battery defines
uint16_t battery_thresholds[3] = {1986, 2020, 3000};
uint8_t battery_level;

// Game block states
bool block_states [N_LEVELS][N_COLS*N_ROWS] = {{1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1},
											   {0,1,0,1,0, 1,1,1,1,1, 0,1,1,1,0, 0,0,1,0,0},
											   {1,0,0,0,1, 1,0,0,0,1, 1,1,0,1,1, 1,1,1,1,1}};
//bool block_states [N_LEVELS][N_COLS*N_ROWS] = {{0,0,0,0,0, 0,0,0,0,0, 0,1,0,0,0, 0,0,0,0,0},
//											   {0,1,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0},
//											   {1,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0}};
bool current_block_states [N_COLS*N_ROWS];

int RowColors [] = {YELLOW, CYAN, PINK, GREEN};

struct time current_time = {{1,1,1,1,1,1},{0, 0, 0, 0, 0, 0}};
struct time alarm_time = {{1,1,1,1,1,1}, {0, 0, 0, 0, 1, 0}};
struct time *time_to_set;

// Main state machine
uint8_t current_main_state;
uint8_t next_main_state;

// Button debouncing states
bool btn_1_is_debounced;
bool btn_2_is_debounced;

// Clock state machine
uint8_t current_clock_state;
uint8_t next_clock_state;

// Game state machine
uint8_t current_game_state;
uint8_t next_game_state;

// Set values state machine
uint8_t current_set_state;
uint8_t next_set_state;
uint8_t n_loops = 0;

// Blink things
bool blink = 0;

// Game ball
int16_t ball_position[2] = {120,160};
int8_t ball_direction[2] = {0,0};
uint8_t ball_size = 4;

// Platform
uint8_t platform_position = 100;
int8_t platform_direction = 0;
uint8_t platform_width = 40;
uint8_t platform_height = 10;
uint8_t previously_moved_right = 0;
uint8_t previously_moved_left = 0;

// Game variables
int8_t hp = 0;
uint16_t score = 0;
uint8_t level = 0;
bool ball_hit_something = 0;

uint8_t prev_battery_level = 0;
uint8_t new_battery_level = 0;


void process_battery(){
	  // Check battery status
	  HAL_ADC_Start(&hadc);
	  HAL_ADC_PollForConversion(&hadc, HAL_MAX_DELAY);
	  uint16_t battery_value = HAL_ADC_GetValue(&hadc);

	  // Find the correct battery level
	  new_battery_level = 0;
	  for(uint8_t i = 0; i < 3; i++){
		  if (battery_value > battery_thresholds[i]) new_battery_level++;
	  }

	  // Print the new battery level
	  if(prev_battery_level != new_battery_level && current_main_state == WATCH){ // WATCH MODE
		  draw_battery();
	  }
	  prev_battery_level = new_battery_level;
}

void draw_battery(){
	ILI9341_Draw_Filled_Rectangle_Coord(250, 20, 275, 30, WHITE);
	ILI9341_Draw_Filled_Rectangle_Coord(275, 23, 278, 28, WHITE);

	if(new_battery_level == 0){
	  ILI9341_Draw_Filled_Rectangle_Coord(252, 22, 273, 28, BLACK);
	}
	else if(new_battery_level == 1){
	  ILI9341_Draw_Filled_Rectangle_Coord(259, 22, 273, 28, BLACK);
	}
	else if(new_battery_level == 2){
	  ILI9341_Draw_Filled_Rectangle_Coord(266, 22, 273, 28, BLACK);
	}
	else{
	  ILI9341_Draw_Filled_Rectangle_Coord(252, 22, 273, 28, GREEN);
	}
	}

void draw_hp(){
	ILI9341_Draw_Text("HP", 5, 2 , BLACK, 2, BLUE);
	char hp_c = hp + '0';
	ILI9341_Draw_Char(hp_c, 40, 2, BLACK, 2, BLUE);
}

void draw_score(uint8_t x, uint8_t y){
	ILI9341_Draw_Text("SCORE", x, y, BLACK, 2, BLUE);
	char score_c[5];
	score_c[3] = '0';
	score_c[2] = '0';
	score_c[1] = score % 10 + '0';
	score_c[0] = score / 10 + '0';
	score_c[4] = 0;
	ILI9341_Draw_Text(score_c, x+65, y , BLACK, 2, BLUE);
}


void copy_level(int level){
	for(uint8_t i = 0; i < N_ROWS * N_COLS; i++)
	{
		current_block_states[i] = block_states[level][i];
	}
}


void draw_block(int index, bool draw){
	volatile uint8_t position_x, position_y;
	uint8_t row_i = index / N_COLS;
	position_x= (index % N_COLS) * COLUMN_WIDTH + LEFT_COLUMN;
	position_y = (index / N_COLS) * ROW_HEIGHT + TOP_ROW;

	ILI9341_Draw_Filled_Rectangle_Coord(position_x, position_y,
			position_x + COLUMN_WIDTH, position_y + ROW_HEIGHT, (!draw) ? BLACK : BLUE);

	ILI9341_Draw_Filled_Rectangle_Coord(position_x + PLATFORM_LINE_THICKNESS, position_y + PLATFORM_LINE_THICKNESS,
			position_x + COLUMN_WIDTH - PLATFORM_LINE_THICKNESS, position_y + ROW_HEIGHT - PLATFORM_LINE_THICKNESS,  (!draw) ? RowColors[row_i] : BLUE );
}

void draw_blocks(){
	for(int i = 0; i < N_ROWS * N_COLS; i++){
			if(current_block_states[i]) draw_block(i, 0);
	}
}

void draw_set_time(bool draw){
	// Prit messages to set time and alarm
	if(draw){
		ILI9341_Draw_Text("Set time", 5, 5, WHITE, 2, BLACK);
	}
	else{
		ILI9341_Draw_Filled_Rectangle_Coord(0, 0, 106, 26, BLACK);
	}


}

void draw_set_alarm(bool draw){
	if(draw){
		ILI9341_Draw_Text("Set alarm", 5, 220, WHITE, 2, BLACK);
	}
	else{
		ILI9341_Draw_Filled_Rectangle_Coord(0, 214,  121,  240,  BLACK);
	}

}


void start_ball(){
	// Delete the previous ball position
	ILI9341_Draw_Filled_Circle(ball_position[0], ball_position[1], ball_size, BLUE);

	ball_position[0] = 120;
	ball_position[1] = 160;

	ball_direction[0] = 0;
	ball_direction[1] = 7;

	ILI9341_Draw_Filled_Circle(ball_position[0], ball_position[1], ball_size, RED);
}

void draw_top_borders(){
	ILI9341_Draw_Filled_Rectangle_Coord(0, 0, LEFT_BORDER, 320, BLACK);
	ILI9341_Draw_Filled_Rectangle_Coord(10, 0, 240, 10, BLACK);
	ILI9341_Draw_Filled_Rectangle_Coord(RIGHT_BORDER, 10, 240, 320, BLACK);
	ILI9341_Draw_Filled_Rectangle_Coord(0, 0, LEFT_BORDER-2, 320, DARKGREY);
	ILI9341_Draw_Filled_Rectangle_Coord(10, 0, 240, 10-2, DARKGREY);
	ILI9341_Draw_Filled_Rectangle_Coord(RIGHT_BORDER+2, 10, 240, 320, DARKGREY);
}

void draw_game_borders(){
	ILI9341_Draw_Filled_Rectangle_Coord(0, TOP_BORDER - TOP_BORDER_HEIGHT - 1, LEFT_BORDER, 320, BLACK);
	ILI9341_Draw_Filled_Rectangle_Coord(10, TOP_BORDER - TOP_BORDER_HEIGHT - 1, 240, TOP_BORDER, BLACK);
	ILI9341_Draw_Filled_Rectangle_Coord(RIGHT_BORDER, TOP_BORDER - TOP_BORDER_HEIGHT, 240, 320, BLACK);
	ILI9341_Draw_Filled_Rectangle_Coord(0, TOP_BORDER - TOP_BORDER_HEIGHT, LEFT_BORDER -1, 320, DARKGREY);
	ILI9341_Draw_Filled_Rectangle_Coord(10, TOP_BORDER - TOP_BORDER_HEIGHT, 240, TOP_BORDER-1, DARKGREY);
	ILI9341_Draw_Filled_Rectangle_Coord(RIGHT_BORDER+1, TOP_BORDER - TOP_BORDER_HEIGHT, 240, 320, DARKGREY);
}

void move_ball(){

	// Delete a bug ball
	ILI9341_Draw_Filled_Circle(120, 160, ball_size, BLUE);

	uint16_t prev_ball_position [2];
	prev_ball_position[0] = ball_position[0];
	prev_ball_position[1] = ball_position[1];


	ball_position[0] += ball_direction[0];
	ball_position[1] += ball_direction[1];

	// Move ball
	if((ball_position[0] != prev_ball_position[0]) || (ball_position[1] != prev_ball_position[1])){
		// Check if the ball has hit the ceiling and reverse the direction
		if(ball_position[1] - ball_size + ball_direction[1] <= TOP_BORDER){
			ball_direction[0] = ball_direction[0];
			ball_direction[1] = -ball_direction[1];
			ball_hit_something = 1;
		}
		// Check if the ball hit the platform and reverse the direction
		else if((ball_position[1] + ball_size + ball_direction[1] >= PLATFORM_LINE - platform_height / 2) &&
				(ball_position[1] + ball_size + ball_direction[1] < PLATFORM_LINE + platform_height / 2) &&
				(ball_position[0] >= platform_position - platform_width / 2 - ball_size - ball_direction[0] - platform_height &&
			    (ball_position[0] <= platform_position + platform_width / 2 + ball_size + ball_direction[0] + platform_height)))
		{
			ball_direction[0] = ball_direction[0];
			ball_direction[1] = -ball_direction[1];

			// Check if the platform is moving
			if(HAL_GPIO_ReadPin(GPIOA, BTN1_Pin) == 0){
				ball_direction[0] += platform_direction / 2;
			}
			else if(HAL_GPIO_ReadPin(GPIOA, BTN2_Pin) == 0){
				ball_direction[0] -= platform_direction / 2;
			}
			ball_hit_something = 1;
		}
		// Check if the ball hit the right side
		else if(ball_position[0] + ball_size + ball_direction[0] >= RIGHT_BORDER){
			ball_direction[0] = -ball_direction[0];
			ball_direction[1] = ball_direction[1];
			ball_hit_something = 1;
		}
		// Check if the ball hit the left side
		else if(ball_position[0] - ball_size + ball_direction[0] <= LEFT_BORDER){
			ball_direction[0] = -ball_direction[0];
			ball_direction[1] = ball_direction[1];
			ball_hit_something = 1;
		}

		else if(ball_position[1] > 320){ // Ball is out of bounds
			start_ball();
			hp--;
			draw_hp();
			start_platform();
			}
		}
		// Delete previous ball
		ILI9341_Draw_Filled_Circle(prev_ball_position[0], prev_ball_position[1], ball_size, BLUE);
		// Draw new ball
		ILI9341_Draw_Filled_Circle(ball_position[0], ball_position[1], ball_size, RED);

		// Check collissions with all the blocks
		for(uint8_t i = 0; i < N_COLS * N_ROWS; i++){
			if(current_block_states[i]){
				uint8_t position_x, position_y;
				position_x = (i % N_COLS) * COLUMN_WIDTH + LEFT_COLUMN;
				position_y = (i / N_COLS) * ROW_HEIGHT + TOP_ROW;

				// Check collision
				if((ball_position[1] + ball_size + ball_direction[1] <= position_y + ROW_HEIGHT) &&
				  (ball_position[1] + ball_size + ball_direction[1] > position_y) &&
				  (ball_position[0] >= position_x - ball_size - ball_direction[0]) &&
				  (ball_position[0] <= position_x + COLUMN_WIDTH + ball_size + ball_direction[0])){
					current_block_states[i] = 0;
					draw_block(i, 1);
					draw_blocks();
					draw_game_borders();
					// Side collisions
					if((ball_position[0] >= position_y)||(ball_position[0] <= position_y + ROW_HEIGHT)) {
						ball_direction[1] = -ball_direction[1];
					}
					else{ // Top and bottom collisions
						ball_direction[0] = -ball_direction[0];
					}
					ball_hit_something = 1;
				score++;
				draw_score(125,2);
			}
		}
	}
}

void start_platform(){
	// Delete the previous platform position
	ILI9341_Draw_Filled_Rectangle_Coord(
			platform_position - platform_width / 2,
			PLATFORM_LINE - platform_height / 2,
			platform_position + platform_width / 2,
			PLATFORM_LINE + platform_height / 2, BLUE);

	platform_position = (RIGHT_BORDER + LEFT_BORDER) / 2;
	platform_direction = DEFAULT_PLATFORM_DIRECTION;
	ILI9341_Draw_Filled_Rectangle_Coord(
			platform_position - platform_width / 2,
			PLATFORM_LINE - platform_height / 2,
			platform_position + platform_width / 2,
			PLATFORM_LINE + platform_height / 2, BLACK);

	ILI9341_Draw_Filled_Rectangle_Coord(
			platform_position - platform_width / 2 + PLATFORM_LINE_THICKNESS,
			PLATFORM_LINE -  platform_height / 2 + PLATFORM_LINE_THICKNESS,
			platform_position + platform_width / 2 - PLATFORM_LINE_THICKNESS,
			PLATFORM_LINE + platform_height / 2- PLATFORM_LINE_THICKNESS , DARKGREY);
}

void move_platform(){
	int prev_platform_position = platform_position;
	if(HAL_GPIO_ReadPin(GPIOA, BTN1_Pin) == 0){
		if(previously_moved_left){
			previously_moved_left = 0;
			platform_direction = DEFAULT_PLATFORM_DIRECTION;
		}
		platform_position += platform_direction;
		previously_moved_right = 1;
		if(previously_moved_right){
			platform_direction += 2;
		}

	}
	else if(HAL_GPIO_ReadPin(GPIOA, BTN2_Pin) == 0){
		if(previously_moved_right){
			previously_moved_right = 0;
			platform_direction = DEFAULT_PLATFORM_DIRECTION;
		}
		platform_position -= platform_direction;
		previously_moved_left = 1;
		if(previously_moved_left){
			platform_direction += 2;
		}
	}
	else{
		platform_direction = DEFAULT_PLATFORM_DIRECTION;
		previously_moved_right = 0;
		previously_moved_left = 0;

	}


	if(prev_platform_position != platform_position){
		// Check that it has not gone out of bounds
		if(platform_position + platform_width / 2 >= RIGHT_BORDER){
			platform_position = RIGHT_BORDER - platform_width / 2;
		}
		else if(platform_position - platform_width / 2 <= LEFT_BORDER){
			platform_position = LEFT_BORDER + platform_width / 2;
		}

		// Move
		ILI9341_Draw_Filled_Rectangle_Coord(
						prev_platform_position - platform_width / 2,
						PLATFORM_LINE - platform_height / 2,
						prev_platform_position + platform_width / 2,
						PLATFORM_LINE + platform_height / 2, BLUE);

		ILI9341_Draw_Filled_Rectangle_Coord(
							platform_position - platform_width / 2,
							PLATFORM_LINE - platform_height / 2,
							platform_position + platform_width / 2,
							PLATFORM_LINE + platform_height / 2, BLACK);

		ILI9341_Draw_Filled_Rectangle_Coord(
			platform_position - platform_width / 2 + PLATFORM_LINE_THICKNESS,
			PLATFORM_LINE -  platform_height / 2 + PLATFORM_LINE_THICKNESS,
			platform_position + platform_width / 2 - PLATFORM_LINE_THICKNESS,
			PLATFORM_LINE + platform_height / 2- PLATFORM_LINE_THICKNESS , DARKGREY);
	}
}

void draw_msg(){
	char msg[] = "Press button";
	ILI9341_Draw_Text(msg, 45, 255, BLACK, 2, BLUE);
}

void delete_msg(){
	ILI9341_Draw_Filled_Rectangle_Coord(40, 250, 200, 280, BLUE);
}

void prepare_start(char title[10]){
	ILI9341_Set_Rotation(SCREEN_VERTICAL_2);
	ILI9341_Fill_Screen(BLUE);

	draw_top_borders();

	// Draw platform
	platform_position = (RIGHT_BORDER + LEFT_BORDER) / 2;
	ILI9341_Draw_Filled_Rectangle_Coord(
			platform_position - platform_width / 2,
			PLATFORM_LINE - 100 - platform_height / 2,
			platform_position + platform_width / 2,
			PLATFORM_LINE - 100 + platform_height / 2, BLACK);
	ILI9341_Draw_Filled_Rectangle_Coord(
				platform_position - platform_width / 2 + PLATFORM_LINE_THICKNESS,
				PLATFORM_LINE - 100 - platform_height / 2 + PLATFORM_LINE_THICKNESS,
				platform_position + platform_width / 2 - PLATFORM_LINE_THICKNESS,
				PLATFORM_LINE - 100 + platform_height / 2- PLATFORM_LINE_THICKNESS , DARKGREY);

	// Draw ball
	ILI9341_Draw_Filled_Circle(120, 160, ball_size, RED);

	// Draw title message
	if(title[0] == 'G'){
		ILI9341_Draw_Text("GAME", 70, 30, BLACK, 4, BLUE);
		ILI9341_Draw_Text("OVER", 70, 70, BLACK, 4, BLUE);
	}
	else{
	ILI9341_Draw_Text(title, 25, 40, BLACK, 4, BLUE);
	}
	// Draw start message
	draw_msg();
}


void prepare_game(){
	// Delete title
	ILI9341_Fill_Screen(BLUE);

	// Draw borders
	draw_game_borders();

	// Start ball
	start_ball();

	// Start platform
	start_platform();
}

void prepare_watch(struct time * current_time){
	ILI9341_Set_Rotation(SCREEN_HORIZONTAL_2);
	ILI9341_Fill_Screen(BLACK);
	// Print minute and hour separator
	ILI9341_Draw_Char(':', 150, 70, WHITE, 8, BLACK);
	raise_flags(current_time);
}

void draw_min_rectangles(bool draw){
	// Draw rectangle below and above minutes
	ILI9341_Draw_Filled_Rectangle_Coord(170, 140, 280, 145, draw ? WHITE : BLACK);
	ILI9341_Draw_Filled_Rectangle_Coord(170, 65, 280, 70, draw ? WHITE : BLACK);
}

int compare_times(struct time * time1, struct time * time2){
	bool are_equal = 1;
	for(uint8_t i = 0; i < 6; i++){
		if(time1->values[i] != time2->values[i]){
			are_equal = 0;
		}
	}
	return are_equal;
}

void draw_hour_rectangles(bool draw){
	// Draw new rectangles above hours
	ILI9341_Draw_Filled_Rectangle_Coord(30, 140, 140, 145, draw ? WHITE : BLACK);
	ILI9341_Draw_Filled_Rectangle_Coord(30, 65, 140, 70, draw ? WHITE : BLACK);
}


uint8_t positions[3][6] = {{250, 210, 230, 170, 90, 30}, {170, 170, 60, 60, 60, 60}, {7,7,10,10,10,10}};
void update_watch(struct time* current_time){
	for(uint8_t i = 0; i < 6; i++){
		if(current_time->change_flag[i]){
			ILI9341_Draw_Char(current_time->values[i] + '0', positions[0][i], positions[1][i], WHITE, positions[2][i], BLACK);
			if((i == 2 || i == 3) && current_clock_state != NORMAL)
				draw_min_rectangles(1);
			else if((i == 4 || i == 5) && current_clock_state != NORMAL)
				draw_hour_rectangles(1);
			current_time->change_flag[i] = 0;
		}
	}
}

void raise_flags(struct time* current_time){
	for(uint8_t i = 0; i < 6; i++){
		current_time->change_flag[i] = 1;
	}
}

uint8_t time_limits[5] = {10, 6, 10, 6, 10};
void increase_time(struct time *current_time, int position){
	// Increase the position
	current_time->values[position*2]++;
	current_time->change_flag[position*2] = 1;

	for(uint8_t i = 0; i < 5; i++){
		if(current_time->values[i] == time_limits[i]){
			current_time->values[i] = 0;
			current_time->values[i+1]++;
			current_time->change_flag[i+1] = 1;
			process_battery();
		}
	}
	if(current_time->values[5] == 2 && current_time->values[4]== 4){
	  for(uint8_t i = 0; i < 6; i++){
		  current_time->values[i] = 0;
	  }
	  current_time->change_flag[4] = 1;
	  current_time->change_flag[5] = 1;
	}
	HAL_GPIO_TogglePin(GPIOA, LED_Pin);
}

void decrease_time(struct time* current_time, volatile int position){
	current_time->values[position*2]--;
	current_time->change_flag[position*2] = 1;
	for(uint8_t i = 0; i < 3; i++){
		if(current_time->values[i*2] == -1){
			current_time->values[i*2+1]--;
			current_time->change_flag[i*2+1] = 1;
			current_time->values[i*2] = 9;
			if(current_time->values[i*2+1] == -1){
				current_time->values[i*2] = 0;
				current_time->values[i*2+1] = 0;
				current_time->change_flag[i*2] = 0;
				current_time->change_flag[i*2+1] = 0;
			}
		}
	}
}

void small_buzz(){
	HAL_GPIO_WritePin(GPIOA, EN_Pin, GPIO_PIN_SET);
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
	HAL_Delay(50);
	HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
	HAL_GPIO_WritePin(GPIOA, EN_Pin, GPIO_PIN_RESET);
}
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
  MX_SPI1_Init();
  MX_ADC_Init();
  MX_TIM1_Init();
  MX_TIM3_Init();
  MX_TIM14_Init();
  MX_TIM16_Init();
  MX_TIM17_Init();
  /* USER CODE BEGIN 2 */
  bool mode_switch;

  // Blinking control
  bool appear = 0;


  ILI9341_Init();//initial driver setup to drive ili9341

  HAL_TIM_Base_Start_IT(&htim3); // Start the clock timer
  HAL_TIM_Base_Start_IT(&htim14); // Start the debouncing timer


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  current_main_state = WATCH;
  current_clock_state = NORMAL;
  current_set_state = SET_MINUTES;
  prepare_watch(&current_time);
  process_battery();
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	// Read mode
	mode_switch = HAL_GPIO_ReadPin(GPIOF, ON_Pin);

	// Alarm function
	if(compare_times(&current_time, &alarm_time)){
		 HAL_TIM_Base_Start_IT(&htim16);  // Start the blinking timer
		 appear = 1;
		 while(1){
			 if(blink){
				if(appear){
					HAL_GPIO_WritePin(GPIOA, EN_Pin, GPIO_PIN_SET);
					HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
				}
				else{
					HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
					HAL_GPIO_WritePin(GPIOA, EN_Pin, GPIO_PIN_RESET);
				}
				appear = !appear;
				blink = 0;
			 }

			 if(btn_1_is_debounced || btn_2_is_debounced){
				 HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
			     HAL_GPIO_WritePin(GPIOA, EN_Pin, GPIO_PIN_RESET);
			     HAL_TIM_Base_Stop_IT(&htim16);  // Stop the blinking timer
			     btn_1_is_debounced = 0;
			     btn_2_is_debounced = 0;
				 break;
			 }
		 }
	}

	// Main state machine
	next_main_state = current_main_state;
	switch(current_main_state){
	case WATCH:
		update_watch(&current_time);
		// Clock state machine
		next_clock_state = current_clock_state;
		switch(current_clock_state){
		case NORMAL:
			if (btn_1_is_debounced || btn_2_is_debounced){
				HAL_TIM_Base_Stop_IT(&htim3);   // Stop counting time
				HAL_TIM_Base_Start_IT(&htim16); // Start the blinking timer
				appear = 1;
				if(btn_1_is_debounced){
					time_to_set = &current_time;
					next_clock_state = SET_TIME;
					btn_1_is_debounced = 0;
				}
				else{
					time_to_set = &alarm_time;
					next_clock_state = SET_ALARM;
					raise_flags(&alarm_time);
					update_watch(&alarm_time);
					draw_hour_rectangles(0);
					btn_2_is_debounced = 0;
				}
			}
		break;

		case SET_ALARM:
			update_watch(&alarm_time);
		case SET_TIME:
			next_set_state = current_set_state;
			// Blinking effect
			if(blink){
				if(current_clock_state == SET_TIME)
					draw_set_time(appear);
				else
					draw_set_alarm(appear);

				if(current_set_state == SET_MINUTES)
					draw_min_rectangles(appear);
				else
					draw_hour_rectangles(appear);

				appear = !appear;
				n_loops++;
				blink = 0;
			}
			switch(current_set_state){
			case SET_MINUTES:
			case SET_HOURS:
				// Increase values when btn1 is pressed
				if(btn_1_is_debounced){
					increase_time(time_to_set, current_set_state + 1);
					n_loops = 0;
					btn_1_is_debounced = 0;
				}
				// Decrease values when btn2 is pressed
				if(btn_2_is_debounced){
					decrease_time(time_to_set, current_set_state + 1);
					n_loops = 0;
					btn_2_is_debounced = 0;
				}
				// State transition
				if(n_loops > LOOP_LIMIT+1){
					n_loops = 0;
					if(current_set_state == SET_MINUTES){
						next_set_state = SET_HOURS;
						draw_hour_rectangles(1);
						draw_min_rectangles(0);
					}
					else{
						next_clock_state = NORMAL;
						next_set_state = SET_MINUTES;
						HAL_TIM_Base_Start_IT(&htim3);   // Start counting time
						HAL_TIM_Base_Stop_IT(&htim16);   // Stop the blinking timer
						raise_flags(&current_time);
						update_watch(&current_time);
						draw_min_rectangles(0);
						draw_hour_rectangles(0);
						draw_set_time(0);
						draw_set_alarm(0);
					}
				}
				break;
			}
			current_set_state = next_set_state;
		break;
		}
		current_clock_state = next_clock_state;


		// State transition
		if (mode_switch == 1){
			next_main_state = GAME;
			current_game_state = START;
			// Prepare game
			prepare_start("ARKANOID");
			HAL_TIM_Base_Stop_IT(&htim14);   // Stop button debounce timer
			HAL_TIM_Base_Start_IT(&htim16);  // Start the blinking timer
			btn_1_is_debounced = 0; // Reset buttons
			btn_2_is_debounced = 0;
			appear = 1;
		}
	break;

	case GAME:
		// Game state machine
		next_game_state = current_game_state;
		switch(current_game_state){
		case START:
		case GAME_OVER:
		case GAME_WIN:
			if(blink){
				if(appear){
					draw_msg();
				}
				else{
					delete_msg();
				}
				appear = !appear;
				blink = 0;
			}
			if((HAL_GPIO_ReadPin(GPIOA, BTN1_Pin) == 0) || (HAL_GPIO_ReadPin(GPIOA, BTN2_Pin) == 0)){
				next_game_state = GAMING;
				small_buzz(); // Do it before the game starts
				HAL_TIM_Base_Start_IT(&htim17);   // Start the game timer
				HAL_TIM_Base_Stop_IT(&htim16);    // Stop the blinking timer


				hp = 3;
				score = 0;
				level = 0;
				delete_msg();
				prepare_game();
				draw_hp();
				draw_score(125, 2);
				copy_level(level);
				draw_blocks();


				btn_1_is_debounced = 0;
				btn_2_is_debounced = 0;

			}

			break;
		case GAMING:
			// Check if the level is over
			bool level_over = 1;
			for(int i = 0; i < N_COLS*N_ROWS; i++){
				if(current_block_states[i]) level_over = 0;
			}
			if(ball_hit_something){
				ball_hit_something = 0;
				small_buzz();
			}
			if(level_over){
				level++;
				start_ball();
				start_platform();

				if(level == 3){
					next_game_state = GAME_WIN;
					HAL_TIM_Base_Stop_IT(&htim17);   // Stop the game timer
					HAL_TIM_Base_Start_IT(&htim16);  // Start the blinking timer

					prepare_start("YOU WIN!");
					btn_1_is_debounced = 0; // Reset buttons
					btn_2_is_debounced = 0;
					draw_score(60, 200);
					appear = 1;
				}
				else{
					// 1 second delay
					HAL_TIM_Base_Stop_IT(&htim17);
					HAL_Delay(1000);
					copy_level(level);
					draw_blocks();
					HAL_TIM_Base_Start_IT(&htim17);
				}
			}
			if(hp < 0){
				next_game_state = GAME_OVER;
				HAL_TIM_Base_Stop_IT(&htim17);   // Stop the game timer
				HAL_TIM_Base_Start_IT(&htim16);  // Start the blinking timer

				prepare_start("GAME OVER");
				btn_1_is_debounced = 0; // Reset buttons
				btn_2_is_debounced = 0;
				draw_score(60, 200);
				appear = 1;
			}
			break;
		}
		current_game_state = next_game_state;

		// State transition
		if(mode_switch == 0 && btn_1_is_debounced == 0 && btn_2_is_debounced == 0){
			next_main_state = WATCH;
			next_clock_state = NORMAL;

			// Prepare watch
			prepare_watch(&current_time);

			HAL_TIM_Base_Stop_IT(&htim17);    // Stop the game timer
			HAL_TIM_Base_Stop_IT(&htim16);    // Stop the blinking timer
			HAL_TIM_Base_Start_IT(&htim14);   // Start the button debounce timer

			draw_battery();
		}
		break;

	}
	current_main_state = next_main_state;
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSI14;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSI14State = RCC_HSI14_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.HSI14CalibrationValue = 16;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC_Init(void)
{

  /* USER CODE BEGIN ADC_Init 0 */

  /* USER CODE END ADC_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC_Init 1 */

  /* USER CODE END ADC_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc.Instance = ADC1;
  hadc.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc.Init.Resolution = ADC_RESOLUTION_12B;
  hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD;
  hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc.Init.LowPowerAutoWait = DISABLE;
  hadc.Init.LowPowerAutoPowerOff = DISABLE;
  hadc.Init.ContinuousConvMode = DISABLE;
  hadc.Init.DiscontinuousConvMode = DISABLE;
  hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc.Init.DMAContinuousRequests = DISABLE;
  hadc.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  if (HAL_ADC_Init(&hadc) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC_Init 2 */

  /* USER CODE END ADC_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_1LINE;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 3199;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 10;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 4;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 3199;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 10000;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM14 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM14_Init(void)
{

  /* USER CODE BEGIN TIM14_Init 0 */

  /* USER CODE END TIM14_Init 0 */

  /* USER CODE BEGIN TIM14_Init 1 */

  /* USER CODE END TIM14_Init 1 */
  htim14.Instance = TIM14;
  htim14.Init.Prescaler = 3199;
  htim14.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim14.Init.Period = 10;
  htim14.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim14.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim14) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM14_Init 2 */

  /* USER CODE END TIM14_Init 2 */

}

/**
  * @brief TIM16 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM16_Init(void)
{

  /* USER CODE BEGIN TIM16_Init 0 */

  /* USER CODE END TIM16_Init 0 */

  /* USER CODE BEGIN TIM16_Init 1 */

  /* USER CODE END TIM16_Init 1 */
  htim16.Instance = TIM16;
  htim16.Init.Prescaler = 3199;
  htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim16.Init.Period = 10000;
  htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim16.Init.RepetitionCounter = 0;
  htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM16_Init 2 */

  /* USER CODE END TIM16_Init 2 */

}

/**
  * @brief TIM17 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM17_Init(void)
{

  /* USER CODE BEGIN TIM17_Init 0 */

  /* USER CODE END TIM17_Init 0 */

  /* USER CODE BEGIN TIM17_Init 1 */

  /* USER CODE END TIM17_Init 1 */
  htim17.Instance = TIM17;
  htim17.Init.Prescaler = 3199;
  htim17.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim17.Init.Period = 1000;
  htim17.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim17.Init.RepetitionCounter = 0;
  htim17.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim17) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM17_Init 2 */

  /* USER CODE END TIM17_Init 2 */

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
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, RST_Pin|LED_Pin|CS_Pin|EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(DC_GPIO_Port, DC_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : ON_Pin */
  GPIO_InitStruct.Pin = ON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ON_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : BTN1_Pin BTN2_Pin */
  GPIO_InitStruct.Pin = BTN1_Pin|BTN2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : RST_Pin LED_Pin CS_Pin EN_Pin */
  GPIO_InitStruct.Pin = RST_Pin|LED_Pin|CS_Pin|EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : DC_Pin */
  GPIO_InitStruct.Pin = DC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(DC_GPIO_Port, &GPIO_InitStruct);

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
