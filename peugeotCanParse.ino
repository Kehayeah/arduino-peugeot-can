#include <SPI.h>
#include "mcp_can.h"

MCP_CAN CAN(10);

// Enumeration representing the type of frames sent over serial
typedef enum {
    INIT_STATUS_FRAME    = 0x00,
    VOLUME_FRAME         = 0x01,
    RADIO_SOURCE_FRAME   = 0x03,
    RADIO_NAME_FRAME     = 0x04,
    RADIO_FREQ_FRAME     = 0x05,
    RADIO_FMTYPE_FRAME   = 0x06,
    RADIO_DESC_FRAME     = 0x07,
    INFO_MSG_FRAME       = 0x08,
    RADIO_STATIONS_FRAME = 0x09,
    INFO_TRIP1_FRAME     = 0x0C,
    INFO_TRIP2_FRAME     = 0x0D,
    INFO_INSTANT_FRAME   = 0x0E,
    TRIP_MODE_FRAME      = 0x0F,
    AUDIO_SETTINGS_FRAME = 0x10,
    IGNITION_FRAME       = 0x11,
    CD_ARTIST_FRAME      = 0x12,
    CD_TITLE_FRAME       = 0x13,
    USB_ARTIST_FRAME     = 0x14,
    USB_TITLE_FRAME      = 0x15,
    SECRET_FRAME         = 0x42, // Dark button
} FrameType;

int usbStart = 0;
int usbSplit = 0;

byte reverseEngaged = 0;

//Ignition State
byte ignition = 0;

//Volume 0-30
int volume = 0;

//Radio Source (FM, CD, AUX, USB, BT)
int radioSource = 0;

//FM Band (1, 2, AST, MW)
int fmType = 0;

//Radio Frequency
int fmFreq = 0;

//Station Name
char radioName[9];

//RDS
char radioMsg[100];
char msgRecvCount = 0;
char tempo[100];

//Artist CD
char cdTitle[100];
char cdArtist[100];

//USB Stuff
int usbLen = 0;
char usbTitle[100];
char usbArtist[100];

//Saved Stations
char stations[100];
char stationsRecvCount = 0;
char tempBuffer[100];

//Diagnostic Messages
byte messageInfo[8];

//Audio Settings Tab
byte audioSettings[7];

//Trip Info
byte infoTrip1[7];
byte infoTrip2[7];
byte infoInstant[7];

//Current Trip Displayed
byte tripMode = 0;

//Trip button state (not in low spec 206)
boolean tripModeButtonPressed = false;

boolean tripDidReset = false;
unsigned long timeSinceTripInfoButtonPressed = 0;

boolean firstHit = false;

//Dark button state
byte darkPressed = 0;
void setup() {
  // Initialize serial port and CAN bus shield
    Serial.begin(115200);

    while (CAN.begin(CAN_125KBPS) != CAN_OK) {
        // Retry until successful init
        sendByteWithType(INIT_STATUS_FRAME, 0x01);
        delay(100);
    }

    // Successful init
    sendByteWithType(INIT_STATUS_FRAME, 0x00);

}

