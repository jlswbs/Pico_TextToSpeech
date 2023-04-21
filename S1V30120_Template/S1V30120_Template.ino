// TextToSpeech click EPSON S1V30120 example //

#include "PicoSPI.h"
#include "text_to_speech_img.h"

#define S1V30120_CLK    2
#define S1V30120_MOSI   3
#define S1V30120_MISO   4
#define S1V30120_CS     5
#define S1V30120_RST    13
#define S1V30120_RDY    17
#define S1V30120_MUTE   26

#define ISC_VERSION_REQ         0x0005
#define ISC_BOOT_LOAD_REQ       0x1000
#define ISC_BOOT_RUN_REQ        0x1002
#define ISC_TEST_REQ            0x0003

#define ISC_AUDIO_CONFIG_REQ    0x0008
#define ISC_AUDIO_VOLUME_REQ    0x000A
#define ISC_AUDIO_MUTE_REQ      0x000C

#define ISC_TTS_CONFIG_REQ      0x0012
#define ISC_TTS_SAMPLE_RATE     0x01
#define ISC_TTS_VOICE           0x00
#define ISC_TTS_EPSON_PARSE     0x01
#define ISC_TTS_LANGUAGE        0x00
#define ISC_TTS_SPEAK_RATE_LSB  0xC8
#define ISC_TTS_SPEAK_RATE_MSB  0x00
#define ISC_TTS_DATASOURCE      0x00
#define ISC_TTS_SPEAK_REQ       0x0014

#define ISC_VERSION_RESP        0x0006
#define ISC_BOOT_LOAD_RESP      0x1001
#define ISC_BOOT_RUN_RESP       0x1003
#define ISC_TEST_RESP           0x0004
#define ISC_AUDIO_CONFIG_RESP   0x0009
#define ISC_AUDIO_VOLUME_RESP   0x000B
#define ISC_AUDIO_MUTE_RESP     0x000D
#define ISC_TTS_CONFIG_RESP     0x0013
#define ISC_TTS_SPEAK_RESP      0x0015
#define ISC_ERROR_IND           0x0000
#define ISC_MSG_BLOCKED_RESP    0x0007
#define ISC_TTS_FINISHED_IND    0x0021

#define TTS_AUDIO_CONF_AS   0x00 // mono
#define TTS_AUDIO_CONF_AG   0x43 // gain +18db
#define TTS_AUDIO_CONF_AMP  0x00 // amp deactivate
#define TTS_AUDIO_CONF_ASR  0x01 // 16kHz samplerate
#define TTS_AUDIO_CONF_AR   0x00 // routing to DAC
#define TTS_AUDIO_CONF_ATC  0x00 // audio tone
#define TTS_AUDIO_CONF_ACS  0x00 // audio click
#define TTS_AUDIO_CONF_DC   0x01 // DAC

String mytext = "Art is not limited to just paintings and sculptures, and circuitry is no exception.";

char rcvd_msg[20] = {0};
static volatile char send_msg[200] = {0};
static volatile unsigned short msg_len;
static volatile unsigned short txt_len;

unsigned short tmp;
long idx;

static volatile unsigned short TTS_DATA_IDX;

void S1V30120_reset(void){

  digitalWrite(S1V30120_CS, HIGH); 
  digitalWrite(S1V30120_RST, LOW);
  PicoSPI0.transfer(0x00);
  delay(5);
  digitalWrite(S1V30120_RST, HIGH);
  delay(150);

}

unsigned short S1V30120_get_version(void){
  
    unsigned short S1V30120_version = 0;
    unsigned short tmp_disp;
    char msg_ver[] = {0x04, 0x00, 0x05, 0x00};
    S1V30120_send_message(msg_ver, 0x04);
    while(digitalRead(S1V30120_RDY) == 0);
    digitalWrite(S1V30120_CS, LOW);
    while(PicoSPI0.transfer(0x00) != 0xAA);
    for (int i = 0; i < 20; i++) rcvd_msg[i]= PicoSPI0.transfer(0x00);
    S1V30120_send_padding(16);
    digitalWrite(S1V30120_CS, HIGH);
    S1V30120_version = rcvd_msg[4] << 8 | rcvd_msg[5];
    return S1V30120_version;

}

