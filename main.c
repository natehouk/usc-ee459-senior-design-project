/*************************************************************
*
*   EE459 Alarm Clock Project
*   Team 1A, Spring 2008
*   
*   Author: Nate Houk
*   Description: This code implements a 7-day alarm clock
*   on a MC908JL16 microcontroller utilizing the Apple
*   Accessory Protocol to control an iPod via RS232.
*
*   Copyright 2008, all rights reserved.
*
*************************************************************/

#include <hidef.h>      /* For EnableInterrupts macro */
#include "derivative.h" /* Include peripheral declarations */

/* The following puts the dummy interrupt service routine at
   location MY_ISR_ROM which is defined in the PRM file as
   the start of the FLASH ROM */
#pragma CODE_SEG MY_ISR_ROM
#pragma TRAP_PROC
void dummyISR(void) {}

/* This pragma sets the code storage back to default area of ROM as defined in
   the PRM file.*/
#pragma CODE_SEG DEFAULT

/* Implements the ASSERT macro */
#define assert(x)      if(!(x)) {\
                          DisableInterrupts;\
                          for(;;) {}\
                       }
                       
/* Defines the variable input as PTA bits 0-1, 4-5 */
volatile struct {
    char a:1;
    char b:1;
    char :2;
    char c:1;
    char d:1;
    char :2;
} MyPTA @0x0000;
#define input ((MyPTA.d << 3) | (!MyPTA.c << 2) | (!MyPTA.b << 1) | !MyPTA.a)                       

/* Defines the variable data as PTB bits 0-7 */
volatile struct {
    char data:8;
} MyPTB @0x0001;
#define data MyPTB.data

/* Defines the variable addr as PTD bits 0-2, the variable mode_switch as PTD bit 3,
   the variable debug_switch as PTD bit 4, the variable buzzer_switch as PTD bit 5 */
volatile struct {
    char addr:3;
    char mode_switch:1;
    char debug_switch:1;
    char alarm_switch:1;
    char :2;    
} MyPTD @0x0003;
#define addr MyPTD.addr
#define mode_switch (!MyPTD.mode_switch)
#define debug_switch (!MyPTD.debug_switch)
#define alarm_switch (!MyPTD.alarm_switch)

/* Defines the device addresses on the I2C bus */
#define CLOCK_ADDR      0xD0    // DS1307 Clock
#define EEPROM_ADDR     0xA0    // 24LC256 EEPROM

/* Defines the device addresses on the data bus */
#define SEND            0x00
#define HOUR_ADDR       0x01
#define MIN_ADDR        0x02
#define OUTPUT0_ADDR    0x03
#define OUTPUT1_ADDR    0x04
#define OUTPUT2_ADDR    0x05

/* Defines the internal clock addresses */
#define CLOCK_SEC_ADDR  0x00
#define CLOCK_CTR_ADDR  0x07
#define CLOCK_MODE_ADDR 0x06

/* Defines the internal clock control bits */
#define CLOCK_CTR_BITS  0x11
#define CLOCK_SET       0xD9
#define CLOCK_NOT_SET   0x00

/* Defines the internal eeprom addresses */
#define EEPROM_MSB_ADDR 0x00
#define EEPROM_ALM_ADDR 0x00

/* Defines common bit masks */
#define BIT0_MASK       1
#define BIT1_MASK       2
#define BIT2_MASK       4
#define BIT3_MASK       8
#define BIT4_MASK       16
#define BIT5_MASK       32
#define BIT6_MASK       64
#define BIT7_MASK       128
#define CH_MASK         128
#define TIME_MODE_MASK  64
#define SEC_MASK        0x7F
#define MIN_MASK        0x7F
#define HOUR_MASK       0x3F
#define DAY_MASK        0x07
#define ALARM_MASK      1
#define BEEP_MASK       1
#define AM_PM_MASK      1
#define COLON_MASK      2
#define ONE_DIGIT_MASK  4
#define DAYS_MASK       0xFE
#define FLASH_MASK      0x1F

/* Global data and constants*/
#define HOUR            0
#define MIN             1
#define SEC             2
#define DAY             3
#define ALM_ENABLE      2
#define BLANK           0xFF
#define TIME_SIZE       4
#define ALARM_COUNT     8
#define ALARM_SIZE      3
#define INPUT_COUNT     10
#define INPUT_SIZE      5
#define BUFFER_SIZE     128
#define OUTPUT_SIZE     3

char debug;
signed char time[TIME_SIZE], debug_time[TIME_SIZE];
char alarms[ALARM_COUNT][ALARM_SIZE];
char output[OUTPUT_SIZE];
char buttons[12][7];
char buffer[BUFFER_SIZE];