void loop() {
  //Init Some needed temps
  unsigned char len = 0;
  byte buf[8];
  byte tmp[9];
  int tempValue;

  //Start Reading Loop
  if (CAN.checkReceive() == CAN_MSGAVAIL) {
    CAN.readMsgBuf(&len, buf);
    int id = CAN.getCanId();

    if (id == 246){
      //Handle Ignition on/off to turn off screen and halt pi afterwards
      tempValue = (buf[0] & 0x08) == 0x08;
      if (ignition != tempValue) {
          ignition = tempValue;

          sendByteWithType(IGNITION_FRAME, ignition);
      }

      
      //Handle Reverse for camera here
    }
    else if (id == 997){
      // Reset all data stored in memory when pressing MENU + ESC
      // Forces a resend of all data on the next loop() iteration
      if ((buf[2] & 0b00010000) && (buf[0] & 0b01000000)) {
          volume = 0;
          radioSource = 0;
          fmType = 0;
          fmFreq = 0;
          memset(radioName, 0, sizeof radioName);
          memset(radioMsg, 0, sizeof radioMsg);
          memset(stations, 0, sizeof stations);
          memset(messageInfo, 0, sizeof messageInfo);
          memset(infoTrip1, 0, sizeof infoTrip1);
          memset(infoTrip2, 0, sizeof infoTrip2);
          memset(infoInstant, 0, sizeof infoInstant);
          memset(audioSettings, 0, sizeof audioSettings);
          memset(cdArtist, 0, sizeof cdArtist);
          memset(cdTitle, 0, sizeof cdTitle);
          memset(usbArtist, 0, sizeof usbArtist);
          memset(usbTitle, 0, sizeof usbTitle);
          memset(tempo, 0, sizeof tempo);
          darkPressed = 0;
      }
      // Check whether the DARK button is pressed or not
      tempValue = (buf[2] & 0x04) == 0x04;
      if (darkPressed != tempValue) {
          darkPressed = tempValue;

          sendByteWithType(SECRET_FRAME, darkPressed);
      }
    }
    else if (id == 357) {
      // Radio source
      tempValue = buf[2] >> 4;
      if (radioSource != tempValue) {
          radioSource = tempValue;
  
          sendByteWithType(RADIO_SOURCE_FRAME, radioSource);
      }
      if (buf[0] == 0x40){
          memset(cdArtist, 0, sizeof cdArtist);
          memset(cdTitle, 0, sizeof cdTitle);
          memset(usbArtist, 0, sizeof usbArtist);
          memset(usbTitle, 0, sizeof usbTitle);
          memset(tempo, 0, sizeof tempo);
      }
    }
    else if (id == 421){
      //Handle Volume
      tempValue = buf[0] & 0b00011111;
      if (volume != tempValue) {
        volume = tempValue;
        sendByteWithType(VOLUME_FRAME, volume);
        //Maybe add isVolumeChanging
      }
    }
    else if (id == 677){
      //Radio Station Name. Greece Stations are lazy and add RDS text here.
      if (strncmp((char*)buf, radioName, len)) {
          strncpy(radioName, (char*)buf, len);

          sendFrameWithType(RADIO_NAME_FRAME, buf, len);
      }
    }
    else if (id == 549) {
      // Radio frequency
      tempValue = ((((buf[3] & 0xFF) << 8) + (buf[4] & 0xFF)) / 2 + 500);
      if (fmFreq != tempValue) {
          fmFreq = tempValue;
  
          byte freqBytes[] = { (fmFreq >> 8) & 0xFF, fmFreq & 0xFF };
          sendFrameWithType(RADIO_FREQ_FRAME, freqBytes, 2);
      }
  
      // FM type
      tempValue = buf[2] >> 4;
      if (fmType != tempValue) {
          fmType = tempValue;
  
          sendByteWithType(RADIO_FMTYPE_FRAME, fmType);
      }
    }
    else if (id == 164) {
      if (radioSource == 0x02){
        // If Source is CD, frame transmits CD Text
        // CD Artist / Track Name
    
        // Can-TP frame. Also splits the string to tile and artist.
        // PSA uses a weird logic for that: The Artist gets 20 characters only, the rest is tilte.
        // It's weird because the USB's frames don't have a limit on how many characters the artist can have
    
        if (buf[0] == 0x05) {
            msgRecvCount = 0;
        }
    
        if (buf[0] & 0x10) {
            msgRecvCount++;
    
            tempo[0] = buf[6];
            tempo[1] = buf[7];
    
            byte data[] = { 0x30, 0x00, 0x0A };
            CAN.sendMsgBuf(159, 0, 3, data);
        } else if (buf[0] & 0x20) {
            msgRecvCount++;
    
            int idx = buf[0] & 0x0F;
            for (int i = 1; i < len; i++) {
              if (buf[i] == 0x00){
                tempo[2 + (idx - 1) * 7 + (i - 1)] = 0x20;
              }else{
                tempo[2 + (idx - 1) * 7 + (i - 1)] = buf[i];
              }
            }
  
            //Pass temp variable into both Artist and Title variables, handling the extra spaces
            if (buf[0] == 0x26){
              for (int i = 0; i < 20; i++){
                if (tempo[i] == 0x20 && tempo[i + 1] == 0x20){
                  //We have reached the end and the rest are spaces so stop
                  cdArtist[i] = '\0';
                  break;
                }else{
                  cdArtist[i] = tempo[i];
                }
              }
              for (int i = 20; i < strlen(tempo); i++){
                if (tempo[i] == 0x20 && tempo[i + 1] == 0x20){
                  cdTitle[i-20] = '\0';
                  break;
                }else{
                  cdTitle[i-20] = tempo[i];
                }
              }
            }
        }
    
        if (msgRecvCount == 7) {
            sendFrameWithType(CD_ARTIST_FRAME, (byte*)cdArtist, strlen(cdArtist));
            sendFrameWithType(CD_TITLE_FRAME, (byte*)cdTitle, strlen(cdTitle));
            msgRecvCount = 0;
        }
      }
      else if (radioSource == 0x01){
      // If Source is Radio, then the frame transmits radio text. Greece doesn't use it but let's add it because why not
      // Radio text frame
      // Same method as before.

      if (buf[0] == 0x05) {
          msgRecvCount = 0;
      }

      if (buf[0] & 0x10) {
          msgRecvCount++;

          radioMsg[0] = buf[6];
          radioMsg[1] = buf[7];

          byte data[] = { 0x30, 0x00, 0x0A };
          CAN.sendMsgBuf(159, 0, 3, data);
      } else if (buf[0] & 0x20) {
          msgRecvCount++;

          int idx = buf[0] & 0x0F;
          for (int i = 1; i < len; i++) {
              radioMsg[2 + (idx - 1) * 7 + (i - 1)] = buf[i];
          }

          if (buf[0] == 0x29) {
              radioMsg[2 + (idx - 1) * 7 + (len - 1)] = '\0';
          }
      }

      if (msgRecvCount == 10) {
          sendFrameWithType(RADIO_DESC_FRAME, (byte*)radioMsg, strlen(radioMsg));
          msgRecvCount = 0;
      }
    }
    }
    else if (id == 739){
      // This frame contains info about the current track playing from USB.
      // Frame is CAN-TP with FCF being in 15F
      if (buf[0] == 0x05) {
          msgRecvCount = 0;
      }
      
      if (buf[0] == 0x01 && buf[1] == 0x60) {
            msgRecvCount = 0;
            usbStart = 0;
            usbLen = 0;
            memset(tempo, 0, sizeof tempo);
            memset(usbArtist, 0, sizeof usbArtist);
            memset(usbTitle, 0, sizeof usbTitle);
        }
    
        if (buf[0] & 0x10) {
            usbStart = 0;
            msgRecvCount++;
            usbLen = buf[1] & 0xFF;
            for (int i = 2; i < len; i++){
              tempo[i-2] = buf[i];
              usbStart++;
            }
    
            byte data[] = { 0x30, 0x00, 0x0A };
            CAN.sendMsgBuf(351, 0, 3, data);
        } else if (buf[0] & 0x20) {
            msgRecvCount++;
            
            int idx = buf[0] & 0x0F;
            for (int i = 1; i < len; i++) {
              if (buf[i] == 0x00 && firstHit == false){
                firstHit = true;
                usbSplit = usbStart + (idx - 1) * 7 + (i - 1);
                tempo[usbStart + (idx - 1) * 7 + (i - 1)] = 0x20;
              }else{
                tempo[usbStart + (idx - 1) * 7 + (i - 1)] = buf[i];
              }
            }
        }
    
        if (usbLen == strlen(tempo) + 1) {
          for (int i = 0; i < usbSplit; i++){
              usbArtist[i] = tempo[i];
          }
          for (int i = usbSplit + 1; i < strlen(tempo); i++){
              usbTitle[i-usbSplit-1] = tempo[i];
          }
          usbArtist[usbSplit] = '\0';
          usbTitle[strlen(tempo) - usbSplit] = '\0';
          firstHit = false;
          sendFrameWithType(USB_ARTIST_FRAME, (byte*)usbArtist, strlen(usbArtist));
          sendFrameWithType(USB_TITLE_FRAME, (byte*)usbTitle, strlen(usbTitle));
          msgRecvCount = 0;
        }
    }
    else if (id == 293) {
      // Memorized radio station names. Works roughly the same way as frame 164
      // (see above)
      if (buf[0] & 0x10) {
        stationsRecvCount = 0;

        tempBuffer[0] = buf[6];
        tempBuffer[1] = buf[7];
      } else if (buf[0] & 0x20) {
        stationsRecvCount++;

        int idx = buf[0] & 0x0F;
        for (int i = 1; i < len; i++) {
            tempBuffer[2 + (idx - 1) * 7 + (i - 1)] = buf[i];
        }

        if (buf[0] == 0x29) {
            tempBuffer[2 + (idx - 1) * 7 + (len - 1)] = '\0';
        }
      }

      if (stationsRecvCount == 8) {
        char* p = tempBuffer;
        while (*p != '\0') {
            if (*p == '\xA0' || *p == '\xB0' || *p == '\x90' || *p > 127) {
                // set separator between station names
                *p = '|';
            }

            p++;
        }

        if (strcmp(tempBuffer, stations)) {
            strcpy(stations, tempBuffer);
            sendFrameWithType(RADIO_STATIONS_FRAME, (byte*)stations, strlen(stations));
        }
        stationsRecvCount = 0;
      }
    }
    else if (id == 417) {
      // Information message frame
      // Pass it Raw and decode it in the app
      
      for (int i = 0; i < len; ++i) {
          tempBuffer[i] = buf[i];
      }

      if (memcmp(tempBuffer, messageInfo, 8)) {
          memcpy(messageInfo, tempBuffer, 8);

          sendFrameWithType(INFO_MSG_FRAME, messageInfo, 8);
      }
    }
    else if (id == 673 || id == 609 || id == 545) {
      // Trip computer data frames
      // There is 3 different frames (1 for each data set)
      // but they're all structured the same way
      byte* value;
      byte frameType;
  
      switch (id) {
      case 673:
          value = infoTrip1;
          frameType = INFO_TRIP1_FRAME;
          break;
      case 609:
          value = infoTrip2;
          frameType = INFO_TRIP2_FRAME;
          break;
      case 545:
          value = infoInstant;
          frameType = INFO_INSTANT_FRAME;
          break;
      }
  
      if (memcmp(buf, value, 7)) {
          memcpy(value, buf, 7);
  
          sendFrameWithType(frameType, value, 7);
      }
  
      if (id == 545) {
          // Special treatment for the instant data frame
          // which contains the trip data button state
          // I don't need it for my car but might as well copy it
          if ((buf[0] & 0x0F) == 0x08 && !tripModeButtonPressed) {
              tripModeButtonPressed = true;
              timeSinceTripInfoButtonPressed = millis();
          } else if ((buf[0] & 0x0F) == 0x00) {
              if (tripModeButtonPressed) {
                  if (!tripDidReset) {
                      tripMode++;
                      tripMode %= 3;
                      sendByteWithType(TRIP_MODE_FRAME, tripMode);
                  } else {
                      for (int i = 0; i < 50; i++) {
                          // We need to send this to actually stop the reset
                          // (else the distance/fuel counters never goes up again)
                          // FIXME: we should test explicitely the tripModes because it
                          // will currently reset the second memory if tripMode is equal
                          // to any value besides 1
                          byte data[] = { tripMode == 1 ? 0x02 : 0x04,
                              0x00,
                              0xFF,
                              0xFF,
                              0x00,
                              0x00,
                              0x00,
                              0x00 };
                          CAN.sendMsgBuf(359, 0, 8, data);
                      }
                  }
  
                  tripDidReset = false;
                  tripModeButtonPressed = false;
                  timeSinceTripInfoButtonPressed = 0;
              }
          }
      }
    }
    else if (id == 485) {
        // Audio settings frame
        // Same as information message frame: we send the raw frame and parse it afterwards
        for (int i = 0; i < 7; ++i) {
            tempBuffer[i] = buf[i];
        }
  
        if (memcmp(tempBuffer, audioSettings, 7)) {
            memcpy(audioSettings, tempBuffer, 7);
  
            sendFrameWithType(AUDIO_SETTINGS_FRAME, audioSettings, 7);
        }
    }
  }
}


