#include "dev_HRF.h"
#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "decoder.h"
#include <time.h>
#include <string.h>

uint8_t send_join_response = FALSE;
uint8_t recieve_temp_report;

uint8_t join_manu_id;
uint8_t join_prod_id;
uint32_t join_sensor_id;

void HRF_config_FSK(){
	regSet_t regSetup[] = {
		{ADDR_REGDATAMODUL, VAL_REGDATAMODUL_FSK},	// modulation scheme FSK
		{ADDR_FDEVMSB, 		VAL_FDEVMSB30},  			// frequency deviation 5kHz 0x0052 -> 30kHz 0x01EC
		{ADDR_FDEVLSB, 		VAL_FDEVLSB30},			// frequency deviation 5kHz 0x0052 -> 30kHz 0x01EC
		{ADDR_FRMSB, 		VAL_FRMSB434},			// carrier freq -> 434.3MHz 0x6C9333
		{ADDR_FRMID, 		VAL_FRMID434},			// carrier freq -> 434.3MHz 0x6C9333
		{ADDR_FRLSB, 		VAL_FRLSB434},			// carrier freq -> 434.3MHz 0x6C9333
		{ADDR_AFCCTRL, 		VAL_AFCCTRLS},			// standard AFC routine
		{ADDR_LNA, 			VAL_LNA50},				// 200ohms, gain by AGC loop -> 50ohms
		{ADDR_RXBW, 		VAL_RXBW60},				// channel filter bandwidth 10kHz -> 60kHz  page:26
		//{ADDR_AFCFEI, 		VAL_AFCFEIRX},		// AFC is performed each time rx mode is entered
		//{ADDR_RSSITHRESH, 	VAL_RSSITHRESH220},	// RSSI threshold 0xE4 -> 0xDC (220)
		{ADDR_PREAMBLELSB, 	VAL_PREAMBLELSB3},		// preamble size LSB -> 3
		{ADDR_SYNCCONFIG, 	VAL_SYNCCONFIG2},		// Size of the Synch word = 2 (SyncSize + 1)
		{ADDR_SYNCVALUE1, 	VAL_SYNCVALUE1FSK},		// 1st byte of Sync word
		{ADDR_SYNCVALUE2, 	VAL_SYNCVALUE2FSK},		// 2nd byte of Sync word
		{ADDR_PACKETCONFIG1, VAL_PACKETCONFIG1FSK},// Variable length, Manchester coding, Addr must match NodeAddress
		{ADDR_PAYLOADLEN, 	VAL_PAYLOADLEN64},		// max Length in RX, not used in Tx
		{ADDR_NODEADDRESS, 	VAL_NODEADDRESS01},		// Node address used in address filtering
		{ADDR_FIFOTHRESH, 	VAL_FIFOTHRESH1},		// Condition to start packet transmission: at least one byte in FIFO
		{ADDR_OPMODE, 		MODE_RECEIVER}			// Operating mode to Receiver
	}; 
	uint8_t size = sizeof(regSetup)/sizeof(regSet_t), i;
	for (i=0; i<size; ++i){
		HRF_reg_W(regSetup[i].addr, regSetup[i].val);
	}
}
void HRF_config_OOK(){
	static regSet_t regSetup[] = {
		{ADDR_REGDATAMODUL, VAL_REGDATAMODUL_OOK},	// modulation scheme OOK
		{ADDR_FDEVMSB, 		0}, 					// frequency deviation -> 0kHz 
		{ADDR_FDEVLSB, 		0}, 					// frequency deviation -> 0kHz
		{ADDR_FRMSB, 		VAL_FRMSB433},			// carrier freq -> 433.92MHz 0x6C7AE1
		{ADDR_FRMID, 		VAL_FRMID433},			// carrier freq -> 433.92MHz 0x6C7AE1
		{ADDR_FRLSB, 		VAL_FRLSB433},			// carrier freq -> 433.92MHz 0x6C7AE1
		{ADDR_RXBW, 		VAL_RXBW120},			// channel filter bandwidth 120kHz
		//{0x03, 	0x40}, {0x04, 0x80},				// 1938b/s
		{0x03, 0x1A}, {0x04, 0x0B},					// 4800b/s
		//{0x03, 0x0D}, {0x04, 0x05},					// 9600b/s
		//{0x03, 0x06}, {0x04, 0x83},					// 19200b/s
		//{0x12, 0x09},
		{ADDR_PREAMBLELSB, 	0},						// preamble size LSB 0
		{ADDR_SYNCCONFIG, 	VAL_SYNCCONFIG4},		// Size of the Synch word = 4 (SyncSize + 1)
		{ADDR_SYNCVALUE1, 	VAL_SYNCVALUE1OOK},		// sync value 1
		{ADDR_SYNCVALUE2, 	0},						// sync value 2
		{ADDR_SYNCVALUE3, 	0},						// sync value 3
		{ADDR_SYNCVALUE4, 	0},						// sync value 4
		{ADDR_PACKETCONFIG1, VAL_PACKETCONFIG1OOK},	// Fixed length, no Manchester coding
		{ADDR_PAYLOADLEN, 	VAL_PAYLOADLEN_OOK},	// Payload Length
		{ADDR_FIFOTHRESH, 	VAL_FIFOTHRESH30},		// Condition to start packet transmission: wait for 30 bytes in FIFO
		{ADDR_OPMODE, 		MODE_TRANSMITER}		// Transmitter mode
	}; 
	uint8_t size = sizeof(regSetup)/sizeof(regSet_t), i;
	for (i=0; i<size; ++i){
		HRF_reg_W(regSetup[i].addr, regSetup[i].val);
	}
}
void HRF_clr_fifo(void){
	while (HRF_reg_R(ADDR_IRQFLAGS2) & MASK_FIFONOTEMPTY)				// FIFO FLAG FifoNotEmpty
	{
		HRF_reg_R(ADDR_FIFO);
	}
	return;
}
void HRF_reg_Rn(uint8_t *retBuf, uint8_t addr, uint8_t size){
	uint8_t tmp = retBuf[0];
	retBuf[0] = addr;
	bcm2835_spi_transfern((char*)retBuf, size + 1);
	retBuf[0] = tmp;
	return;
}
void HRF_reg_Wn(uint8_t *retBuf, uint8_t addr, uint8_t size){
	uint8_t tmp = retBuf[0];
	retBuf[0] = addr | MASK_WRITE_DATA;
	bcm2835_spi_writenb((char*)retBuf, size + 1);
	retBuf[0] = tmp;
	return;
}
uint8_t HRF_reg_R(uint8_t addr){
	uint8_t buf[2];
	buf[0] = addr;
	bcm2835_spi_transfern((char*)buf, 2);
	return buf[1];
}
void HRF_reg_W(uint8_t addr, uint8_t val){
	uint8_t buf[2];
	buf[0] = addr | MASK_WRITE_DATA;
	buf[1] = val;
	bcm2835_spi_writenb((char*)buf, 2);
	return;
}
void HRF_change_mode(uint8_t mode){
	uint8_t buf[2];
	buf[0] = ADDR_OPMODE | MASK_WRITE_DATA;
	buf[1] = mode;
	bcm2835_spi_writenb((char*)buf, 2);
}
void HRF_assert_reg_val(uint8_t addr, uint8_t mask, uint8_t val, char *desc){
	uint8_t buf[2];
	buf[0] = addr;
	bcm2835_spi_transfern((char*)buf, 2);
	if (val){
		if ((buf[1] & mask) != mask)
			printf("ASSERTION FAILED: addr:%02x, expVal:%02x(mask:%02x) != val:%02x, desc: %s\n", addr, val, mask, buf[1], desc);
	} else {
		if ((buf[1] & mask) != 0)
			printf("ASSERTION FAILED: addr:%02x, expVal:%02x(mask:%02x) != val:%02x, desc: %s\n", addr, val, mask, buf[1], desc);
	}
}
void HRF_wait_for(uint8_t addr, uint8_t mask, uint8_t val){
	uint32_t cnt = 0; 
	uint8_t ret;
	do {
		//++cnt;										// Uncomment to wait in a loop finite amount of time
		if (cnt > 4000000)
		{
			printf("timeout inside a while for addr %02x\n", addr);
			break;
		}
		ret = HRF_reg_R(addr);
	} while ((ret & mask) != (val ? mask : 0));
}
void HRF_send_OOK_msg(uint8_t relayState)
{
	uint8_t buf[17];
	uint8_t i;

	HRF_config_OOK();
	
	buf[1] = 0x80;				// Preambule 32b enclosed in sync words
	buf[2] = 0x00;
	buf[3] = 0x00;
	buf[4] = 0x00;
	
	for (i = 5; i <= 14; ++i){
		buf[i] = 8 + (i&1) * 6 + 128 + (i&2) * 48;				// address 20b * 4 = 10 Bytes
	}
	
	if (relayState == 1)
	{
		printf("relay ON\n\n");
	}
	else
	{
		printf("relay OFF\n\n");
	}
	
	if (relayState != 0)
	{
		//buf[15] = 0xEE;		// D0-high, D1-h		// All on
		//buf[16] = 0x8E;		// D2-h, D3-h
		buf[15] = 0xEE;		// D0-high, D1-h		// S1 on
		buf[16] = 0xEE;		// D2-h, D3-h
	}
	else
	{
		//buf[15] = 0xEE;		// D0-high, D1-h		// All off
		//buf[16] = 0x88;		// D2-h, D3-h
		buf[15] = 0xEE;		// D0-high, D1-h		// S1 off
		buf[16] = 0xE8;		// D2-h, D3-h
	}
	
	HRF_wait_for (ADDR_IRQFLAGS1, MASK_MODEREADY | MASK_TXREADY, TRUE);		// wait for ModeReady + TX ready
	//bcm2835_gpio_write(RPI_V2_GPIO_P1_15, 1);					// Turn the led on Test
	HRF_reg_Wn(buf + 4, 0, 12);		// Send few more same messages

	for (i = 0; i < 8; ++i)
	{
		HRF_wait_for(ADDR_IRQFLAGS2, MASK_FIFOLEVEL, FALSE);
		HRF_reg_Wn(buf, 0, 16);			// +4 sync bytes
	}

	HRF_wait_for (ADDR_IRQFLAGS2, MASK_PACKETSENT, TRUE);		// wait for Packet sent
	//bcm2835_gpio_write(RPI_V2_GPIO_P1_15, 0);					// Turn the led off Test
	HRF_assert_reg_val(ADDR_IRQFLAGS2, MASK_FIFONOTEMPTY | MASK_FIFOOVERRUN, FALSE, "are all bytes sent?");
	HRF_config_FSK();
	HRF_wait_for (ADDR_IRQFLAGS1, MASK_MODEREADY, TRUE);			// wait for ModeReady
}
uint8_t* HRF_make_FSK_msg(uint8_t manufacturerId, uint8_t encryptionId,
						  uint8_t productId, uint32_t sensorId, uint8_t paramNum, ...){
	uint8_t *msgData = (uint8_t*)malloc(MAX_FIFO_SIZE * sizeof(uint8_t));
	uint8_t i;
	va_list valist;
	//										msgData[0] reserved, reg used while sending
	msgData[MSG_REMAINING_LEN+1] = MSG_OVERHEAD_LEN + paramNum;
	msgData[MSG_MANUF_ID+1] = manufacturerId;
	msgData[MSG_PRODUCT_ID+1] = productId;
	msgData[MSG_RESERVED_HI+1] = rand();
	msgData[MSG_RESERVED_LO+1] = rand();
	msgData[MSG_SENSOR_ID_2+1] = (sensorId >> 16) & 0xff;
	msgData[MSG_SENSOR_ID_1+1] = (sensorId >> 8) & 0xff;
	msgData[MSG_SENSOR_ID_0+1] = sensorId & 0xFF;
	
	va_start(valist, paramNum);
	for (i = 0; i < paramNum; ++i)
	{
		msgData[MSG_DATA_START+1 + i] = va_arg(valist, uint);
	}
	va_end(valist);
	setupCrc(msgData + 1);
	encryptMsg(encryptionId, msgData + 1, msgData[MSG_REMAINING_LEN+1]);
	return msgData;
}
void HRF_send_FSK_msg(uint8_t* buf, uint8_t encryptionId){
	uint8_t size = buf[MSG_REMAINING_LEN+1], i;

	HRF_change_mode(MODE_TRANSMITER);									// Switch to TX mode
	HRF_wait_for(ADDR_IRQFLAGS1, MASK_MODEREADY | MASK_TXREADY, TRUE);		// wait for ModeReady + TX ready
	HRF_reg_Wn(buf, 0, size + 1);

	encryptMsg(encryptionId, buf + 1, buf[MSG_REMAINING_LEN+1]);
	printf("Send msg data: ");
	for (i=1; i <= buf[MSG_REMAINING_LEN+1] + 1 ; ++i)
		printf("%02x, ",buf[i]);
	printf("\n\n");

	HRF_wait_for(ADDR_IRQFLAGS2, MASK_PACKETSENT, TRUE);					// wait for Packet sent
	HRF_assert_reg_val(ADDR_IRQFLAGS2, MASK_FIFONOTEMPTY | MASK_FIFOOVERRUN, FALSE, "are all bytes sent?");

	HRF_change_mode(MODE_RECEIVER);												// Switch to RX mode
	HRF_wait_for(ADDR_IRQFLAGS1, MASK_MODEREADY, TRUE);						// wait for ModeReady

	free(buf);
	return;
}
#if 0
void decryptMsg(uint8_t *buf, uint8_t size){
	uint8_t i;
	seed(SEED_PID, (uint16_t)(buf[MSG_RESERVED_HI]<<8)|buf[MSG_RESERVED_LO]);
	for (i = MSG_ENCR_START; i <= size; ++i)
		buf[i] = decrypt(buf[i]);	
}
#endif
void encryptMsg(uint8_t encryptionId, uint8_t *buf, uint8_t size){
	uint8_t i;
	seed(encryptionId, (uint16_t)(buf[MSG_RESERVED_HI]<<8)|buf[MSG_RESERVED_LO]);
	for (i = MSG_ENCR_START; i <= size; ++i)
		buf[i] = decrypt(buf[i]);	
}
void setupCrc(uint8_t *buf){
	uint16_t val, size = buf[MSG_REMAINING_LEN];
	buf[size - 2] = 0;
	val = crc((uint8_t*)buf + MSG_ENCR_START, size - (MSG_ENCR_START+1));
	buf[size - 1] = val >> 8;
	buf[size] = val & 0x00FF;
}	
	