/* State variables and constants */
#define CLOCK_MODE      0
#define CLOCK           0
#define SET_CLOCK_HOUR  1
#define SET_CLOCK_MIN   2
#define SET_DAY         3
#define SET_ALARM_HOUR  4
#define SET_ALARM_MIN   5
#define VIEW_ALARM      6
#define ENABLE_ALARM    7
#define ACTIVATE_ALARM  8
#define TIME_MODE       1
#define NORMAL          0
#define MILITARY        1
#define ALARM_MODE      2
#define BUZZER          TRUE
#define IPOD            FALSE
#define NONE            0
#define FLASH_SEC       1
#define FLASH_MIN       2
#define FLASH_HOUR      4
#define FLASH_DAY       8
#define FLASH_ALARM_DAY 16
#define ALARM_ON        32
#define FLASH_INTERVAL  5       // Flash interval in 1/20's of a second
#define BUZZ_INTERVAL   5       // Buzz interval in 1/20's of a second
#define VIEW_TIME       80      // View time in 1/20's of a second
#define BEEP_TIME       2       // Beep time in 1/20's of a second
#define OFF_TIME        80      // iPod off button hold time in 1/20's of a second
#define DEBUG_SPEED     6       // 1/( 20 * Debug_Speed) = length of a second in debug mode 
#define BEEP            TRUE
#define NO_BEEP         FALSE
#define SNOOZE          0
#define SNOOZE_TIME     10      // Snooze interval in minutes
#define SUN             1
#define MON             2
#define TUE             3
#define WED             4
#define THU             5
#define FRI             6
#define SAT             7
#define DOWN            8
#define SEL             9
#define UP              10
#define STATUS          0
#define RELEASED        0
#define PRESSED         1
#define TOTAL_ELAPSE    1      
#define KEYFIRE_ELAPSE  2
#define PREV_ELAPSE     3
#define STATE           4
#define STATE_RESET     0
#define STATE_PRESSED   1
#define STATE_HELD      2
#define STATE_RELEASED  3
#define STATE_LOCKED    4
#define NO_START        -1
#define NO_REPEAT       -1
#define LOCK_BUTTON     -2

char mode[3];
char control;
char flash;
char flash_ds;
char beep;
char beep_ds;
char buzz;
char buzz_ds;
char view;
char view_ds;
char off;
char off_ds;
char alarm_day;
char clock_set;
char i2c_timeout;

/* Initialize function prototypes */
void init(void);

/* I/O function prototypes */
void flush(void);
void scan(void);;
char released(char, signed char, char);
char held(char, signed char, signed char, char);

/* Helper function prototpes */
char bcd2dec(char);
char dec2bcd(char);
char sn(char);

/* Alarm function prototypes */
void alarm_write(void);
void alarm_read(void);
char alarm_check(void);

/* Time function prototypes */
void time_write(void);
void time_read(void);
char time_format(char);
void time_volatile(char);

/* iPod function prototypes */
void ipod_play(void);
void ipod_pause(void);
void ipod_off(void);
void ipod_skip_forward(void);
void ipod_skip_back(void);
void ipod_volume_up(void);
void ipod_volume_down(void);
void ipod_cmd_play(void);
void ipod_cmd_pause(void);
void ipod_cmd_skip_forward(void);
void ipod_cmd_skip_back(void);
void ipod_cmd_volume_up(void);
void ipod_cmd_volume_down(void);
void ipod_cmd_stop(void);
void ipod_cmd_off(void);
void ipod_cmd_button_release(void);

/* SCI bus function prototypes */
void sci_write(unsigned char);

/* I2C bus function prototypes */
void i2c_write(char, char *, char);
void i2c_read(char, char *, char);
void i2c_start_timeout(void);

