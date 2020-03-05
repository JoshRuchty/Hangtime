
//▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
//Definitions

//PN532 code ; If using the PN532 with I2C, define the pins connected to the IRQ and reset lines as well.
//You MUST ALSO CONNECT SDA & SCL with i2c Mode! (Nano pins A4/A5) -Josh
#define PN532_IRQ   (2)
#define PN532_RESET (3)


//fastLED definitions

#define LED_PIN     7
#define COLOR_ORDER GRB  // interesting...
#define CHIPSET     WS2812
#define NUM_LEDS    40
#define BASE_LED    8   // the first LEDs on the bottom to be used 
#define BRIGHTNESS  255 // up to 255
#define FRAMES_PER_SECOND 60

//▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
//LIBRARIES

//PN532
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
// use this line for a breakout or shield with an I2C connection:
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET); //

//fastLED
#include <FastLED.h> //See fastLED example file fire2012 for original code comments and explanation
CRGB leds[NUM_LEDS];


//▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
//Global Variables


//Game settings
long roundLength = 5000; //in milliseconds
byte numOfRounds = 6;
long rndExpireTime; // when set by pendingPlayer(), sets the millis time that ends the round
byte numOfPlayers = 2;

//Rfid  info
byte uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned Uid from Library
byte uidLength;                        // Length of the Uid (4 or 7 ints depending on ISO14443A card type)
byte playerColors [11][3] = {{0, 0, 0}, {0, 255, 0}, {0, 0, 255}}; //player x RGB/HSV value
byte playerIDs [11][4]; // first dimension (11) is the number of possible players; 2nd is for the 4 blocks of RFID #
// note that for a 10 player game, I've reserved 11 spaces; this is because int playerTurn
//points to the player number in the array so instead of [0][], player one's uid is at playerIDs [1]
byte p1ID[] = {60, 166, 231, 11}; //red tag I think
byte p2ID[] = {76, 132, 231, 11}; // hard coded for now
bool success = 0;


//Misc bg processes etc.
byte currentRound = 0;
bool gameOver = 0;
bool gameIsLive = 0;
int playerScores[11]; // playerScores[0] = coop group score; playerScores[1]=p1 score; can make this multi-dimensional for roundscores
//round victor calculated by comparing p
byte playerTurn = 1;
int tagDebounce = 200; //used as milliseconds between tagReads before it's recognized
unsigned long lastTag = 0;
byte timerPos = 0;
byte hsTimerLum = 0; // 40 or whatever. an independent array on which to do math for the timer
byte showAllScores [NUM_LEDS][3]; //an array for showing the current scores


//▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
//SETUP
//▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
void setup(void) {
  Serial.begin(115200);


  //For PN532
  nfc.begin();
  nfc.setPassiveActivationRetries(2); //keeps code from searching for card indefinitely
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }
  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata >> 24) & 0xFF, HEX);  Serial.print("Firmware ver. "); Serial.print((versiondata >> 16) & 0xFF, DEC);  Serial.print('.'); Serial.println((versiondata >> 8) & 0xFF, DEC);
  nfc.SAMConfig();
  Serial.println("Waiting for an ISO14443A Card ...");
  //End PN532 setup code: see pn532 example sketch included with library

  //fastLED
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness( BRIGHTNESS );

  //My Code
  assignPlayer(1, p1ID); //make sure to adjust the TotalPlayers variable
  assignPlayer(2, p2ID);


  //startAnimation();
}//end setup

//▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
//    Loop
//▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
void loop() {
  delay(1000);
  clearUid();
  //Serial.print("beforeuid");
  getUid();
  //Serial.print("afteruid");
  printInfo();


  if (gameIsLive) {
    timerCheck();
    addPlayerPoint();
    //showPlayerScores();
    //CountDownLed();
  }
  pendingPlayer();


  // LED display to reflect playerTurn, time left, & all player scores
  //HS stands for High Score Game


  FastLED.show(); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);
  //Serial.println("//end main loop");

} //end main loop
//▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
//    FUNCTIONS
//▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒



void touchAnimate() {
  //Serial.println("touchanimate()");
  if (isTurnTag) {
    for (byte i = NUM_LEDS; i > BASE_LED; i--) {
      leds[i] = (255, 255, 255);
      FastLED.delay(10);
      //leds[i].fadeToBlackBy( 64 );
    }
    for (byte i = BASE_LED; i < playerScores[playerTurn]; i++) {
      leds[i].setRGB( playerColors[playerTurn][0], playerColors[playerTurn][1], playerColors[playerTurn][2]);
    }
  }
}

// Dim a color by 25% (64/256ths)
// eventually fading to full black
//


