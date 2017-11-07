#include "Marlin.h"

#ifdef PAT9125

#include "fsensor.h"
#include "pat9125.h"
#include "planner.h"

//#include "LiquidCrystal.h"
//extern LiquidCrystal lcd;


#define FSENSOR_ERR_MAX      5  //filament sensor max error count
#define FSENSOR_INT_PIN     63  //filament sensor interrupt pin
#define FSENSOR_CHUNK_LEN  560  //filament sensor chunk length in steps

extern void stop_and_save_print_to_ram(float z_move, float e_move);
extern void restore_print_from_ram_and_continue(float e_move);
extern int8_t FSensorStateMenu;

void fsensor_stop_and_save_print()
{
	stop_and_save_print_to_ram(0, 0); //XYZE - no change
}

void fsensor_restore_print_and_continue()
{
	restore_print_from_ram_and_continue(0); //XYZ = orig, E - no change
}

uint8_t fsensor_int_pin = FSENSOR_INT_PIN;
int16_t fsensor_chunk_len = FSENSOR_CHUNK_LEN;
bool fsensor_enabled = true;
//bool fsensor_ignore_error = true;
bool fsensor_M600 = false;
uint8_t fsensor_err_cnt = 0;
int16_t fsensor_st_cnt = 0;


void fsensor_enable()
{
	MYSERIAL.println("fsensor_enable");
	fsensor_enabled = true;
//	fsensor_ignore_error = true;
	fsensor_M600 = false;
	fsensor_err_cnt = 0;
	eeprom_update_byte((uint8_t*)EEPROM_FSENSOR, 0xFF); 
	FSensorStateMenu = 1;
}

void fsensor_disable()
{
	MYSERIAL.println("fsensor_disable");
	fsensor_enabled = false;
	eeprom_update_byte((uint8_t*)EEPROM_FSENSOR, 0x00); 
	FSensorStateMenu = 0;
}

void pciSetup(byte pin)
{
	*digitalPinToPCMSK(pin) |= bit (digitalPinToPCMSKbit(pin)); // enable pin
	PCIFR |= bit (digitalPinToPCICRbit(pin)); // clear any outstanding interrupt
	PCICR |= bit (digitalPinToPCICRbit(pin)); // enable interrupt for the group 
}

void fsensor_setup_interrupt()
{
	uint8_t fsensor_int_pin = 63;

	pinMode(fsensor_int_pin, OUTPUT);
	digitalWrite(fsensor_int_pin, HIGH); 

	pciSetup(fsensor_int_pin);
}

ISR(PCINT2_vect)
{
//	return;
	int st_cnt = fsensor_st_cnt;
	fsensor_st_cnt = 0;
	sei();
	*digitalPinToPCMSK(fsensor_int_pin) &= ~bit(digitalPinToPCMSKbit(fsensor_int_pin));
	digitalWrite(fsensor_int_pin, HIGH);
	*digitalPinToPCMSK(fsensor_int_pin) |= bit(digitalPinToPCMSKbit(fsensor_int_pin));
	pat9125_update_y();
	if (st_cnt != 0)
	{
#ifdef DEBUG_FSENSOR_LOG
		MYSERIAL.print("cnt=");
		MYSERIAL.print(st_cnt, DEC);
		MYSERIAL.print(" dy=");
		MYSERIAL.print(pat9125_y, DEC);
#endif //DEBUG_FSENSOR_LOG
		if (st_cnt != 0)
		{
			if( (pat9125_y == 0) || ((pat9125_y > 0) && (st_cnt < 0)) || ((pat9125_y < 0) && (st_cnt > 0)))
			{ //invalid movement
				fsensor_err_cnt++;
#ifdef DEBUG_FSENSOR_LOG
				MYSERIAL.print("\tNG ! err=");
				MYSERIAL.println(fsensor_err_cnt, DEC);
#endif //DEBUG_FSENSOR_LOG
			}
			else
			{ //propper movement
				if (fsensor_err_cnt > 0)
					fsensor_err_cnt--;
#ifdef DEBUG_FSENSOR_LOG
				MYSERIAL.print("\tOK    err=");
				MYSERIAL.println(fsensor_err_cnt, DEC);
#endif //DEBUG_FSENSOR_LOG
			}
		}
		else
		{ //no movement
#ifdef DEBUG_FSENSOR_LOG
			MYSERIAL.println("\tOK 0");
#endif //DEBUG_FSENSOR_LOG
		}
	}
	pat9125_y = 0;
	return;
}

void fsensor_st_block_begin(block_t* bl)
{
	if ((fsensor_st_cnt > 0) && (bl->direction_bits & 0x8))
		digitalWrite(fsensor_int_pin, LOW);
	if ((fsensor_st_cnt < 0) && !(bl->direction_bits & 0x8))
		digitalWrite(fsensor_int_pin, LOW);
}

void fsensor_st_block_chunk(block_t* bl, int cnt)
{
	fsensor_st_cnt += (bl->direction_bits & 0x8)?-cnt:cnt;
	if ((fsensor_st_cnt >= fsensor_chunk_len) || (fsensor_st_cnt <= -fsensor_chunk_len))
		digitalWrite(fsensor_int_pin, LOW);
}

void fsensor_update()
{
	if (!fsensor_enabled) return;
	if (fsensor_err_cnt > FSENSOR_ERR_MAX)
	{
		MYSERIAL.println("fsensor_update (fsensor_err_cnt > FSENSOR_ERR_MAX)");
/*		if (fsensor_ignore_error)
		{
			MYSERIAL.println("fsensor_update - error ignored)");
			fsensor_ignore_error = false;
		}
		else*/
		{
			MYSERIAL.println("fsensor_update - ERROR!!!");
			fsensor_stop_and_save_print();
			enquecommand_front_P((PSTR("M600")));
			fsensor_M600 = true;
			fsensor_enabled = false;
		}
	}
}

#endif //PAT9125