bool S1V30120_download(void){

   unsigned short len = sizeof (TTS_INIT_DATA);
   unsigned short fullchunks;
   unsigned short remaining;
   long data_index = 0;
   fullchunks = len / 2044;
   remaining = len - fullchunks * 2044;
   for (int num_chunks = 0; num_chunks < fullchunks; num_chunks++) S1V30120_load_chunk (2044);
   S1V30120_load_chunk (remaining);

  return 1;
  
}

bool S1V30120_load_chunk(unsigned short chunk_len){

  char len_msb = ((chunk_len + 4) & 0xFF00) >> 8;
  char len_lsb = (chunk_len + 4) & 0xFF;
  digitalWrite(S1V30120_CS, LOW);
  PicoSPI0.transfer(0xAA);
  PicoSPI0.transfer(len_lsb);
  PicoSPI0.transfer(len_msb);
  PicoSPI0.transfer(0x00);
  PicoSPI0.transfer(0x10);
  for (int chunk_idx = 0; chunk_idx < chunk_len; chunk_idx++){
    PicoSPI0.transfer(TTS_INIT_DATA[TTS_DATA_IDX]);
    TTS_DATA_IDX++;
  }
  digitalWrite(S1V30120_CS, HIGH);
  return S1V30120_parse_response(ISC_BOOT_LOAD_RESP, 0x0001, 16);
  
}

bool S1V30120_boot_run(void){

  char boot_run_msg[] = {0x04, 0x00, 0x02, 0x10};
  S1V30120_send_message(boot_run_msg, 0x04);
  return S1V30120_parse_response(ISC_BOOT_RUN_RESP, 0x0001, 8);
    
}