void main(void) {  
  char i;
 
  EnableInterrupts;             // Enable interrupts
  CONFIG1_COPD = 1;             // Disable COP reset
 
  /* Configure SCI */
  SCC1_ENSCI = 1;               // Enable SCI
    
  /* Set output for 19,200 baud assuming a 9.8304MHz clock */
  SCBR_SCP = 0;                 // Baud rate prescaler = 1
  SCBR_SCR = 1;                 // Baud rate divisor = 2
  SCC2_TE = 1;                  // Enable transmitter
    
  /* Configure I2C */
  CONFIG2_IICSEL = 1;           // Set PTA bits 2-3 for I2C
  MIMCR_MMBR = 2;               // Set baud rate divisor
  MMCR_MMEN = 1;                // Enable MMII
    
  /* Set PTA bits 0-1, 4-5 for input */
  DDRA = DDRA & ~BIT0_MASK & ~BIT1_MASK & ~BIT4_MASK & ~BIT5_MASK;   

  /* Set PTB bits 0-7 for output */
  DDRB = DDRB | BIT0_MASK | BIT1_MASK | BIT2_MASK | BIT3_MASK
              | BIT4_MASK | BIT5_MASK | BIT6_MASK | BIT7_MASK;
             
  /* Set PTD bits 0-2 for output, 3-5 for input */
  DDRD = DDRD | BIT0_MASK | BIT1_MASK | BIT2_MASK & ~BIT3_MASK & ~BIT4_MASK & ~BIT5_MASK;
 
  /* Initialize the clock */
  init();
    
  /* Configure Timer 2 */
  T2SC_TRST = 1;                // Reset timer
  T2SC_PS = 2;                  // Set prescalar for divide by 4
  T2SC_TOIE = 0;                // Disable timer interrupt
  T2MOD = 31250;                // Store modulo value in T1MODH:T1MODL
  T2SC_TSTOP = 0;               // Start timer running
     
  for(;;) {  
    /* Update status of buttons */
    for (i=0; i<INPUT_COUNT; i++) {
      buttons[i][STATUS]=RELEASED;
    }
    if (input>0 && input<=INPUT_COUNT) {
      buttons[input-1][STATUS]=PRESSED;
    }
    
    /* Are we debugging? */
    if (debug != debug_switch) {
      debug = debug_switch;
      time_write();
    }
 
    /* Are we in 12/24 mode? */
    if (mode[TIME_MODE] != mode_switch) {
      mode[TIME_MODE] = mode_switch;
    }
    
    /* Are we in buzzer or iPod mode? */
    if (mode[ALARM_MODE] != alarm_switch) {
      mode[ALARM_MODE] = alarm_switch;
    }
              
    /* Did Timer 2 expire? */
    if (T2SC_TOF == 1) {
      T2SC_TOF = 0;             // Reenable timer
      if (!(control & FLASH_MASK)) {
        flash=TRUE;
        flash_ds=1;
      } else if (flash_ds++ % FLASH_INTERVAL == 0) {
        flash=!flash;
        flash_ds=1;
      }
      if (!(control & ALARM_ON)) {
        buzz=TRUE;
        buzz_ds=1;
      } else if (buzz_ds++ % BUZZ_INTERVAL == 0) {
        buzz=!buzz;
        buzz_ds=1;
      }
      if (!view) view_ds=1;
      else if (view_ds++ % VIEW_TIME == 0) {
        view=FALSE;
        view_ds=1;
      }
      if (!beep) beep_ds=1;
      else if (beep_ds++ % BEEP_TIME == 0) {
        beep=FALSE;
        beep_ds=1;
      }
      if (!off) off_ds=1;
      else if (off_ds++ % OFF_TIME == 0) {
        off=FALSE;
        off_ds=1;
        ipod_cmd_button_release();
      }
      
      /* Debug clock? */
      if (debug) {
        /* Speed up time for debugging */
        debug_time[SEC]+=DEBUG_SPEED;
        if (debug_time[SEC]>59) {
          debug_time[SEC]=0;
          debug_time[MIN]++;
        }
        if (debug_time[MIN]>59) {
          debug_time[MIN]=0;
          debug_time[HOUR]++;
        }
        if (debug_time[HOUR]>23) {
          debug_time[HOUR]=0;
          debug_time[DAY]++;
        }
        if (debug_time[DAY]>7) {
          debug_time[DAY]=1;
        }
      }
 
      scan();                   // Scan inputs
    }
      
    /* CLOCK mode */  
    if (mode[CLOCK_MODE] == CLOCK) {
      control = NONE;
    
      if (clock_set == FALSE) control = FLASH_HOUR | FLASH_MIN;
      
      /* Update time */
      time_read();
      
      /* Change mode? */
      if (alarm_check()) {
        mode[CLOCK_MODE] = ACTIVATE_ALARM;
        if (mode[ALARM_MODE]==IPOD) {
          ipod_play();
        }
      } else if (released(SEL, 0, NO_BEEP)) {
        ipod_pause();
      } else if (released(DOWN, 0, NO_BEEP)) {
        ipod_skip_back();
      } else if (released(UP, 0, NO_BEEP)) {
        ipod_skip_forward(); 
      } else if (held(SEL, 20, LOCK_BUTTON, BEEP)) {
        time_read();
        mode[CLOCK_MODE] = SET_CLOCK_HOUR;
        time_volatile(FALSE);
      } else if (held(UP, 10, 4, NO_BEEP)) {    
        ipod_volume_up();
      } else if (held(DOWN, 10, 4, NO_BEEP)) {    
        ipod_volume_down();
      } else {  
        for (i=SUN; i<=SAT; i++) {
          if (released(i, 0, NO_BEEP)) {
            alarm_day = i;
            view = TRUE;
            mode[CLOCK_MODE] = VIEW_ALARM;
          }
          else if (held(i, 10, NO_REPEAT, BEEP)) {
            alarm_day = i;
            mode[CLOCK_MODE] = ENABLE_ALARM;
          } else if (held(i, NO_START, 20, BEEP)) {
            alarm_day = i;
              
            /* Load alarm to set */
            time[HOUR]= alarms[alarm_day][HOUR];
            time[MIN] = alarms[alarm_day][MIN];
            
            mode[CLOCK_MODE] = SET_ALARM_HOUR;
          }
        }
      }
    }    
      
    /* SET_CLOCK_HOUR mode */
    else if (mode[CLOCK_MODE] == SET_CLOCK_HOUR) {
      control = FLASH_HOUR;
      
      /* Decrease hour? */
      if (released(DOWN, 0, NO_BEEP)) time[HOUR]--;
      else if (held(DOWN, 20, 20, NO_BEEP)) {
        if (mode[TIME_MODE] == NORMAL) time[HOUR]-=12;
        else time[HOUR]--;
      }
      if (time[HOUR]<0) time[HOUR]+=24;
      
      /* Increase hour? */
      if (released(UP, 0, NO_BEEP)) time[HOUR]++;
      else if (held(UP, 20, 20, NO_BEEP)) {
        if (mode[TIME_MODE] == NORMAL) time[HOUR]+=12;
        else time[HOUR]++;
      }
      if (time[HOUR]>23) time[HOUR]-=24;
      
      /* Change mode? */
      if (held(SEL, 0, LOCK_BUTTON, BEEP)) mode[CLOCK_MODE]=SET_CLOCK_MIN;
    }
      
    /* SET_CLOCK_MIN mode */
    else if (mode[CLOCK_MODE] == SET_CLOCK_MIN) {
      control = FLASH_MIN;
       
      /* Decrease minute? */
      if (released(DOWN, 0, NO_BEEP)) time[MIN]--;
      else if (held(DOWN, 10, 2, NO_BEEP)) time[MIN]--;     
      if (time[MIN]<0) time[MIN]+=60;
      
      /* Increase minute? */
      if (released(UP, 0, NO_BEEP)) time[MIN]++;
      else if (held(UP, 10, 2, NO_BEEP)) time[MIN]++;     
      if (time[MIN]>59) time[MIN]-=60;
        
      /* Change mode? */
      if (held(SEL, 0, LOCK_BUTTON, BEEP)) mode[CLOCK_MODE]=SET_DAY;
    }
            
    /* SET_DAY mode */
    else if (mode[CLOCK_MODE] == SET_DAY) {
      control = FLASH_DAY;
      
      /* Decrease day? */
      if(released(DOWN, 0, NO_BEEP)) time[DAY]--;
      else if (held(DOWN, 10, 10, NO_BEEP)) time[DAY]--;
      if (time[DAY]<1) time[DAY]+=7;
      
      /* Increase day? */
      if(released(UP, 0, NO_BEEP)) time[DAY]++;
      else if (held(UP, 10, 10, NO_BEEP)) time[DAY]++;
      if (time[DAY]>7) time[DAY]-=7;
       
      /* Change mode? */
      if (held(SEL, 0, LOCK_BUTTON, BEEP)) {
        mode[CLOCK_MODE]=CLOCK;
        time[SEC] = 0;
        time_write();
        time_volatile(TRUE);
      }
    }
    
    /* SET_ALARM_HOUR mode */
    else if (mode[CLOCK_MODE] == SET_ALARM_HOUR) {
      control = FLASH_HOUR | FLASH_ALARM_DAY;
      
      /* Decrease hour? */
      if (released(DOWN, 0, NO_BEEP)) time[HOUR]--;
      else if (held(DOWN, 10, 20, NO_BEEP)) {
        if (mode[TIME_MODE] == NORMAL) time[HOUR]-=12;
        else time[HOUR]--;
      }
      if (time[HOUR]<0) time[HOUR]+=24;
      
      /* Increase hour? */
      if (released(UP, 0, NO_BEEP)) time[HOUR]++;
      else if (held(UP, 10, 20, NO_BEEP)) {
        if (mode[TIME_MODE] == NORMAL) time[HOUR]+=12;
        else time[HOUR]++;
      }
      if (time[HOUR]>23) time[HOUR]-=24;
      
      /* Change mode? */
      if (held(SEL, 0, LOCK_BUTTON, BEEP)) mode[CLOCK_MODE]=SET_ALARM_MIN;
    }
    
    /* SET_ALARM_MIN mode */
    else if (mode[CLOCK_MODE] == SET_ALARM_MIN) {
      control = FLASH_MIN | FLASH_ALARM_DAY;
       
      /* Decrease minute? */
      if (released(DOWN, 0, NO_BEEP)) time[MIN]--;
      else if (held(DOWN, 10, 2, NO_BEEP)) time[MIN]--;     
      if (time[MIN]<0) time[MIN]+=60;
      
      /* Increase minute? */
      if (released(UP, 0, NO_BEEP)) time[MIN]++;
      else if (held(UP, 10, 2, NO_BEEP)) time[MIN]++;     
      if (time[MIN]>59) time[MIN]-=60;
        
      /* Change mode? */
      if (held(SEL, 0, LOCK_BUTTON, BEEP)) {
        mode[CLOCK_MODE]=CLOCK;
      
        /* Enable and save the alarm */
        alarms[alarm_day][HOUR]=time[HOUR];
        alarms[alarm_day][MIN]=time[MIN];
        alarms[alarm_day][ALM_ENABLE] = TRUE;
        alarm_write();
        alarm_day = NONE;
      }
    }
        
    /* VIEW_ALARM mode */
    else if (mode[CLOCK_MODE] == VIEW_ALARM) {
      control = FLASH_ALARM_DAY;
      
      /* Update time */
      time_read();
      
      /* Change mode? */
      if (alarm_check()) {
        mode[CLOCK_MODE] = ACTIVATE_ALARM;
        if (mode[ALARM_MODE]==IPOD) {
          ipod_play();
        }
      } else if (held(SEL, 20, LOCK_BUTTON, BEEP)) {
        time_read();
        mode[CLOCK_MODE] = SET_CLOCK_HOUR;
        time_volatile(FALSE);
      } else if (held(UP, 0, LOCK_BUTTON, NO_BEEP)) {    
        ipod_volume_up();
      } else if (held(DOWN, 0, LOCK_BUTTON, NO_BEEP)) {    
        ipod_volume_down();
      } else {  
        for (i=SUN; i<=SAT; i++) {
          if (released(i, 0, NO_BEEP)) {
            alarm_day = i;
            view = TRUE;
            view_ds=1;
            mode[CLOCK_MODE] = VIEW_ALARM;
          }
          else if (held(i, 10, NO_REPEAT, BEEP)) {
            alarm_day = i;
            mode[CLOCK_MODE] = ENABLE_ALARM;
          } else if (held(i, NO_START, 20, BEEP)) {
            alarm_day = i;
              
            /* Load alarm to set */
            time[HOUR]= alarms[alarm_day][HOUR];
            time[MIN] = alarms[alarm_day][MIN];
            
            mode[CLOCK_MODE] = SET_ALARM_HOUR;
          }
        }
      }
      
      /* Did the view time expire? */
      if (view == FALSE) {
        alarm_day = NONE;
        mode[CLOCK_MODE]=CLOCK;
      }
      
      /* Load alarm to view */
      time[HOUR]= alarms[alarm_day][HOUR];
      time[MIN] = alarms[alarm_day][MIN];     
    }
    
    /* ENABLE_ALARM mode */
    else if (mode[CLOCK_MODE] == ENABLE_ALARM) {
      control = NONE;
    
      /* Toggle alarm enable */
      alarms[alarm_day][ALM_ENABLE] = !alarms[alarm_day][ALM_ENABLE];
      alarm_write();
      alarm_day = NONE;
      
      mode[CLOCK_MODE]=CLOCK;
    }
      
    /* ACTIVATE_ALARM mode */
    else if (mode[CLOCK_MODE] == ACTIVATE_ALARM) {
      control = FLASH_HOUR | FLASH_MIN;
      if (mode[ALARM_MODE]==BUZZER) {
        control |= ALARM_ON;
      }
         
      /* Update time */
      time_read();
          
      /* Change mode? */
      if (released(DOWN, 0, NO_BEEP) | released(SEL, 0, NO_BEEP) | released(UP, 0, NO_BEEP)) {
        mode[CLOCK_MODE]=CLOCK;
        alarms[SNOOZE][ALM_ENABLE]=TRUE;
        alarms[SNOOZE][MIN]=time[MIN]+SNOOZE_TIME;
        alarms[SNOOZE][HOUR]=time[HOUR];
        if (alarms[SNOOZE][MIN]>59) {
          alarms[SNOOZE][MIN]-=60;
          alarms[SNOOZE][HOUR]++;
        }
        if (alarms[SNOOZE][HOUR]>23) {
          alarms[SNOOZE][HOUR]-=24;
        }        
        if (mode[ALARM_MODE]==IPOD) {
          ipod_off();
        }
      } else if (held(DOWN, 20, LOCK_BUTTON, NO_BEEP) | held(SEL, 20, LOCK_BUTTON, NO_BEEP) | held(UP, 20, LOCK_BUTTON, NO_BEEP)) {
        mode[CLOCK_MODE]=CLOCK;
        alarms[SNOOZE][ALM_ENABLE]=FALSE;
        if (mode[ALARM_MODE]==IPOD) {
          ipod_off();
        }
      }
    }

    /* Flush output */
    flush();
  }
}

