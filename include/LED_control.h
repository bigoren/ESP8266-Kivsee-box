#define COLOR_FIELD_MASK   0x07
//#define PATTERN_FIELD_MASK 0x0C
#define NUMBER_FIELD_MASK  0x30

// void checkWinStatus() {
//   if(millis() - winTime > winLengthMs)
//   {
//     state = state & ~(WIN_STATE);
//     master_state = Pattern;
//     power = 0;
//   }
// }

// master_state -> Off=0, Pattern=1, Color=2

void set_leds(byte state, byte master_state ) {
  CHSV ledsCHSV[NUM_LEDS];
  if (master_state == 0) {
    FastLED.clear();
    return;
  }
  if (master_state == 1) {
    fadeToBlackBy( leds, NUM_LEDS, 10);
    int pos = random16(NUM_LEDS);
    leds[pos] += CHSV( random8(128), 200, 255);
    return;
  }
  // state over 127 means WIN_STATE was reached, play victory sequence
  if (state > 127) {
    {
      fill_rainbow(leds, NUM_LEDS, beat8(60), 256 / NUM_LEDS);
      if(random8() < 64) {
        leds[random(NUM_LEDS)] = CRGB::White;
      }
    }
    return;
  }
  // after special cases state can be checked for COLOR, PATTERN and NUMBER and set leds accordingly
  switch (state & COLOR_FIELD_MASK) {
    case 0x01 :
      fill_solid(ledsCHSV, NUM_LEDS, CHSV(0 * 256 / 5, 255, 255));
      break;
    case 0x02:
      fill_solid(ledsCHSV, NUM_LEDS, CHSV(1 * 256 / 5, 255, 255));
      break;
    case 0x03:
      fill_solid(ledsCHSV, NUM_LEDS, CHSV(2 * 256 / 5, 255, 255));
      break;
    case 0x04:
      fill_solid(ledsCHSV, NUM_LEDS, CHSV(3 * 256 / 5, 255, 255));
      break;
    case 0x05:
      fill_solid(ledsCHSV, NUM_LEDS, CHSV(4 * 256 / 5, 255, 255));
      break;
  }
//  switch ( (state & MOTION_FIELD_MASK) >> 4) {
//    case 0x00:
//      {
//        // blink
//        uint8_t brightness = beatsin8(30, 64, 255);
//        for(int i=0; i<NUM_LEDS; i++) {
//          ledsCHSV[i].val = brightness;
//        }
//      }
//      break;
//    case 0x01:
//      {


        uint8_t snakeHeadLoc = beat8(30) / RING_LEDS;
        for (byte i=0; i < RING_LEDS; i++) {
          uint8_t distanceFromHead = (i - snakeHeadLoc + RING_LEDS) % RING_LEDS;
          const int snakeLength = 8;
          uint8_t brightness = distanceFromHead > snakeLength ? 255 : 255 - ((int)distanceFromHead * (256 / snakeLength));
          for (byte j=0; j < RINGS; j++) {
            ledsCHSV[i+(j*RING_LEDS)].val = brightness;
          }
        }


//      }
//      break;
//    case 0x02:
//      // static
//      break;
//    case 0x03:
//      //flicker
//      {
//        static bool isOn = true;
//        if(random8() < (isOn ? 40 : 24))
//          isOn = !isOn;
//        uint8_t brightness = isOn ? 255 : 0;
//        for(int i=0; i<NUM_LEDS; i++) {
//          ledsCHSV[i].val = brightness;
//        }
//      }
//      break;
//  }

// NO PATTERN PARAMETER IN THIS GAME
//  switch ((state & PATTERN_FIELD_MASK) >> 2) {
//    case 0x00 :
//      {
//        // Turn off even leds for dotted pattern
//        for (byte i=0; i < NUM_LEDS/2; i++) {
//          ledsCHSV[i*2].val = 0;
//        }
//      }
//      break;
//    case 0x01:
//      {
//        // Turn off first half
//        for (byte i=0; i < RING_LEDS/2; i++) {
//          for (byte j=0; j < RINGS; j++) {
//            ledsCHSV[i+(j*RING_LEDS)].val = 0;
//          }
//        }
//      }
//      break;
//    case 0x02:
//      {
//         // full
//      }
//      break;
//    case 0x03:
//      {
//        // Turn off quarters
//        for (byte j=0; j < RINGS; j++) {
//          for (byte i=0; i < RING_LEDS/4; i++) {
//            ledsCHSV[i+(j*RING_LEDS)].val = 0;
//            ledsCHSV[RING_LEDS/2 + i + (j*RING_LEDS)].val = 0;
//          }
//        }
//      }
//      break;
//  }

  // Turn off rings according to number field
//   byte rings = (state & NUMBER_FIELD_MASK) >> 4;
//   for (byte j=3; j > rings; j--) {
//     for (byte i=0; i < RING_LEDS; i++) {
//       ledsCHSV[i+(j*RING_LEDS)].val = 0;
//     }
//   }
  
  for(byte i=0; i<NUM_LEDS; i++) {
    leds[i] = (CRGB)(ledsCHSV[i]);
  }
}