/*
 A Literal copypasta from Alexandre:
 Serial packet structure:
 0x12   0x04    0xXX  0xXX  0xXX  0xXX  0x13
 start  length  type  `--   data   --´  end
 Escape sequence: 0x7e
 If any byte between start and end is 0x12, 0x13, or 0x7e,
 it is preceded by 0x7e and the byte is XOR'd by 0x20
*/

#define FRAME_START 0x12
#define FRAME_END 0x13
#define FRAME_ESCAPE 0x7E
#define ESCAPE_XOR 0x20

#define isControlChar(x) \
    (x == FRAME_START || x == FRAME_END || x == FRAME_ESCAPE)

byte serialBuffer[100];

void sendFrameWithType(byte frameType, const byte* data, int dataLength)
{
    int pos = 0;

    serialBuffer[pos++] = FRAME_START;

    byte lengthByte = dataLength + 1; // account for frame type
    if (isControlChar(lengthByte)) {
        serialBuffer[pos++] = FRAME_ESCAPE;
        lengthByte ^= ESCAPE_XOR;
    }

    serialBuffer[pos++] = lengthByte;

    if (isControlChar(frameType)) {
        serialBuffer[pos++] = FRAME_ESCAPE;
        frameType ^= ESCAPE_XOR;
    }

    serialBuffer[pos++] = frameType;

    for (int i = 0; i < dataLength; ++i) {
        byte b = data[i];
        if (isControlChar(b)) {
            serialBuffer[pos++] = FRAME_ESCAPE;

            b ^= ESCAPE_XOR;
        }

        serialBuffer[pos++] = b;
    }

    serialBuffer[pos++] = FRAME_END;

    Serial.write(serialBuffer, pos);
}

inline void sendByteWithType(byte frameType, byte byteToSend)
{
    byte arr[] = { byteToSend };
    sendFrameWithType(frameType, arr, 1);
}