/* Initalizes the clock upon bootup */
void init(void) {
  int i;

  /* Check for power loss */
  buffer[0] = CLOCK_MODE_ADDR;
  i2c_write(CLOCK_ADDR, buffer, 1);
  i2c_read(CLOCK_ADDR, buffer, 1);
  if (buffer[0] != CLOCK_SET) time_volatile(FALSE);
  else time_volatile(TRUE);
 
  /* Set modes */
  mode[CLOCK_MODE]=NORMAL;
  mode[TIME_MODE]=NORMAL;
  
  /* Initialize timers */
  flash_ds=1;
  beep_ds=1;
  buzz_ds=1;
  view_ds=1;
  off_ds=1;
 
  /* Initalize clock chip */
  buffer[0] = CLOCK_SEC_ADDR;
  i2c_write(CLOCK_ADDR, buffer, 1);
  i2c_read(CLOCK_ADDR, buffer, 3);
  buffer[3] = buffer[2] & ~TIME_MODE_MASK; // Set 12/24 = 0
  buffer[2] = buffer[1];
  buffer[1] = buffer[0] & ~CH_MASK;        // Set CH = 0
  buffer[0] = CLOCK_SEC_ADDR;
  i2c_write(CLOCK_ADDR, buffer, 4);
  buffer[0] = CLOCK_CTR_ADDR;
  buffer[1] = CLOCK_CTR_BITS;
  i2c_write(CLOCK_ADDR, buffer, 2);
 
  /* Load time and alarms */  
  time_read();
  alarm_read();
  alarms[SNOOZE][ALM_ENABLE]=FALSE;
  
  /* Wake iPod up */
  ipod_skip_back();
  
  /* Wait for iPod to wake up */
  for (i=0; i<1000; i++) {}
  
  /* Turn the iPod off */
  ipod_off();
}

