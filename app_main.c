#include "app_main.h"
#include "dev_HRF.h"
#include "decoder.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <bcm2835.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

// receive in variable length packet mode, display and resend. Data with swapped first 2 bytes
int main(int argc, char **argv){
	uint32_t sensorId = 0x102030;
	uint8_t manufacturerId = 0x04;
	uint8_t productId = 0x3;
	uint8_t encryptId = 0xf2;
	uint32_t data  = 0x1020;
    		
	int option;
	int quiet = 0;
	uint8_t queued_data = FALSE;
extern	uint8_t recieve_temp_report;
	
	while ((option = getopt(argc, argv, "e:m:p:s:d:q")) != -1)
	{
        
		switch(option)
		{
			case 'e':
				encryptId = strtoul(optarg, NULL, 0);
				
				break;
			case 'm':
				manufacturerId = strtoul(optarg, NULL, 0);
				
				break;
			case 'p':
				productId = strtoul(optarg, NULL, 0);
				
				break;
			case 's':
				sensorId = strtoul(optarg, NULL, 0);
				
				break;
			case 'd':
				data = strtoul(optarg, NULL, 0);
				 queued_data = TRUE;
				break;	
			case 'q':
				quiet = 1;
				break;
			default:
				printf("Syntax: %s [options]\n\n"
					   "\t-e\tEncryption ID to use\n"
					   "\t-m\tManufacturer ID to send\n"
					   "\t-p\tProduct ID to send\n"
					   "\t-s\tSensor ID to send\n"
					   "\t-d\tdata of TEMP SET to send\n"					   				   
					   "\t-q\tQuiet (send no comands)\n", argv[0]);
				return 2;
		}
	}	
	
    
	
	
	if (!quiet)
		printf("Sending to %02x:%02x:%06x, encryption %02x\n",
			   manufacturerId, productId, sensorId, encryptId);
			   
     if (queued_data) 
        {
        printf("queued data to be sent\n");			   
        } else {
        printf("No queued data to be sent, NIL command will be sent\n");      
        }
        
        
	if (!bcm2835_init())
		return 1;

	time_t currentTime;
	time_t monitorControlTime;

	
	// LED INIT
	bcm2835_gpio_fsel(LEDG, BCM2835_GPIO_FSEL_OUTP);			// LED green
	bcm2835_gpio_fsel(LEDR, BCM2835_GPIO_FSEL_OUTP);			// LED red
	bcm2835_gpio_write(LEDG, LOW);
	bcm2835_gpio_write(LEDR, LOW);
	// SPI INIT
	bcm2835_spi_begin();	
	bcm2835_spi_setClockDivider(SPI_CLOCK_DIVIDER_26); 			// 250MHz / 26 = 9.6MHz
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0); 				// CPOL = 0, CPHA = 0
	bcm2835_spi_chipSelect(BCM2835_SPI_CS1);					// chip select 1

	HRF_config_FSK();
	HRF_wait_for(ADDR_IRQFLAGS1, MASK_MODEREADY, TRUE);			// wait until ready after mode switching
	HRF_clr_fifo();

	monitorControlTime = time(NULL);
	while (1){
		currentTime = time(NULL);
		
		HRF_receive_FSK_msg(encryptId, productId, manufacturerId, sensorId );

		if (send_join_response)
		{
			printf("send JOIN response\n");
			HRF_send_FSK_msg(HRF_make_FSK_msg(join_manu_id, encryptId, join_prod_id, join_sensor_id,
											  2, PARAM_JOIN_RESP, 0), encryptId);
			send_join_response = FALSE;
		}
			
		if (recieve_temp_report)
		{
      
		   if (queued_data)
		    {
                                
			printf("send temp report\n");
			HRF_send_FSK_msg(HRF_make_FSK_msg(manufacturerId, encryptId, productId, sensorId,
											  4, PARAM_TEMP_SET, 0x92, (data & 0xff), (data >> 8 & 0xff)), encryptId);
			queued_data = FALSE;
	        recieve_temp_report = FALSE;
	        
  	    } else {
            printf("send NIL command\n");
			HRF_send_FSK_msg(HRF_make_FSK_msg(manufacturerId, encryptId, productId, sensorId,
											  0), encryptId);
			recieve_temp_report = FALSE;  
      }
      }
		
		
	/*	if (!quiet && difftime(currentTime, monitorControlTime) > 1)
		{
			monitorControlTime = time(NULL);
			static bool switchState = false;
			switchState = !switchState;
			printf("send temp message:\trelay %s\n", switchState ? "ON" : "OFF");
			bcm2835_gpio_write(LEDG, switchState);
			HRF_send_FSK_msg(HRF_make_FSK_msg(manufacturerId, encryptId, productId, sensorId,
											  4, PARAM_TEMP_SET, 0x92, 0x10, 0x20),
							 encryptId);
		}
		*/
	}
	bcm2835_spi_end();
	return 0;
}