bool S1V30120_registration(void){

  char reg_code[] = {0x0C, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  S1V30120_send_message(reg_code, 0x0C);
  return S1V30120_parse_response(ISC_TEST_RESP, 0x0000, 16);

}

bool S1V30120_parse_response(unsigned short expected_message, unsigned short expected_result, unsigned short padding_bytes){

    unsigned short rcvd_tmp;
    while(digitalRead(S1V30120_RDY) == 0);
    digitalWrite(S1V30120_CS, LOW);
    while(PicoSPI0.transfer(0x00) != 0xAA);
    for (int i = 0; i < 6; i++) rcvd_msg[i]= PicoSPI0.transfer(0x00);
    S1V30120_send_padding(padding_bytes);
    digitalWrite(S1V30120_CS, HIGH);
    rcvd_tmp = rcvd_msg[3] << 8 | rcvd_msg[2];
    if (rcvd_tmp == expected_message){
       rcvd_tmp = rcvd_msg[5] << 8 | rcvd_msg[4];
       if (rcvd_tmp == expected_result) // OK, return 1
         return 1;
       else
         return 0;
    }
  else
  return 0;

}

void S1V30120_send_padding(unsigned short num_padding_bytes){
  
  for (int i = 0; i < num_padding_bytes; i++) PicoSPI0.transfer(0x00);

}

void S1V30120_send_message(volatile char message[], unsigned char message_length){

  while(digitalRead(S1V30120_RDY) == 1);
  digitalWrite(S1V30120_CS, LOW);
  PicoSPI0.transfer(0xAA);
  for (int i = 0; i < message_length; i++) PicoSPI0.transfer(message[i]);

}

bool S1V30120_configure_audio(void){

  msg_len = 0x0C;
  send_msg[0] = msg_len & 0xFF;
  send_msg[1] = (msg_len & 0xFF00) >> 8;
  send_msg[2] = ISC_AUDIO_CONFIG_REQ & 0xFF;
  send_msg[3] = (ISC_AUDIO_CONFIG_REQ & 0xFF00) >> 8;
  send_msg[4] = TTS_AUDIO_CONF_AS;
  send_msg[5] = TTS_AUDIO_CONF_AG;
  send_msg[6] = TTS_AUDIO_CONF_AMP;
  send_msg[7] = TTS_AUDIO_CONF_ASR;
  send_msg[8] = TTS_AUDIO_CONF_AR;
  send_msg[9] = TTS_AUDIO_CONF_ATC;
  send_msg[10] = TTS_AUDIO_CONF_ACS;
  send_msg[11] = TTS_AUDIO_CONF_DC;
  S1V30120_send_message(send_msg, msg_len);
  return S1V30120_parse_response(ISC_AUDIO_CONFIG_RESP, 0x0000, 16);
  
}

bool S1V30120_set_volume(void){
  
  char setvol_code[]={0x06, 0x00, 0x0A, 0x00, 0x00, 0x00};
  S1V30120_send_message(setvol_code, 0x06);
  return S1V30120_parse_response(ISC_AUDIO_VOLUME_RESP, 0x0000, 16);
    
}

bool S1V30120_configure_tts(void){

  msg_len = 0x0C;
  send_msg[0] = msg_len & 0xFF;
  send_msg[1] = (msg_len & 0xFF00) >> 8;
  send_msg[2] = ISC_TTS_CONFIG_REQ & 0xFF;
  send_msg[3] = (ISC_TTS_CONFIG_REQ & 0xFF00) >> 8;
  send_msg[4] = ISC_TTS_SAMPLE_RATE;
  send_msg[5] = ISC_TTS_VOICE;
  send_msg[6] = ISC_TTS_EPSON_PARSE;
  send_msg[7] = ISC_TTS_LANGUAGE;
  send_msg[8] = ISC_TTS_SPEAK_RATE_LSB;
  send_msg[9] = ISC_TTS_SPEAK_RATE_MSB;
  send_msg[10] = ISC_TTS_DATASOURCE;
  send_msg[11] = 0x00;
  S1V30120_send_message(send_msg, msg_len);
  return S1V30120_parse_response(ISC_TTS_CONFIG_RESP, 0x0000, 16);
  
}

bool S1V30120_speech(String text_to_speech, unsigned char flush_enable){
  
  bool response;
  txt_len = text_to_speech.length();
  msg_len = txt_len + 6;
  send_msg[0] = msg_len & 0xFF;
  send_msg[1] = (msg_len & 0xFF00) >> 8;
  send_msg[2] = ISC_TTS_SPEAK_REQ & 0xFF;
  send_msg[3] = (ISC_TTS_SPEAK_REQ & 0xFF00) >> 8;
  send_msg[4] = flush_enable;
  for (int i = 0; i < txt_len; i++) send_msg[i+5] = text_to_speech[i];
  send_msg[msg_len-1] = '\0';
  S1V30120_send_message(send_msg, msg_len);
  response = S1V30120_parse_response(ISC_TTS_SPEAK_RESP, 0x0000, 16);
  while (!S1V30120_parse_response(ISC_TTS_FINISHED_IND, 0x0000, 16)); 
  return response;
  
}


void setup(){
  
  PicoSPI0.configure (S1V30120_CLK, S1V30120_MOSI, S1V30120_MISO, S1V30120_CS, 750000, 3, true);

  pinMode(S1V30120_RST, OUTPUT);
  pinMode(S1V30120_RDY, INPUT);
  pinMode(S1V30120_CS, OUTPUT);
  pinMode(S1V30120_MUTE, OUTPUT);
  digitalWrite(S1V30120_MUTE, LOW);

  S1V30120_reset();

  tmp = S1V30120_get_version();
  if (tmp == 0x0402) {} 
  
  S1V30120_download();
  S1V30120_boot_run();
  
  delay(150);
  
  S1V30120_registration();
  S1V30120_configure_audio();
  S1V30120_set_volume();
  S1V30120_configure_tts();
  
}

void loop(){

  S1V30120_speech(mytext,1);
  delay(1000);
    
}