/* Flushes the output to the board */
void flush() {
  char i;
 
  /* Output hour */
  if (control & FLASH_HOUR && !flash) data = BLANK;
  else {
    /* Format the hour, convert to BCD and swap the nibbles! */
    data = sn(dec2bcd(time_format(time[HOUR])));
    
    /* Display AM/PM? */
    if (mode[TIME_MODE] == NORMAL && (data & 0x0F) == 0) data = data | 0x0F;
  }
  addr = HOUR_ADDR;
  addr = SEND;
   
  /* Output minute */    
  if (control & FLASH_MIN && !flash) data = BLANK;
  else {
    /* Convert to BCD and swap the nibbles! */
    data = sn(dec2bcd(time[MIN]));
  }
  addr = MIN_ADDR;
  addr = SEND;
       
  /* Write output bank 0 */
  if ((control & FLASH_DAY) && !flash) output[0] = output[0] | DAYS_MASK;
  else output[0] = (output[0] | DAYS_MASK) & ~(1 << time[DAY]);
  data = output[0];
  addr = OUTPUT0_ADDR;
  addr = SEND;
        
  /* Write output bank 1 */
  if (beep) output[1] = (output[1] | BEEP_MASK);
  else output[1] = output[1] & ~BEEP_MASK;
  for (i=SUN; i<=SAT; i++) {
    if ( (alarms[i][ALM_ENABLE] && i!=alarm_day) ||
          (alarms[i][ALM_ENABLE] && !(control & FLASH_ALARM_DAY) && i == alarm_day) ||
          ((control & FLASH_ALARM_DAY) && flash && i == alarm_day) ) {
      output[1] = output[1] & ~(1 << i);
    }
    else output[1] = output[1] | (1 << i);
  }
  data = output[1];
  addr = OUTPUT1_ADDR;
  addr = SEND;
 
  /* Write output bank 2 */
  if ((control & ALARM_ON) && buzz) output[2] = (output[2] | ALARM_MASK);
  else output[2] = output[2] & ~ALARM_MASK;
  if ((control & FLASH_HOUR) || (control & FLASH_MIN) || (control & FLASH_DAY) || (control & FLASH_ALARM_DAY)
            || (!debug && (time[SEC] % 2 == 0)) || (debug && (time[MIN] % 2 == 0))) output[2] = (output[2] & ~COLON_MASK);
  else output[2] = (output[2] | COLON_MASK);
  if ((control & FLASH_HOUR) && !flash) output[2] = (output[2] | ONE_DIGIT_MASK);
  
  data = output[2];
  addr = OUTPUT2_ADDR;
  addr = SEND;   
}