void HRF_receive_FSK_msg(uint8_t encryptionId, uint8_t productId, uint8_t manufacturerId, uint32_t sensorId )
{
	static uint16_t msg_cnt = 0;
	if ((HRF_reg_R(ADDR_IRQFLAGS2) & MASK_PAYLOADRDY) == MASK_PAYLOADRDY)
	{
		uint8_t recordBytesRead = 0;
		time_t receiveTime = time(NULL);
		struct tm *tmp;
		msg_t msg = {S_MSGLEN, 1, SIZE_MSGLEN, 0, 0, 0, 0, 0};	// message strucure instance
		receiveTime = time(NULL);
		tmp = gmtime(&receiveTime);
		printf("%02d:%02d:%02d %d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec, msg_cnt++);
		while (msg.state != S_FINISH)
		{
			if (msg.msgSize == 0){
				printf("\nERROR: Trying to read more data than should be read\n");
				msg.state = S_FINISH;
				break;
			}
			//HRF_wait_for(ADDR_IRQFLAGS2, MASK_FIFONOTEMPTY, TRUE);		// Not needed now. All message bytes should be already waiting in FIFO
			if (msg.state > S_ENCRYPTPIP)						// in states after S_ENCYPTPIP bytes need to be decrypted
			{
				msg.buf[msg.bufCnt++] = decrypt(HRF_reg_R(ADDR_FIFO));
			}
			else
			{
				msg.buf[msg.bufCnt++] = HRF_reg_R(ADDR_FIFO);
			}
			msg.value = (msg.value << 8) | msg.buf[msg.bufCnt - 1];
			++recordBytesRead;
			--msg.msgSize;

			if (recordBytesRead == msg.recordBytesToRead)
			{
				recordBytesRead = 0;
				msgNextState(encryptionId, productId, manufacturerId, sensorId, &msg);
				msg.value = 0;
			}
		}
		msgNextState(encryptionId, productId, manufacturerId, sensorId, &msg);
		                
	}
}



void msgNextState(uint8_t encryptionId, uint8_t productId, uint8_t manufacturerId, uint32_t sensorId, msg_t *msgPtr){		// Switch and initialize next state
	const char *temp;
	switch (msgPtr->state)
	{
		case S_MSGLEN:							// Read message length
			msgPtr->state = S_MANUFID;
			msgPtr->recordBytesToRead = SIZE_MANUF_ID;
			msgPtr->msgSize = msgPtr->value;
			break;
		case S_MANUFID:
               if (msgPtr->value == manufacturerId)
            {
                msgPtr->state = S_PRODID;
                msgPtr->recordBytesToRead = SIZE_PRODID;
                printf(" ManufacturerID=%#02x", msgPtr->value);
            }
            else
            {
                msgPtr->state = S_FINISH;
                msgPtr->msgSize = 0;
             //   msgPtr->crcResult = CRC_IGNORED;
			}
			break;
		case S_PRODID:							// Read product identifier
			
			  if (msgPtr->value == productId)
            {
                msgPtr->state = S_ENCRYPTPIP;
                msgPtr->recordBytesToRead = SIZE_ENCRYPTPIP;
                printf(" ProductID=%#02x\n", msgPtr->value);
            }
            else
            {
                msgPtr->state = S_FINISH;
                msgPtr->msgSize = 0;
            }
			break;
		case S_ENCRYPTPIP:						// Read encryption pip
			msgPtr->state = S_SENSORID;
			msgPtr->recordBytesToRead = SIZE_SENSORID;
			msgPtr->pip = msgPtr->value;
			seed(encryptionId, msgPtr->pip);
			break;
		case S_SENSORID:						// Read sensor ID		

		/*printf("test %#08x", sensorId);
		printf("test1 %#08x\n", (sensorId & 0xff));
     	printf("test2 %#08x\n", (msgPtr->value & 0xff));
		*/
		
			 if (((sensorId & 0xff) == (msgPtr->value & 0xff)) &&
                ((sensorId >> 8 & 0xff) == ((msgPtr->value >> 8) & 0xff)) &&
                ((sensorId >> 16 & 0xff) == ((msgPtr->value >> 16) & 0xff)))
            {
                msgPtr->state = S_DATA_PARAMID;
                msgPtr->recordBytesToRead = SIZE_DATA_PARAMID;
               	printf(" SensorID=%#08x", msgPtr->value);
            }
                        else
            {
                msgPtr->state = S_FINISH;
                msgPtr->msgSize = 0;
              
            }
			
			break;
	/******************* start reading RECORDS  ********************/
		case S_DATA_PARAMID:					// Read record parameter identifier
			msgPtr->paramId = msgPtr->value;
			temp = getIdName(msgPtr->paramId);
			printf(" %s=", temp);
			if (msgPtr->paramId == 0)			// Parameter identifier CRC. Go to CRC
			{
				msgPtr->state = S_CRC;
				msgPtr->recordBytesToRead = SIZE_CRC;
			}
			else
			{

				msgPtr->state = S_DATA_TYPEDESC;
				msgPtr->recordBytesToRead = SIZE_DATA_TYPEDESC;
				
				if (msgPtr->paramId == PARAM_JOIN_CMD)
				{
					msgPtr->gotJoin = 1;
					
				}
				
				
				if (msgPtr->paramId == PARAM_TEMP_REPORT)
				{
		
				   recieve_temp_report = TRUE;			
						printf(" %c=", recieve_temp_report);
				}
			}
			if (strcmp(temp, "Unknown") == 0)		// Unknown parameter, finish fetching message
				msgPtr->state = S_FINISH;
			break;
		case S_DATA_TYPEDESC:					// Read record type description

			if ((msgPtr->value & 0x0F) == 0)	// No more data to read in that record
			{
				msgPtr->state = S_DATA_PARAMID;
				msgPtr->recordBytesToRead = SIZE_DATA_PARAMID;
			}
			else
			{
				msgPtr->state = S_DATA_VAL;
				msgPtr->recordBytesToRead = msgPtr->value & 0x0F;
			}
			msgPtr->type = msgPtr->value;
			break;
		case S_DATA_VAL:						// Read record data
			temp = getValString(msgPtr->value, msgPtr->type >> 4, msgPtr->recordBytesToRead);
			printf("%s", temp);
			msgPtr->state = S_DATA_PARAMID;
			msgPtr->recordBytesToRead = SIZE_DATA_PARAMID;
			if (strcmp(temp, "Reserved") == 0)
				msgPtr->state = S_FINISH;
			break;
	/******************* finish reading RECORDS  ********************/
		case S_CRC:								// Check CRC
			msgPtr->state = S_FINISH;
			if ((int16_t)msgPtr->value == crc(msgPtr->buf + MSG_ENCR_START,
			                                  msgPtr->bufCnt - (MSG_ENCR_START+2)))
			{
				printf("OK\n");
				if (msgPtr->gotJoin)
				{
					join_manu_id = msgPtr->manufId;
					join_prod_id = msgPtr->prodId;
					join_sensor_id = msgPtr->sensorId;
					send_join_response = TRUE;
				}
			}
			else
			{
				printf("FAIL expVal=%04x, pip=%04x, val=%04x\n", (int16_t)msgPtr->value, msgPtr->pip, crc(msgPtr->buf + 4, msgPtr->bufCnt - 6));
			}
			break;
		case S_FINISH:							// Finishing state
			msgPtr->state = S_MSGLEN;
			msgPtr->recordBytesToRead = SIZE_MSGLEN;
			if (msgPtr->msgSize > 0)
				printf("\nShouldn't be there more data?!\n");
			msgPtr->msgSize = 1;
			int i;
			for (i = 0; i < msgPtr->bufCnt; ++i){
				printf("[%d]=%02x%c", i, msgPtr->buf[i], i%8==7?'\n':'\t');
			}
			printf("\n\n");
			msgPtr->bufCnt = 0;
			msgPtr->value = 0;
			HRF_clr_fifo();						// If there is an error, remaining of the message should be discarded
	
			break;
		default:
			printf("\nYou are in an non existing state!!");
	}
}
char* getIdName(uint8_t val){
	static char name[2];
	switch (val){
		case PARAM_JOIN_CMD:
			return "Join";
		case PARAM_JOIN_RESP:
			return "Join_response";
		case PARAM_POWER:
			return "Power";
		case PARAM_REACTIVE_P:
			return "Reactive_P";
		case PARAM_VOLTAGE:
			return "Voltage";
		case PARAM_CURRENT:
			return "Current";
		case PARAM_ACTUATE_SW:
			return "Actuate_switch";
		case PARAM_FREQUENCY:
			return "Frequency";
		case PARAM_TEST:
			return "Test";
		case PARAM_SW_STATE:
			return "Switch_state";
		case PARAM_CRC:
			return "CRC";
		default:
			if ((val>='a' && val <= 'z') || (val >= 'A' && val <= 'Z'))
			{
				name[0] = (char)val;
				name[1] = '\0';
				return name;
			}
			return "Unknown";
	}
}
char* getValString(uint64_t dataVal, uint8_t type, uint8_t length){
	static char str[20];
	if (type >= 0 && type <= 6){				// unsigned integer
		sprintf(str, "%g", (double)dataVal / (1 << (4*type)));
	} else if (type == 7) {						// characters
		int8_t i;
		char *ch = (char*)&dataVal;
		char *s = str;

		*s++='"';
		for (i = length - 1; i >= 0; --i)
			*s++ = ch[i];
		*s++='"';
		*s = '\0';
	} else if (type >= 8 && type <= 11){		// signed integer
		if (dataVal & (1uLL << (length*8-1))){		// check neg
			int8_t i;
			for (i = 0; i < length; ++i)
				dataVal ^= 0xFFuLL << 8*i;
			sprintf(str, "-%g", (double)(dataVal+1) / (1 << (8*(type-8))));
		} else
			sprintf(str, "%g", (double)dataVal / (1 << (8*(type-8))));
	} else if (type == 15){						// floating point
		if (dataVal & (1uLL << (length*8-1))){		// check neg
			int8_t i;
			for (i = 0; i < length; ++i)
				dataVal ^= 0xFFuLL << 8*i;
			sprintf(str, "-%g", (double)(dataVal+1) / (1 << (length == 2? 11 : length == 4 ? 24 : 53)));
		} else
			sprintf(str, "%g", (double)dataVal / (1 << (length == 2? 11 : length == 4 ? 24 : 53)));
	} else {									// reserved
		sprintf(str, "Reserved");
	}
	return str;
}