void CountDownLed() {
  //Serial.println("countdownled()");
  long msLeft = rndExpireTime - millis();
  timerPos = map (msLeft, 0, roundLength, BASE_LED, NUM_LEDS);
  leds[timerPos] = CRGB:: White;
  leds[timerPos + 1] = CRGB:: White;
  FastLED.delay(10);
  leds[timerPos].fadeToBlackBy( 64 );

}


void showPlayerScores() {
  //Serial.println("showplayerScores()");
  for (byte i = 1; i <= numOfPlayers; i++) { //starts at i=1 because player scores starts at 1
    int score = playerScores[i];
    byte color[3];
    for (byte x = 0; x < 3; x++) {
      color [x] = playerColors[i][x];
    }
    byte scorePos = map(score, 0, NUM_LEDS - BASE_LED + 1, BASE_LED, NUM_LEDS);
    leds[scorePos] = (color[0], color[1], color[2]);
  }
}

void printInfo() {
  //Serial.println("printInfo() ");

  Serial.print("p1: ");
  Serial.print(playerScores[1]);

  Serial.print(" p2: ");
  Serial.print(playerScores[2]);

  Serial.print(" round: ");
  Serial.print(currentRound);
  Serial.print("/");
  Serial.print(numOfRounds);

  Serial.print(" millis/rndExpireTime(): ");
  Serial.print(millis());
  Serial.print("/");
  Serial.print(rndExpireTime);
  if (millis() > rndExpireTime )
    Serial.print(" true  ");
  else
    Serial.print(" false  ");

  Serial.print(" pTurn: ");
  Serial.print(playerTurn);
  Serial.print(" gameIsLive: ");
  Serial.println(gameIsLive);
}

void timerCheck() {
  //Serial.println("timerCheck()");
  if (millis() > rndExpireTime) {
    gameIsLive = 0;
    if (currentRound < numOfRounds) {
      currentRound++;
      nextPlayer();
    } else gameOver = 1;
  }
}

void addPlayerPoint() {// function call must be nested in a if (gameIsLive) = 1 statement
  //Serial.println("addPlayerPoint");
  if (tagMatch(uid, playerIDs[playerTurn]) && (millis() - lastTag > tagDebounce))
    playerScores[playerTurn]++;
    touchAnimate();
  lastTag = millis();
  //Serial.println(playerScores[playerTurn]);
  //update LED here
}

void nextPlayer() {
  //Serial.println("nextPlayer()");
  playerTurn < numOfPlayers ? playerTurn++ : playerTurn = 1;
}

void assignPlayer(byte slotNum, byte Rfid[]) { //slot 0 reserved for coop game score;
  //Serial.print("assignPlayer()");
  for (byte x = 0; x < 4; x++) {
    playerIDs [slotNum][x] = Rfid[x];
  }
}

void pendingPlayer() {
  //Serial.println("pendingPlayer()");
  for (byte i = BASE_LED; i <= NUM_LEDS; i++) {
    leds[i] = (playerColors[playerTurn][0], playerColors[playerTurn][1], playerColors[playerTurn][2]);
  }
  if (currentRound < numOfRounds && gameIsLive == 0 && tagMatch(uid, playerIDs[playerTurn])) { //5 rounds = 6 turns / 2 ps = 3 rounds of 2 ps
    rndExpireTime = millis() + roundLength;
    gameIsLive = 1;
    Serial.print("Pending player rndExpireTime: ");
    Serial.println(rndExpireTime);

  }
} //end pendingPlayer

bool isTurnTag() {
  //Serial.println("isTUrnTag");
  if (gameIsLive && tagMatch(uid, playerIDs[playerTurn])) {
    return true;
  }
}

bool tagMatch(byte array1[], byte array2[]) {
  //Serial.println("tagMatch()");
  for (byte i = 0; i < 4; i++) {
    if (array1[i] != array2[i]) {
      //Serial.println("no match");
      return false;
      break; // if not equal, then break out of the "for" loop
    }
    if (i == 3) { // if we made it to 4 loops for each of the array positions, then all 4 Tag Serial numbers sections are matching
      //Serial.println("matches");
      return true;
    }
  }
}

void clearUid() {
  //Serial.println("clearUid()");
  success = 0;
  for (byte i = 0; i < 7; i++) {
    uid[i] = 0;
  }
}

void getUid() {
  //Serial.println("getUid()");
  bool success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength); // searches for and populates uid[] and uidLength
}
void printUid() {
  Serial.println("printUid running");
  for (int x = 0; x < uidLength; x++) { //note uidLength = 0 every round of the loop, so uidLength = 0 until the loop where a card is detected
    Serial.print(uid[x]);
    x < uidLength - 1 ? Serial.print(", ") : Serial.println(); //add a comma and space until last digit
  }
}