/* Scan the buttons and update the states */
void scan() {
  char i;
  for (i=0; i<INPUT_COUNT; i++) {
    if (buttons[i][STATUS]==PRESSED && buttons[i][STATE]!=STATE_LOCKED) {
      if (buttons[i][STATE]!=STATE_HELD) buttons[i][STATE]=STATE_PRESSED;
      else buttons[i][KEYFIRE_ELAPSE]++;
      if (buttons[i][TOTAL_ELAPSE]<255) buttons[i][TOTAL_ELAPSE]++;
    } else if (buttons[i][STATUS]==RELEASED && buttons[i][STATE]==STATE_PRESSED) {
      buttons[i][STATE]=STATE_RELEASED;
      buttons[i][PREV_ELAPSE]=buttons[i][TOTAL_ELAPSE];
      buttons[i][TOTAL_ELAPSE]=0;
      buttons[i][KEYFIRE_ELAPSE]=0;
    } else if (buttons[i][STATUS]==RELEASED && buttons[i][STATE]==STATE_RELEASED) {
      buttons[i][STATE]=STATE_RESET;
    } else if (buttons[i][STATUS]==RELEASED && (buttons[i][STATE]==STATE_HELD || buttons[i][STATE]==STATE_LOCKED)) {
      buttons[i][STATE]=STATE_RESET;
      buttons[i][PREV_ELAPSE]=0;
      buttons[i][TOTAL_ELAPSE]=0;
      buttons[i][KEYFIRE_ELAPSE]=0;
    }
  }
}

/* Check if button n has been held for t 1/20's of a second; r determines repeat setting; b determines if tactile feedback is given */
char held(char n, signed char t, signed char r, char b) {
    assert(n>0 && n<=INPUT_COUNT);
    n--;
    if (buttons[n][STATUS]==PRESSED && buttons[n][STATE]!=STATE_LOCKED && buttons[n][STATE]!=STATE_HELD) {
      buttons[n][STATE]=STATE_PRESSED;
    }
    if (buttons[n][TOTAL_ELAPSE]>=t && t != NO_START && buttons[n][KEYFIRE_ELAPSE]==0 && buttons[n][STATE]==STATE_PRESSED) {
      if (r == LOCK_BUTTON) buttons[n][STATE]=STATE_LOCKED;
      else buttons[n][STATE]=STATE_HELD;
      if (b) beep=TRUE;
      return TRUE;
    } else if (buttons[n][KEYFIRE_ELAPSE]>=r && r != NO_REPEAT && buttons[n][STATE]==STATE_HELD) {
      buttons[n][KEYFIRE_ELAPSE]=0;
      if (b) beep=TRUE;
      return TRUE;
    }
    else return FALSE;
}    

/* Check if button n has been held for t 1/20's of a second and then released; b determines if tactile feedback is given */
char released(char n, signed char t, char b) {
    assert(n>0 && n<=INPUT_COUNT);
    n--;
    if (buttons[n][STATUS]==RELEASED && buttons[n][STATE]==STATE_PRESSED) {
      buttons[n][STATE]=STATE_RELEASED;
      buttons[n][PREV_ELAPSE]=buttons[n][TOTAL_ELAPSE];
      buttons[n][TOTAL_ELAPSE]=0;
      buttons[n][KEYFIRE_ELAPSE]=0;
    }
    if (buttons[n][STATE]==STATE_RELEASED && buttons[n][PREV_ELAPSE]>=t) {
      buttons[n][PREV_ELAPSE]=0;
      buttons[n][STATE]=STATE_RESET;
      if (b) beep=TRUE;
      return TRUE;
    }      
    else return FALSE;
}

/* Helper function to convert a decimal value to BCD */
char dec2bcd(char n) { assert(n<100); return ((n / 10) << 4) | (n % 10); }

/* Helper fuction to convert a BCD value to decimal */
char bcd2dec(char n) { return ( ((n >> 4) * 10) + (n & 0x0F) ); }

/* Helper function to swap nibbles in a byte. This hack is because we
   wired the most significant digit to the the least significant seven
   segment digit and vice versa. Oops! */
char sn(char n) { return (n << 4) | (n >> 4); }

/* Saves the alarm for day d and writes the alarms to the EEPROM over the I2C bus */
void alarm_write(void) {
  buffer[0] = EEPROM_MSB_ADDR;
  buffer[1] = EEPROM_ALM_ADDR + ALARM_SIZE*alarm_day;
  buffer[2] = alarms[alarm_day][HOUR];
  buffer[3] = alarms[alarm_day][MIN];
  buffer[4] = alarms[alarm_day][ALM_ENABLE];
  i2c_write(EEPROM_ADDR, buffer, 5);
}

/* Reads the saved alarms from the EEPROM over the I2C bus */
void alarm_read(void) {
  char i, j;
  buffer[0] = EEPROM_MSB_ADDR;
  buffer[1] = EEPROM_ALM_ADDR;
  i2c_write(EEPROM_ADDR, buffer, 2);
  i2c_read(EEPROM_ADDR, buffer, (ALARM_SIZE * ALARM_COUNT));
  for (i=0; i<ALARM_COUNT; i++) {
    for (j=0; j<ALARM_SIZE; j++) {
      alarms[i][j] = buffer[(ALARM_SIZE*i + j)];
    }
  }
}

/* Checks the alarms for a match */
char alarm_check(void) {
  if (((alarms[time[DAY]][ALM_ENABLE] && alarms[time[DAY]][HOUR]==time[HOUR] && alarms[time[DAY]][MIN]==time[MIN]) 
        || (alarms[SNOOZE][ALM_ENABLE] && alarms[SNOOZE][HOUR]==time[HOUR] && alarms[SNOOZE][MIN]==time[MIN])) 
        && time[SEC]==0) return TRUE;
  else return FALSE;
}

/* Writes the time to the clock over the I2C bus */
void time_write(void) {
  if (debug) {
    debug_time[SEC] = time[SEC];
    debug_time[MIN] = time[MIN];
    debug_time[HOUR]= time[HOUR];
    debug_time[DAY] = time[DAY];
  }
  buffer[0] = CLOCK_SEC_ADDR;
  buffer[1] = (dec2bcd(time[SEC]) & SEC_MASK);
  buffer[2] = dec2bcd(time[MIN]) & MIN_MASK;
  buffer[3] = dec2bcd(time[HOUR]) & HOUR_MASK;
  buffer[4] = time[DAY] & DAY_MASK;
  i2c_write(CLOCK_ADDR, buffer, 5);
}

/* Reads the time from the clock over the I2C bus */
void time_read(void) {
  if (debug) {
    time[SEC] = debug_time[SEC];
    time[MIN] = debug_time[MIN];
    time[HOUR]=debug_time[HOUR];
    time[DAY] =debug_time[DAY];
  } else {  
    buffer[0] = CLOCK_SEC_ADDR;
    i2c_write(CLOCK_ADDR, buffer, 1);
    i2c_read(CLOCK_ADDR, buffer, 4);
    time[SEC] = bcd2dec(buffer[0] & SEC_MASK);
    time[MIN] = bcd2dec(buffer[1] & MIN_MASK);
    time[HOUR]= bcd2dec(buffer[2] & HOUR_MASK);
    time[DAY] = buffer[3] & DAY_MASK;
  }
}

/* Formats the time to either normal or military time for displaying */
char time_format(char hour) {
  if (mode[TIME_MODE] == NORMAL ) {
    if (hour>=12) output[0] = output[0] & ~AM_PM_MASK;
    else output[0] = output[0] | AM_PM_MASK;
    if (hour==0 | (hour>=10 && hour <=12) | (hour>=22 && hour <=23)) output[2] = output[2] & ~ONE_DIGIT_MASK;
    else output[2] = output[2] | ONE_DIGIT_MASK;
    if (hour==0) return 12;
    else if (hour>12) return hour-12;
    else return hour;   
  } else if (mode[TIME_MODE] == MILITARY) {
    if (hour>=10 && hour <=19) output[2] = output[2] & ~ONE_DIGIT_MASK;
    else output[2] = output[2] | ONE_DIGIT_MASK;
    output[0] = output[0] | AM_PM_MASK;
    return hour;
  }
}

/* Saves the time volatility to the clock chip */
void time_volatile(char b) {
  if (b == TRUE) {
    clock_set = TRUE;
    buffer[0] = CLOCK_MODE_ADDR;
    buffer[1] = CLOCK_SET;
    i2c_write(CLOCK_ADDR, buffer, 2);
  } else {
    clock_set = FALSE;
    buffer[0] = CLOCK_MODE_ADDR;
    buffer[1] = CLOCK_NOT_SET;
    i2c_write(CLOCK_ADDR, buffer, 2);
  }
};

/* Starts the iPod playing */
void ipod_play(void) {
  ipod_cmd_play();
  ipod_cmd_button_release();
  ipod_cmd_skip_back();
  ipod_cmd_button_release();
}

/* Pauses the iPod playing */
void ipod_pause(void) {
  ipod_cmd_pause();
  ipod_cmd_button_release();
}  

/* Stops the iPod playing */
void ipod_off (void) {
  ipod_cmd_stop();
  ipod_cmd_button_release();
  ipod_cmd_pause();
  off = TRUE;
}

/* Skips forward on the iPod */
void ipod_skip_forward(void) {
  ipod_cmd_skip_forward();
  ipod_cmd_button_release(); 
}

/* Skips back on the iPod */
void ipod_skip_back(void) {
  ipod_cmd_skip_back();
  ipod_cmd_button_release(); 
}

/* Increases the iPod volume */
void ipod_volume_up(void) {
  ipod_cmd_volume_up();
  ipod_cmd_button_release();
}

/* Decreases the iPod volume */
void ipod_volume_down(void) {
  ipod_cmd_volume_down();
  ipod_cmd_button_release();
}

/* Writes the play command to the iPod */
void ipod_cmd_play(void) {
  /* Play */
  sci_write(0xFF);              // Header
  sci_write(0x55);
  sci_write(0x04);              // Length
  sci_write(0x02);              // Mode
  sci_write(0x00);              // Command
  sci_write(0x00);
  sci_write(0x01);              
  sci_write(0xF9);              // Checksum
}

/* Writes the pause command to the iPod */
void ipod_cmd_pause(void) {
  /* Pause */
  sci_write(0xFF);              // Header
  sci_write(0x55);
  sci_write(0x03);              // Length
  sci_write(0x02);              // Mode
  sci_write(0x00);              // Command
  sci_write(0x01);              
  sci_write(0xFA);              // Checksum
}

/* Writes the stop command to the iPod */
void ipod_cmd_stop(void) {
  /* Stop */
  sci_write(0xFF);              // Header
  sci_write(0x55);
  sci_write(0x03);              // Length
  sci_write(0x02);              // Mode
  sci_write(0x00);              // Command
  sci_write(0x80);             
  sci_write(0x7B);              // Checksum
}

/* Writes the skip forward command to the iPod */
void ipod_cmd_skip_forward(void) {
  /* Skip back */
  sci_write(0xFF);              // Header
  sci_write(0x55);
  sci_write(0x03);              // Length
  sci_write(0x02);              // Mode
  sci_write(0x00);              // Command
  sci_write(0x08);              
  sci_write(0xF3);              // Checksum
}

/* Writes the skip back command to the iPod */
void ipod_cmd_skip_back(void) {
  /* Skip back */
  sci_write(0xFF);              // Header
  sci_write(0x55);
  sci_write(0x03);              // Length
  sci_write(0x02);              // Mode
  sci_write(0x00);              // Command
  sci_write(0x10);              
  sci_write(0xEB);              // Checksum
}

/* Writes the volume up command to the iPod */
void ipod_cmd_volume_up(void) {
  /* Volume up */
  sci_write(0xFF);              // Header
  sci_write(0x55);
  sci_write(0x03);              // Length
  sci_write(0x02);              // Mode
  sci_write(0x00);              // Command
  sci_write(0x02);              
  sci_write(0xF9);              // Checksum
}

/* Writes the volume down command to the iPod */
void ipod_cmd_volume_down(void) {
  /* Volume down */
  sci_write(0xFF);              // Header
  sci_write(0x55);
  sci_write(0x03);              // Length
  sci_write(0x02);              // Mode
  sci_write(0x00);              // Command
  sci_write(0x04);              
  sci_write(0xF7);              // Checksum
}

/* Writes the button release command to the iPod */
void ipod_cmd_button_release(void) {  
  /* Button release */        
  sci_write(0xFF);              // Header
  sci_write(0x55);
  sci_write(0x03);              // Length
  sci_write(0x02);              // Mode
  sci_write(0x00);              // Command
  sci_write(0x00);
  sci_write(0xFB);              // Checksum
}

/* Writes the byte ch to SCI port */
void sci_write(unsigned char ch) {
    while (SCS1_SCTE == 0);
    SCDR = ch;
}

/* Writes num_bytes bytes to an I2C device at address addr */
void i2c_write(char device_addr, char *p, char num_bytes) {
  i2c_start_timeout();          // Start I2C watchdog timer
  assert(num_bytes >= 1);
  while (MIMCR_MMBB)  {         // Wait for bus not busy
    if (i2c_timeout) return;
  }
  if (i2c_timeout) return;
  MMSR_MMTXIF = 0;              // Set MMDRR writable
  MIMCR_MMRW = 0;               // Set for transmit
  MMADR = device_addr;          // Device address -> address reg
  MMDTR = *p++;                 // First byte of data to write
  MIMCR_MMAST = 1;              // Start transmission
  while (MMSR_MMRXAK)  {
    if (i2c_timeout) return;
  }
  if (i2c_timeout) return;
  while (num_bytes-- > 0) {
    i2c_start_timeout();        // Start I2C watchdog timer
    while (!MMSR_MMTXBE) {      // Wait for TX buffer
      if (i2c_timeout) return;
    }
    if (i2c_timeout) return;
    if (num_bytes == 0)         // Is this the last byte?
      MMDTR = 0xFF;             // Yes. dummy data -> DTR
    else
      MMDTR = *p++;             // No. next data -> DTR
    while (MMSR_MMRXAK) {       // Wait for ACK from slave
      if (i2c_timeout) return;
    }
    if (i2c_timeout) return;
  }
  MIMCR_MMAST = 0;              // Generate STOP bit
  T1SC_TSTOP = 1;               // Stop I2C watchdog timer
}

/* Read num_bytes bytes from an I2C device at address addr */
void i2c_read(char device_addr, char *p, char num_bytes) {
    i2c_start_timeout();        // Start I2C watchdog timer
    assert(num_bytes >= 1);     
    while (MIMCR_MMBB) {
      if (i2c_timeout) return;
    }
    if (i2c_timeout) return;
    MMSR_MMRXIF = 0;
    MIMCR_MMRW = 1;             // Set for receive
    if (num_bytes == 1)
      MMCR_MMTXAK = 1;
    else
      MMCR_MMTXAK = 0;
    MMADR = device_addr;        // Device address -> address reg
    MMDTR = 0xFF;               // Dummy data to get ACK clock
    MIMCR_MMAST = 1;            // Initiate transfer    
    while (MMSR_MMRXAK) {       // Wait for ACK from slave
      if (i2c_timeout) return;
    }
    if (i2c_timeout) return;
    while (num_bytes-- > 0) {   // Up to last byte
        i2c_start_timeout();    // Start I2C watchdog timer
        while (!MMSR_MMRXBF) {  // Wait for RX buffer full
          if (i2c_timeout) return;
        }
        if (i2c_timeout) return;
        if (num_bytes == 1)
          MMCR_MMTXAK = 1;
        else
          MMCR_MMTXAK = 0;
        *p++ = MMDRR;           // Get data
    }
    MIMCR_MMAST = 0;            // Generate STOP bit
    T1SC_TSTOP = 1;             // Start I2C watchdog timer
}                               

/* Reset the I2C bus if the I2C watchdog timer expired */
void i2c_reset(void) {
    MMCR_MMEN = 0;
    MMCR_MMEN = 1;
    i2c_timeout = TRUE;
}

/* Start the I2C watchdog timer */
void i2c_start_timeout(void) {
    i2c_timeout = FALSE;
    T1SC_TRST = 1;              // Reset timer
    T1SC_PS = 6;                // Set prescalar for divide by 64
    T1SC_TOIE = 1;              // enable timer interrupt
    T1MOD = 65535;              // store modulo value in T1MODH:T1MODL
    T1SC_TSTOP = 0;             // start timer running  
}

/* The ISR for Timer 1 */
#pragma TRAP_PROC
void i2c_watchdog(void) {
    T1SC_TOF = 0;               // Reenable timer
    i2c_reset();
}